#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GPMTypes.h"
#include "GPMSetup.h"
#include "GPMCar.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UProceduralMeshComponent;

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
	/** Drapeau rouge : retour sur la grille (Slot) en gardant la course (tour NewLap), pneus neufs NewTyre et voiture réparée. */
	void Regrid(int32 Slot, int32 NewLap, TCHAR NewTyre);
	float Pursuit(float InLane) const;

	/** Couleurs de l'écurie (provisoire : modèle en formes simples). */
	UFUNCTION(BlueprintCallable, Category = "GPM") void SetLivery(FLinearColor Main, FLinearColor Accent);
	/** Monoplace de l'ère voulue (vide = ère en cours du règlement) : reconstruit le modèle s'il a changé. */
	void SyncModel(const FString& Era = FString());
	FString ModelEra, ModelWant;

	/** Commandes (−1..1 pour la direction, 0..1 pour les pédales). Remplies au clavier ici, par le tactile à l'étape 5, par l'IA à l'étape 6. */
	UPROPERTY(BlueprintReadWrite, Category = "GPM") float Steer = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "GPM") float Throttle = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "GPM") float Brake = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPM") FGPMAssist Assist;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPM") float Perf = 0.975f;   // performance de la voiture (TEAMS.perf)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPM") bool bPlayerControlled = true;

	UFUNCTION(BlueprintPure, Category = "GPM") float SpeedKmh() const { return V * 3.6f; }
	UFUNCTION(BlueprintPure, Category = "GPM") int32 Gear() const;
	/** Régime moteur (tr/min) d'après la vitesse et le rapport, pour le son et le compte-tours. */
	float Rpm() const;
	/** Caméra suivante : arrière → T-cam (au-dessus de la tête du pilote) → cockpit → arrière. */
	UFUNCTION(BlueprintCallable, Category = "GPM") void ToggleCamera();
	/** 0 arrière, 1 T-cam, 2 cockpit. */
	void SetCamMode(int32 NewMode);
	int32 GetCamMode() const { return CamMode; }
	bool IsCockpit() const { return CamMode == 2; }
	/** Caméra embarquée active (T-cam ou cockpit), sinon nullptr : les gouttes sur l'objectif s'y accrochent. */
	USceneComponent* OnboardCam() const { return CamMode == 1 ? (USceneComponent*)TCam.Get() : CamMode == 2 ? (USceneComponent*)CockpitCam.Get() : nullptr; }
	/** Nom de la caméra pour le bouton du HUD : « ARRIÈRE », « T-CAM », « COCKPIT ». */
	/** Feu arrière rouge des jours de pluie. */
	void SetRainLight(bool bOn);
	/** Voiture loin de la caméra : pas d'ombre (moins de travail pour la carte graphique). */
	void SetNear(bool bNear);
	FString CamLabel() const { return CamMode == 1 ? TEXT("T-CAM") : CamMode == 2 ? TEXT("COCKPIT") : TEXT("ARRIÈRE"); }

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
	uint8 Ground = 0;                      // sous la voiture : 0 piste, 1 herbe (glissante), 2 bac à gravier (freine fort), 3 voie de dégagement
	// limites de piste (commissaires, GPMPenalty.cpp) : sortie en cours, avertissements, délai entre deux pénalités
	int32 OffIdx = -1, OffSide = 0, Strikes = 0;
	float OffT = 0.f, PenCd = 0.f;
	bool bOffGravel = false;

	/** Pilote IA (js/ai.js) : poursuite pure sur la trajectoire, profil de vitesse × niveau, dépassements, consigne d'équipe. */
	void DriveAI(float Dt);
	/** Passe la voiture en IA (pas de caméra ni de lecture du clavier). */
	void MakeAI(float InSkill);
	FString DriverName, TeamName;
	FLinearColor TeamColor = FLinearColor::White;
	bool bFinished = false;                // drapeau à damier passé
	float FinishTime = 0.f;
	float FinProgress = 0.f;               // distance couverte au drapeau à damier (ou à l'abandon), pour le classement
	int32 FlagLap = -1;                    // tour en cours quand le vainqueur a passé la ligne : arrivée au passage suivant
	bool bOut = false;                     // abandon (casse moteur, accident)
	float OutT = 0.f;                      // secondes depuis l'abandon
	FString OutWhy;                        // « moteur », « accident »
	/** Classée à l'arrivée : drapeau passé, ou temps calculé quand la course s'arrête juste après l'arrivée du joueur. */
	bool Classified() const { return bFinished || (!bOut && FinProgress > 0.f); }
	float Skill = 1.f;                     // niveau du pilote × difficulté
	float Lane = 0.f, LaneBias = 0.f, PassTimer = 0.f, PassSide = 1.f, YieldCd = 0.f, YieldRun = 0.f;
	bool bYielding = false;
	// IA : défense de la corde, erreurs de pilotage, temps de réaction au départ, drapeau bleu
	float DefendCd = 0.f, DefendT = 0.f, DefendLane = 0.f, MistakeT = 0.f, MistakeSide = 0.f, LaunchT = 0.f;
	bool bHoldPrev = false, bBlue = false, bMistake = false;
	/** Options des pilotes IA (réglages du jeu) : agressivité 0 prudent, 1 normal, 2 agressif ; erreurs de pilotage. */
	static int32 AIAggro;
	static bool bAIErrors;
	static void LoadAIOptions();
	static void SetAIOptions(int32 InAggro, bool bInErrors);
	/** Réglages de la voiture (écran de réglages) appliqués à la physique. */
	FGPMSetupFx SetupFx;
	AGPMCar* YieldTo = nullptr;           // statut N°2 : laisse passer cette voiture (le joueur)
	TArray<AGPMCar*> Field;                // toutes les voitures de la course (rempli par le mode de jeu)
	float Slipstream = 0.f, Wake = 0.f;    // aspiration (vitesse de pointe) et air sale (appui), calculés par le mode de jeu
	bool bHold = false;                    // avant le départ : freins serrés
	/** Pièces de l'usine (carUpgrades) : multiplicateurs de la voiture du joueur. */
	float UpVTop = 1.f, UpAcc = 1.f, UpMu = 1.f, UpBrake = 1.f, UpKd = 1.f, UpSteer = 1.f, UpFuel = 1.f;
	float UpTyre = 1.f, UpPit = 0.f;   // usure des pneus (multiplicateur), secondes gagnées à l'arrêt (négatif)
	bool bParked = false;                  // garée hors de la séance (qualifications)
	bool bSCMode = false;                  // voiture de sécurité en piste : file indienne
	float Penalty = 0.f;                   // secondes ajoutées au temps de course
	// pénalités à purger (GPMPenalty.cpp) : secondes à faire au prochain arrêt (sinon ajoutées au temps), drive-through (1)
	// ou stop-and-go de 10 s (2) à purger avant la fin du tour PenDue (sinon drapeau noir) ; PenSum : total infligé
	float PenHold = 0.f, PenSum = 0.f;
	uint8 PenKind = 0;
	int32 PenDue = 0;
	bool bPenVisit = false, bDriveThru = false;   // passage au stand pour la pénalité : pas de travail des mécaniciens
	float SlowMul = 1.f;                   // voiture de sécurité virtuelle : vitesse plafonnée (part du profil)
	/** Voiture de sécurité (GPMSafety.cpp) : coupé argent à gyrophares au lieu de la monoplace. */
	void MakeSafetyBody();
	void SetBeacons(bool bOn) { bBeacons = bOn; }
	bool bSafetyBody = false;

	/** Pneus et carburant (strategy.js) : gomme S/M/H, usure 0 (neuf) → 1 (à plat), masse du carburant. */
	TCHAR Tyre = 'M';
	float Wear = 0.f, Fuel = 0.f, GripMul = 1.f, Mass = 1.f;
	int32 Stops = 0, TeamIdx = 0;
	bool bBox = false;                     // rentrer au stand au prochain passage
	TArray<TPair<TCHAR, int32>> Plan;      // stratégie d'une IA : gomme et tours de chaque relais
	TCHAR NextTyre = 'H';
	/** Voie des stands : 0 = en piste, 1 = entrée, 2 = arrêt, 3 = sortie ; PitKf = position (indice signé). */
	int32 PitPhase = 0;
	float PitKf = 0.f, PitStopT = 0.f;
	void StartPit(float StopTime);
	/** Joueur : il prend lui-même la voie des stands et doit s'arrêter dans le carré de son écurie (pitManual de strategy.js). */
	void StartManualPit();
	bool InSlot() const;
	bool bPitManual = false, bPitMissed = false, bPitWarned = false;
	bool bTestDrive = false;               // tests (GPM.TestStand) : conduite automatique jusqu'au carré
	void TestDrive();
	float PitO = 0.f, PitHd = 0.f;
	int32 PitMsg = 0;                      // message du muret à dire : 1 arrêt dans le carré, 2 repartir, 3 carré dépassé, 4 ressorti sans arrêt, 5 excès de vitesse à l'entrée
	static float TyreGrip(TCHAR T);
	static float TyreWpl(TCHAR T);
	static float LifeGrip(float R);
	float Progress() const { return Lap * (Track ? Track->WX.Num() : 1) + Idx; }

	/** Modes de pilotage (MODES de strategy.js) : 0 attaque, 1 normal, 2 économie. */
	int32 Mode = 1;
	float PowerMul = 1.f, AccMul = 1.f;    // vitesse de pointe et accélération (mode, crevaison)
	static float ModeV(int32 InMode) { return InMode == 0 ? 1.012f : InMode == 2 ? 0.985f : 1.f; }
	static float ModeA(int32 InMode) { return InMode == 0 ? 1.04f : InMode == 2 ? 0.94f : 1.f; }
	static float ModeFuel(int32 InMode) { return InMode == 0 ? 1.15f : InMode == 2 ? 0.8f : 1.f; }
	static float ModeWear(int32 InMode) { return InMode == 0 ? 1.3f : InMode == 2 ? 0.75f : 1.f; }
	static FString ModeName(int32 InMode) { return InMode == 0 ? TEXT("attaque") : InMode == 2 ? TEXT("économie") : TEXT("normal"); }
	/**
	 * Adhérence d'une gomme selon l'humidité de la piste (tyreGrip de strategy.js, hors usure) : slicks au sec, intermédiaires
	 * sur piste humide (meilleurs que les slicks vers 25 % d'eau, que les pneus pluie jusque vers 55 %), pneus pluie sur le détrempé.
	 */
	static float WetGrip(TCHAR T, float Wet)
	{
		return T == 'W' ? TyreGrip(T) - 0.04f * Wet : T == 'I' ? TyreGrip(T) - 0.05f * Wet - 0.3f * FMath::Max(0.f, Wet - 0.45f) : TyreGrip(T) * (1.f - 0.45f * Wet);
	}
	/** Usure en plus des pneus pluie et intermédiaires quand la piste sèche (ils surchauffent). */
	static float WetWear(TCHAR T, float Wet)
	{
		return T == 'W' ? 1.f + 2.4f * (1.f - Wet) : T == 'I' ? 1.f + 1.8f * FMath::Clamp((0.3f - Wet) / 0.3f, 0.f, 1.f) : 1.f;
	}

	/** Dégâts (addDamage de strategy.js) : aileron avant → sous-virage, arrière → survirage ; réparés au stand. */
	float Dmg = 0.f, DmgR = 0.f;           // 0 intact … 1 détruit
	float DmgNew = 0.f, DmgNewR = 0.f;     // chocs de l'image, appliqués par le mode de jeu
	float DmgKd = 1.f, RwKd = 1.f;         // part de l'appui restant à l'avant et à l'arrière
	float ContactT = 0.f;                   // pas de nouveaux dégâts pendant un court instant après un contact
	bool bFwOff = false, bRwOff = false;   // aileron arraché
	bool bFlat = false;                    // crevaison (pneu à 0 %)
	bool bAdvised = false;                 // conseil du muret déjà donné ce tour-ci
	float WetCall = 0.25f;                 // humidité de la piste à partir de laquelle ce pilote quitte les slicks
	/** Aileron tordu d'un côté (−1 / 1), arraché, ou voiture réparée (debris.js). */
	void BendWing(bool bRear, float Side);
	void DetachWing(bool bRear);
	void RepairWings();
	/** Morceau d'aileron (repère de la voiture, cm) avec ses couleurs, pour le débris qui s'envole. */
	bool WingPiece(bool bRear, TArray<FVector>& OutP, TArray<FVector>& OutN, TArray<FLinearColor>& OutC) const;
	UMaterialInterface* BodyMaterial() const;
	/** Rugosité de chaque couleur de la palette du modèle (passée par l'alpha des sommets). */
	static float PaintRough(int32 K);

	// Constantes (SPEC du jeu web) : vtop ≈ 335 km/h
	static constexpr float VTop = 93.f, Acc = 12.5f, Brake0 = 17.f, BrakeK = 0.0032f, Mu = 1.65f, Kd = 0.0034f, WB = 3.6f, SteerMax = 0.34f;

