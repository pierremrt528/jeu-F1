#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GPMTypes.h"
#include "GPMCar.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;

/** Aides au pilotage (car.assist du jeu web). */
USTRUCT(BlueprintType)
struct FGPMAssist
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bABS = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTC = true;    // antipatinage
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bStab = true;  // contrôle de stabilité
};

/**
 * Monoplace. Physique = portage direct de stepCar() de js/car.js (modèle semi-réaliste relatif à la piste :
 * pneus progressifs, adhérence par essieu avec transfert de masse, appui aéro, dérive/tête-à-queue, ABS/TC, vibreurs, murs).
 * Calculée dans le repère du jeu web (mètres), puis convertie pour l'affichage. Pas de Chaos Vehicle : même comportement
 * que la version web, plus léger sur mobile, et pas besoin d'un squelette animé pour les roues.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMCar : public APawn
{
	GENERATED_BODY()

public:
	AGPMCar();
	virtual void Tick(float DeltaSeconds) override;

	/** Place la voiture sur la grille (Slot 0 = pole) du circuit donné. */
	void PlaceOnTrack(const FGPMTrackData* InTrack, int32 Slot = 0);
	float Pursuit(float InLane) const;

	/** Couleurs de l'écurie (provisoire : modèle en formes simples). */
	UFUNCTION(BlueprintCallable, Category = "GPM") void SetLivery(FLinearColor Main, FLinearColor Accent);

	/** Commandes (−1..1 pour la direction, 0..1 pour les pédales). Remplies au clavier ici, par le tactile à l'étape 5, par l'IA à l'étape 6. */
	UPROPERTY(BlueprintReadWrite, Category = "GPM") float Steer = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "GPM") float Throttle = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "GPM") float Brake = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPM") FGPMAssist Assist;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPM") float Perf = 0.975f;   // performance de la voiture (TEAMS.perf)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPM") bool bPlayerControlled = true;

	UFUNCTION(BlueprintPure, Category = "GPM") float SpeedKmh() const { return V * 3.6f; }
	UFUNCTION(BlueprintPure, Category = "GPM") int32 Gear() const;
	UFUNCTION(BlueprintCallable, Category = "GPM") void ToggleCamera();

	/** Commandes tactiles (remplies par AGPMHUD) : prioritaires sur le clavier quand l'écran est touché. */
	float TouchSteer = 0.f, TouchGas = 0.f, TouchBrake = 0.f;
	bool bTouchActive = false;

	/** Tours et chronos */
	int32 Lap = 0;
	float RaceTime = 0.f, LapStart = 0.f, LastLap = 0.f, BestLap = 0.f;
	float LapTime() const { return Lap > 0 ? RaceTime - LapStart : 0.f; }
	const FGPMTrackData* GetTrack() const { return Track; }

	// État (mêmes noms que le jeu web)
	float X = 0, Y = 0, Z = 0, Heading = 0, Pitch = 0, V = 0, Lat = 0, Beta = 0, Ax = 0, Ay = 0, SpinT = 0, SpinDir = 1;
	float Dive = 0, Roll = 0, YOff = 0, Slip = 0, HitWall = 0;
	int32 Idx = 0;
	bool bOffTrack = false, bKerb = false, bLock = false, bAbsOn = false, bTcOn = false;

	/** Pilote IA (js/ai.js) : poursuite pure sur la trajectoire, profil de vitesse × niveau, dépassements, consigne d'équipe. */
	void DriveAI(float Dt);
	/** Passe la voiture en IA (pas de caméra ni de lecture du clavier). */
	void MakeAI(float InSkill);
	FString DriverName, TeamName;
	FLinearColor TeamColor = FLinearColor::White;
	bool bFinished = false;                // drapeau à damier passé
	float FinishTime = 0.f;
	float Skill = 1.f;                     // niveau du pilote × difficulté
	float Lane = 0.f, LaneBias = 0.f, PassTimer = 0.f, PassSide = 1.f, YieldCd = 0.f, YieldRun = 0.f;
	bool bYielding = false;
	AGPMCar* YieldTo = nullptr;            // statut N°2 : laisse passer cette voiture (le joueur)
	TArray<AGPMCar*> Field;                // toutes les voitures de la course (rempli par le mode de jeu)
	float Slipstream = 0.f, Wake = 0.f;    // aspiration (vitesse de pointe) et air sale (appui), calculés par le mode de jeu
	bool bHold = false;                    // avant le départ : freins serrés

	/** Pneus et carburant (strategy.js) : gomme S/M/H, usure 0 (neuf) → 1 (à plat), masse du carburant. */
	TCHAR Tyre = 'M';
	float Wear = 0.f, Fuel = 0.f, GripMul = 1.f, Mass = 1.f;
	int32 Stops = 0, TeamIdx = 0;
	bool bBox = false;                     // rentrer au stand au prochain passage
	TCHAR NextTyre = 'H';
	/** Voie des stands : 0 = en piste, 1 = entrée, 2 = arrêt, 3 = sortie ; PitKf = position (indice signé). */
	int32 PitPhase = 0;
	float PitKf = 0.f, PitStopT = 0.f;
	void StartPit(float StopTime);
	static float TyreGrip(TCHAR T);
	static float TyreWpl(TCHAR T);
	static float LifeGrip(float R);
	float Progress() const { return Lap * (Track ? Track->WX.Num() : 1) + Idx; }

	// Constantes (SPEC du jeu web) : vtop ≈ 335 km/h
	static constexpr float VTop = 93.f, Acc = 12.5f, Brake0 = 17.f, BrakeK = 0.0032f, Mu = 1.65f, Kd = 0.0034f, WB = 3.6f, SteerMax = 0.34f;

protected:
	virtual void BeginPlay() override;

private:
	void ReadKeyboard(float Dt);
	void Step(float Dt);
	void PitStep(float Dt);
	void UpdateTransform();
	UStaticMeshComponent* AddPart(const TCHAR* Name, const TCHAR* Shape, FVector Loc, FVector Scale, bool bAccent);

	const FGPMTrackData* Track = nullptr;
	float SteerSmooth = 0.f, Accum = 0.f;
	bool bCockpit = false;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ChaseCam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> CockpitCam;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MainParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> AccentParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
};
