#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GPMTypes.h"
#include "GPMGameMode.generated.h"

class AGPMTrack;
class AGPMCar;
class APlayerController;

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
	enum class EPhase : uint8 { Quali, QualiDone, Lights, Race, Done, Season, Factory, Home, Setup, Solo, Settings, Press, Mercato, Tuning, Prerace, Fired, Intro, Outro };
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
	/** Recommencer la séance en cours (même grille, même stratégie). */
	void Restart();
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
	int32 SoloWeather = 0;          // course solo : 0 au hasard, 1 sec, 2 pluie, 3 averse
	bool bSoloQuali = true, bNegoFromMercato = false, bChampShown = false;
	TArray<FString> LastLines;      // bilan de la course (direction, sponsors)
	// réglages (sauvegardés dans GameUserSettings.ini)
	bool bSetABS = true, bSetTC = true, bSetStab = true, bSetCockpit = false;
	void SaveSettings();
	// --- Week-end comme le jeu web : réglages → qualifs (jouées ou simulées) → stratégie → course (ou simulée) ---
	struct FSetup { const TCHAR* Name; const TCHAR* Desc; float Kd, VTop; };
	static const FSetup Setups[5];
	/** Une stratégie : gomme et nombre de tours de chaque relais, temps estimé. */
	struct FPlan { TArray<TPair<TCHAR, int32>> Stints; float Time = 0.f; float MaxWear = 0.f; FString Name, Desc; };
	int32 SetupK = -1;                       // réglage choisi pour ce week-end (parc fermé) ; -1 = pas encore
	bool bGridReady = false;                 // qualifications faites pour ce Grand Prix
	bool bLongRace = false;                  // format Long (~22 min) au lieu de Sprint (~11 min)
	TArray<TPair<TCHAR, int32>> Stints;      // stratégie du joueur pour la course
	TArray<FPlan> Plans;                     // 3 stratégies proposées
	void OpenTuning();
	void PlayQuali(int32 K);
	void SimQuali(int32 K);
	void OpenPrerace();
	void Go(bool bSim);
	void SimGrandPrix();
	int32 LapsFor() const;
	float SetupLap(int32 K) const;
	int32 BestSetup() const;
	float BaseLap() const;
	float PlanTime(const TArray<TPair<TCHAR, int32>>& InStints, float LapTime, float* OutMaxWear = nullptr) const;
	TArray<FPlan> MakePlans() const;
	const FGPMTrackData* Circuit() const { return CurTrack(); }
	// --- Course complète (strategy.js) : météo, radio du muret, secteurs ---
	/** Météo du week-end (forecast) : pluie au départ, changement en cours de course, piste qui mouille puis sèche. */
	bool bRainStart = false;                 // il pleut au départ
	float WxChangeAt = -1.f;                 // part de la course où la météo change (−1 : jamais)
	bool bWxDone = false, bWxWarned = false;
	bool bRaining = false;
	float TrackWet = 0.f;                    // piste : 0 sèche … 1 détrempée
	float RainShow = 0.f;                    // pluie à l'écran (suit bRaining en douceur)
	void Forecast();
	FString WeatherLabel() const;            // « Sec », « Pluie », « Averse attendue », « Éclaircie attendue »
	/** Tour plus lent sur le mouillé (qualifications et courses simulées). */
	float WetLapK() const { return 1.f + 0.1f * TrackWet; }
	float RaceWetK() const;
	/** Radio du muret (3 boutons, touches 1 2 3 ou I M N) : 0 point complet, 1 rythme, 2 pneus du prochain arrêt. */
	void RadioCall(int32 K);
	TCHAR NextStopTyre(const AGPMCar* C) const;
	TCHAR ManualTyre = 0;                    // pneus demandés à la radio pour un arrêt hors stratégie
	bool bTyrePicked = false;                // le joueur a choisi lui-même les pneus du prochain arrêt
	/** Qualifications : secteurs S1 S2 S3 (0 = pas encore), couleur (1 violet, 2 vert, 3 jaune), records perso du week-end. */
	float QSec[3] = { 0.f, 0.f, 0.f };
	int32 QCol[3] = { 0, 0, 0 };
	float QPB[3] = { 0.f, 0.f, 0.f };
	float QPole() const;                     // meilleur temps des IA (la pole à battre)
	bool QDelta(float& Out) const;           // écart en direct avec la pole sur le tour en cours
	/** Touche chiffre 1 à 5, aussi sur un clavier AZERTY (& é " ' () et sur le pavé numérique. */
	static bool DigitPressed(const APlayerController* PC, int32 D);
	/** Images par seconde (moyenne glissante) et résolution adaptative en %. */
	float Fps = 60.f;
	int32 ScreenPct = 100;

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
	int32 Pending = 0;   // après le chargement d'un circuit : 0 = qualifs (test), 1 = accueil, 2 = QG (bienvenue), 3 = grille en fond, 4 = réglages, 5 = stratégie
	bool bBootMenu = false;
	TSet<FString> IntroSeen;
	void StageGrid();
	void RefreshRoster();
	void LoadSettings();
	void ApplySettings();
	void UpdateCine(float Dt);
	void UpdatePerf(float Dt);
	float FrameAvg = 1.f / 60.f, PerfT = 0.f, PerfLogT = 0.f;
	UPROPERTY() TObjectPtr<class ACameraActor> CineCam;
	bool bTyreWarn = false;
	// captures d'écran automatiques (Saved/Captures, une par écran et par partie) pour régler l'interface et les graphismes
	void AutoCapture(float Dt);
	TSet<FString> Captured;
	EPhase CapPhase = EPhase::Lights;
	float CapT = 0.f;
	void BaseUps();
	void ApplySetup();
	void SimRace();
	bool bNoIntro = false;
	// météo, dégâts, radio, secteurs
	void ResetWeather();
	void UpdateWeather(float Dt);
	void UpdateSky(float Dt);
	void AddDamage(AGPMCar* C, float Amount, bool bRear);
	bool WantBox(const AGPMCar* C) const;
	FString RadioBrief() const;
	void UpdateSectors();
	int32 SectorColour(int32 K) const;
	float QSplit[2] = { 0.f, 0.f };
	float QPB0[3] = { 0.f, 0.f, 0.f };
	TArray<float> QCum;                      // temps de référence cumulé le long du tour (profil de vitesse)
	float SkyGloom = 0.f, SkyGloomSet = -1.f, SkyCaptured = 0.f, WetSet = -1.f;
	float FogBase = 0.004f;
	FLinearColor FogTint = FLinearColor(0.36f, 0.42f, 0.48f);
	UPROPERTY() TObjectPtr<class USkyAtmosphereComponent> Atmos;
	UPROPERTY() TObjectPtr<class AGPMDebris> Debris;
};
