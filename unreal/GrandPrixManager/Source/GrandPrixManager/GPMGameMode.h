#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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
	enum class EPhase : uint8 { Quali, QualiDone, Lights, Race, Done };
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

private:
	UPROPERTY() TObjectPtr<AGPMTrack> Track;
	UPROPERTY() TObjectPtr<AGPMCar> Car;               // le joueur
	UPROPERTY() TArray<TObjectPtr<AGPMCar>> Cars;      // les 20 voitures
	TArray<AGPMCar*> Order;
	void SetupWorld();
	void PlaceCar();
	void Aspiration(float Dt);
	void Collide();
	void Strategy(float Dt);
	void EndQuali();
	void Park(AGPMCar* C, bool bPark);
	const FGPMTrackData* CurTrack() const;
	void UpdateSafety(float Dt);
	UPROPERTY() TObjectPtr<AGPMCar> SC;
	float SCT = 0.f, SCEnding = 0.f;
	bool bSCUsed = false;
	TSet<AGPMCar*> SCAhead;
	TCHAR PickTyre(int32 LapsLeft) const;
	bool bTyreWarn = false;
};