protected:
	virtual void BeginPlay() override;

private:
	void ReadKeyboard(float Dt);
	void Step(float Dt);
	void PitStep(float Dt);
	void PitManual(float Dt);
	void PlaceInLane(float Hd);
	void PitService();
	void UpdateTransform();
	UStaticMeshComponent* AddPart(const TCHAR* Name, const TCHAR* Shape, FVector Loc, FVector Scale, bool bAccent);

	const FGPMTrackData* Track = nullptr;
	const FGPMCarModel* CarModel = nullptr;   // modèle partagé (positions d'origine pour tordre / réparer les ailerons)
	float WingBend[2] = { 0.f, 0.f };
	bool bWingsShaped = false;
	FLinearColor LiveryMain = FLinearColor::Red, LiveryAccent = FLinearColor::White;
	FString HelmetOf;   // pilote dont le casque est peint (couleur propre à chaque pilote, d'après son nom)
	void ShapeWings();
	float SteerSmooth = 0.f, Accum = 0.f;
	int32 CamMode = 0;
	bool bRainLightOn = false, bNearCam = true;
	float ShakeT = 0.f;
	bool bBeacons = false;
	float BeaconT = 0.f;
	void TickBeacons(float Dt);
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RainLight;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Model;   // monoplace du jeu web (car.json)
	UPROPERTY() TObjectPtr<UMaterialInterface> VertexColorMat;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Arm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ChaseCam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> CockpitCam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> TCam;   // T-cam : sur l'arceau, au-dessus du casque
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MainParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> AccentParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
};
