#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GPMTypes.h"
#include "GPMGameMode.generated.h"

class AGPMTrack;
class AGPMCar;

/**
 * Mode de jeu de base. Étape 3 : charge un circuit (option ?circuit=<id>, comme le jeu web, ou commande GPM.Circuit <id>)
 * place la lumière et les 20 monoplaces sur la grille (19 IA, js/race.js), aspiration, contacts et classement en direct.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGPMGameMode();
	virtual void BeginPlay() override;

	/** Remplace le circuit affiché. */
	UFUNCTION(BlueprintCallable, Category = "GPM") void LoadCircuit(const FString& Id);
	virtual void Tick(float DeltaSeconds) override;

	/** Place (1 = en tête) d'une voiture au classement en direct. */
	int32 PositionOf(const AGPMCar* C) const { return Order.IndexOfByKey(C) + 1; }
	int32 NumCars() const { return Order.Num(); }
	/** Départ (race.js) : feux rouges allumés un par un, extinction au bout d'un délai aléatoire. */
	enum class EPhase : uint8 { Quali, QualiDone, Lights, Race, Done, Season, Factory, Home, Setup, Solo, Settings, Press, Mercato, Fired, Intro, Outro };
	/** Écrans de gestion (la course est figée derrière, caméra de survol). */
	bool InMenu() const { return Phase >= EPhase::Season && Phase <= EPhase::Fired; }
	EPhase Phase = EPhase::Lights;
	float LightsT = 0.f, GoAt = 5.f, Clock = 0.f, DoneT = 0.f;
	int32 LightsOn() const { return Phase == EPhase::Lights ? FMath::Clamp(FMath::FloorToInt(LightsT / 0.8f), 0, 5) : 0; }
	int32 TotalLaps = 5;
	float LapEstimate = 90.f;
	const TArray<AGPMCar*>& Standings() const { return Order; }
	/** Temps (ou estimation) à l'arrivée : arrivés au temps réel, les autres d'après leur retard. */
	float ResultTime(const AGPMCar* C) const;
	/** Recommencer la course (Entrée, ou toucher l'écran des résultats). */
	void Restart() { PlaceCar(); }
	/** Après les résultats : écran de saison (carrière) ou nouvelle course ; depuis l'écran de saison : GP suivant. */
	void Continue();
	bool bCareer = true;
	int32 LastPts = 0;
	float LastPrize = 0.f;
	/** Qualifications : temps de chaque pilote (le joueur : son meilleur tour) et ordre de la grille. */
	TMap<AGPMCar*, float> QTimes;
	TArray<AGPMCar*> QOrder;
	void StartQuali();
	void StartRace();
	static float LapTimeFor(const FGPMTrackData* T, float Skill, float Perf);
	/** Stand : demander / annuler l'arrêt du joueur (touche B ou bouton STAND). */
	void ToggleBox();
	/** Voiture de sécurité (safety.js) : après un gros accident (ou GPM.SC / ?sc), file indienne, pénalité de 5 s si le joueur dépasse. */
	void RequestSafety(bool bForce);
	bool SafetyOut() const { return SC != nullptr && SCEnding <= 0.f; }
	/** Message radio du muret affiché quelques secondes. */
	FString Radio; float RadioT = 0.f;
	void Say(const FString& Msg) { Radio = Msg; RadioT = 5.f; }
	static FString TyreName(TCHAR T) { return T == 'S' ? TEXT("tendres") : T == 'H' ? TEXT("durs") : T == 'W' ? TEXT("pluie") : TEXT("mediums"); }
	static constexpr int32 Points[10] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };

	// --- Menus (étape 15) ---
	/** Accueil : Carrière / Course solo / Réglages. */
	void GoHome();
	void NewCareer();
	void FinishSetup();
	void ContinueCareer();
	void StartSolo();
	void OpenMercato();
	/** Fin des fenêtres à choix (presse, flash info, négociation). */
	void EndDialogs();
	int32 SetupStep = 0, SoloIdx = 0, SoloLaps = 5, MercatoPage = 0;
	bool bSoloQuali = true, bNegoFromMercato = false, bChampShown = false;
	TArray<FString> LastLines;      // bilan de la course (direction, sponsors)
	// réglages (sauvegardés dans GameUserSettings.ini)
	bool bSetABS = true, bSetTC = true, bSetStab = true, bSetCockpit = false;
	void SaveSettings();

	// --- Cinématiques (étape 16, cine.js) : seule la caméra bouge ---
	enum class ECine : uint8 { None, Menu, Intro, Podium, Welcome };
	ECine Cine = ECine::None;
	float CineT = 0.f;
	FString CineTitle, CineSub;
	void StartCine(ECine Mode);
	void EndCine();
	/** Plan en cours de la cinématique (0, 1, 2) et part écoulée de la cinématique. */
	int32 CineShot() const;

private:
	UPROPERTY() TObjectPtr<AGPMTrack> Track;
	UPROPERTY() TObjectPtr<AGPMCar> Car;               // le joueur
	UPROPERTY() TArray<TObjectPtr<AGPMCar>> Cars;      // les 20 voitures
	TArray<AGPMCar*> Order;
	void SetupWorld();
	void ApplyTheme(const FGPMTrackData* T);
	UPROPERTY() TObjectPtr<class ADirectionalLight> Sun;
	UPROPERTY() TObjectPtr<class ASkyLight> SkyLight;
	UPROPERTY() TObjectPtr<class AExponentialHeightFog> Fog;
	UPROPERTY() TObjectPtr<class AGPMScenery> Scenery;
	UPROPERTY() TObjectPtr<class UGPMEngineSound> EngineSound;
	void PlaceCar();
	void Aspiration(float Dt);
	void Collide();
	void Strategy(float Dt);
	void EndQuali();
	void RecordResult();
	void ApplyCareer();
	bool bRecorded = false;
	void Park(AGPMCar* C, bool bPark);
	const FGPMTrackData* CurTrack() const;
	void UpdateSafety(float Dt);
	UPROPERTY() TObjectPtr<AGPMCar> SC;
	float SCT = 0.f, SCEnding = 0.f;
	bool bSCUsed = false;
	TSet<AGPMCar*> SCAhead;
	TCHAR PickTyre(int32 LapsLeft) const;
	int32 GridSlot = 9;
	int32 Pending = 0;   // après le chargement d'un circuit : 0 = week-end, 1 = accueil, 2 = QG (bienvenue)
	bool bBootMenu = false;
	TSet<FString> IntroSeen;
	void StageGrid();
	void RefreshRoster();
	void LoadSettings();
	void ApplySettings();
	void UpdateCine(float Dt);
	UPROPERTY() TObjectPtr<class ACameraActor> CineCam;
	bool bTyreWarn = false;
};
