#include "GPMCar.h"
#include "GrandPrixManager.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "ProceduralMeshComponent.h"
#include "GPMDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GPMGameMode.h"

namespace
{
	constexpr float Grav = 9.81f;
	constexpr float PhysDt = 1.f / 120.f;   // pas fixe de la physique

	// Courbe de pneu : linéaire, sommet arrondi, puis l'adhérence baisse quand le pneu glisse franchement (tyreCurve)
	float TyreCurve(float R)
	{
		if (R <= 0.75f) return R;
		const float S = 0.75f + 0.25f * FMath::Tanh((R - 0.75f) / 0.25f);
		return S * (1.f - 0.15f * FMath::Clamp((R - 1.15f) / 0.8f, 0.f, 1.f));
	}
	float Sign(float X) { return X > 0.f ? 1.f : X < 0.f ? -1.f : 0.f; }
	float WrapPi(float A) { return FMath::Atan2(FMath::Sin(A), FMath::Cos(A)); }
}

AGPMCar::AGPMCar()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Root;
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Carrosserie"));
	Body->SetupAttachment(Root);

	// Modèle provisoire en formes simples (≈ 5,6 m × 2 m), remplacé par un vrai modèle à l'étape 4b/9.
	// Repère local : X vers l'avant, Z vers le haut, en cm.
	AddPart(TEXT("Coque"), TEXT("Cube"), FVector(0, 0, 35), FVector(3.2f, 0.55f, 0.35f), false);
	AddPart(TEXT("Nez"), TEXT("Cube"), FVector(210, 0, 28), FVector(1.4f, 0.28f, 0.2f), false);
	AddPart(TEXT("Pontons"), TEXT("Cube"), FVector(-20, 0, 30), FVector(1.8f, 1.5f, 0.35f), false);
	AddPart(TEXT("Capot"), TEXT("Cube"), FVector(-90, 0, 65), FVector(1.6f, 0.4f, 0.4f), false);
	AddPart(TEXT("AileronAvant"), TEXT("Cube"), FVector(275, 0, 12), FVector(0.35f, 1.9f, 0.05f), true);
	AddPart(TEXT("AileronArriere"), TEXT("Cube"), FVector(-250, 0, 95), FVector(0.35f, 1.0f, 0.08f), true);
	AddPart(TEXT("Casque"), TEXT("Sphere"), FVector(20, 0, 70), FVector(0.28f), true);
	for (int32 k = 0; k < 4; ++k)
	{
		const FVector L(k < 2 ? 175.f : -165.f, k % 2 ? 85.f : -85.f, 33.f);
		UStaticMeshComponent* W = AddPart(*FString::Printf(TEXT("Roue%d"), k), TEXT("Cylinder"), L, FVector(0.66f, 0.66f, k < 2 ? 0.32f : 0.4f), false);
		W->SetRelativeRotation(FRotator(0, 0, 90));
		Wheels.Add(W);
	}

	Model = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Modele"));
	Model->SetupAttachment(Body);
	Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Model->CastShadow = true;   // ombre de la voiture (soleil, 90 m autour de la caméra)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VC(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VC.Succeeded()) VertexColorMat = VC.Object;

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Perche"));
	Arm->SetupAttachment(Root);
	// caméra arrière plus basse (comme à la télé) ; le retard de rotation laisse voir le flanc de la voiture en virage
	Arm->TargetArmLength = 700.f;
	Arm->SocketOffset = FVector(0, 0, 175.f);
	Arm->bDoCollisionTest = false;
	Arm->bEnableCameraRotationLag = true;
	Arm->CameraRotationLagSpeed = 6.f;
	Arm->SetRelativeRotation(FRotator(-4.f, 0, 0));
	ChaseCam = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraArriere"));
	ChaseCam->SetupAttachment(Arm);
	ChaseCam->FieldOfView = 75.f;

	CockpitCam = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraCockpit"));
	CockpitCam->SetupAttachment(Body);
	CockpitCam->SetRelativeLocation(FVector(32, 0, 98));   // yeux du pilote (casque du modèle à X = 30 cm, Z = 84 cm)
	CockpitCam->FieldOfView = 80.f;
	CockpitCam->SetAutoActivate(false);

	// T-cam : sur l'arceau de sécurité (haut de la prise d'air à 1,26 m, 15 cm derrière le casque), comme à la télé
	TCam = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraTCam"));
	TCam->SetupAttachment(Body);
	TCam->SetRelativeLocation(FVector(-18, 0, 152));
	TCam->SetRelativeRotation(FRotator(-6.f, 0, 0));
	TCam->FieldOfView = 82.f;
	TCam->SetAutoActivate(false);

	// feu arrière de pluie : sous l'aileron arrière, au bout de la structure anti-choc
	RainLight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FeuPluie"));
	RainLight->SetupAttachment(Body);
	if (UStaticMesh* Cb = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))) RainLight->SetStaticMesh(Cb);
	RainLight->SetRelativeLocation(FVector(-280.f, 0.f, 46.f));
	RainLight->SetRelativeScale3D(FVector(0.04f, 0.16f, 0.08f));
	RainLight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RainLight->CastShadow = false;
	RainLight->SetVisibility(false);
}

void AGPMCar::SetRainLight(bool bOn)
{
	if (bOn == bRainLightOn) return;
	bRainLightOn = bOn;
	if (bOn && !Cast<UMaterialInstanceDynamic>(RainLight->GetMaterial(0)))
	{
		if (UMaterialInstanceDynamic* M = GPMArt::Make(TEXT("M_GPM_Inst"), this))
		{
			M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.02f, 0.02f));
			M->SetVectorParameterValue(TEXT("Color2"), FLinearColor(0.6f, 0.02f, 0.02f));
			M->SetVectorParameterValue(TEXT("Glow"), FLinearColor(30.f, 0.6f, 0.4f));
			RainLight->SetMaterial(0, M);
		}
		else UE_LOG(LogGPM, Warning, TEXT("Feu de pluie : matériau M_GPM_Inst introuvable"));
	}
	RainLight->SetVisibility(bOn);
}

void AGPMCar::SetNear(bool bNear)
{
	if (bNear == bNearCam) return;
	bNearCam = bNear;
	Model->SetCastShadow(bNear);
}

UStaticMeshComponent* AGPMCar::AddPart(const TCHAR* Name, const TCHAR* Shape, FVector Loc, FVector Scale, bool bAccent)
{
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Body);
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
	if (M) C->SetStaticMesh(M);
	C->SetRelativeLocation(Loc);
	C->SetRelativeScale3D(Scale);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->CastShadow = false;
	(bAccent ? AccentParts : MainParts).Add(C);
	return C;
}

void AGPMCar::BeginPlay()
{
	Super::BeginPlay();
	SetLivery(FLinearColor(0.85f, 0.12f, 0.12f), FLinearColor::White);
}

void AGPMCar::SyncModel(const FString& Era)
{
	ModelWant = Era;
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	if (Data && (Era.IsEmpty() ? Data->CarEra : Era) != ModelEra) SetLivery(LiveryMain, LiveryAccent);
}

void AGPMCar::SetLivery(FLinearColor Main, FLinearColor Accent)
{
	if (bSafetyBody) return;   // coupé de la voiture de sécurité : pas de livrée
	auto Paint = [](const TArray<TObjectPtr<UStaticMeshComponent>>& Parts, FLinearColor Col)
	{
		for (UStaticMeshComponent* P : Parts)
			if (UMaterialInstanceDynamic* M = P->CreateDynamicMaterialInstance(0)) M->SetVectorParameterValue(TEXT("Color"), Col);
	};
	// Vraie monoplace (modèle du jeu web) : couleurs par sommet ; les formes simples restent en secours si car.json manque.
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	LiveryMain = Main; LiveryAccent = Accent;
	HelmetOf = DriverName;
	if (Data) ModelEra = ModelWant.IsEmpty() ? Data->CarEra : ModelWant;
	if (const FGPMCarModel* CM = Data ? Data->GetCarModel(ModelEra) : nullptr)
	{
		CarModel = CM;
		WingBend[0] = WingBend[1] = 0.f; bWingsShaped = false;   // modèle reconstruit : ailerons intacts
		TArray<int32> Tri; Tri.SetNum(CM->P.Num());
		TArray<FLinearColor> Col; Col.SetNum(CM->P.Num());
		// matériau éclairé de Tools/gpm_assets.py : la rugosité de chaque pièce passe par l'alpha du sommet
		// (palette de car.json : livrée, accent, casque, carbone, noir, visière, écran, rétroviseur, feu, gris, jantes)
		UMaterialInstanceDynamic* CarMat = GPMArt::Make(TEXT("M_GPM_Lit"), this);
		// casque : une couleur vive propre à chaque pilote (tirée de son nom), pour reconnaître les deux voitures d'une écurie
		FLinearColor Helmet = Accent;
		if (!DriverName.IsEmpty())
		{
			const uint32 Hh = GetTypeHash(DriverName);
			Helmet = FLinearColor(float(Hh % 360), 0.55f + 0.35f * float((Hh >> 9) % 100) / 100.f, 0.55f + 0.4f * float((Hh >> 17) % 100) / 100.f).HSVToLinearRGB();
		}
		for (int32 i = 0; i < CM->P.Num(); ++i)
		{
			const int32 K = CM->K[i];
			Col[i] = K == 0 ? Main : K == 1 ? Accent : K == 2 ? Helmet : CM->Fixed.IsValidIndex(K) ? CM->Fixed[K] : FLinearColor::Black;
			Col[i].A = CarMat ? PaintRough(K) : 1.f;
		}
		if (CarMat)
		{
			CarMat->SetScalarParameterValue(TEXT("Rough"), 1.f);
			CarMat->SetScalarParameterValue(TEXT("Metal"), 0.2f);   // moins métallique : couleurs de livrée plus franches
			CarMat->SetScalarParameterValue(TEXT("Spec"), 0.6f);
		}
		// sens des triangles selon la normale (Unreal : normale de face = −(B−A)×(C−A))
		for (int32 i = 0; i + 2 < CM->P.Num(); i += 3)
		{
			const FVector& A = CM->P[i]; const FVector& B = CM->P[i + 1]; const FVector& C = CM->P[i + 2];
			const bool bFlip = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), CM->N[i] + CM->N[i + 1] + CM->N[i + 2]) > 0.f;
			Tri[i] = i; Tri[i + 1] = bFlip ? i + 2 : i + 1; Tri[i + 2] = bFlip ? i + 1 : i + 2;
		}
		Model->CreateMeshSection_LinearColor(0, CM->P, Tri, CM->N, TArray<FVector2D>(), Col, TArray<FProcMeshTangent>(), false);
		if (CarMat) Model->SetMaterial(0, CarMat);
		else if (VertexColorMat) Model->SetMaterial(0, VertexColorMat);
		for (UStaticMeshComponent* P : MainParts) P->SetVisibility(false);
		for (UStaticMeshComponent* P : AccentParts) P->SetVisibility(false);
		for (UStaticMeshComponent* W : Wheels) W->SetVisibility(false);
		return;
	}
	Paint(MainParts, Main);
	Paint(AccentParts, Accent);
	for (UStaticMeshComponent* W : Wheels)
		if (UMaterialInstanceDynamic* M = W->CreateDynamicMaterialInstance(0)) M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.02f));
}

// rugosité par couleur de la palette de car.json : livrée, accent, casque, carbone, noir, visière, écran, rétroviseur, feu, gris, jantes
float AGPMCar::PaintRough(int32 K)
{
	// livrée vernie : reflets du ciel plus nets sur la carrosserie (le carbone reste satiné, les pneus mats)
	static const float Rough[11] = { 0.2f, 0.2f, 0.16f, 0.36f, 0.85f, 0.05f, 0.3f, 0.1f, 0.3f, 0.6f, 0.4f };
	return Rough[FMath::Clamp(K, 0, 10)];
}

void AGPMCar::PlaceOnTrack(const FGPMTrackData* InTrack, int32 Slot)
{
	Track = InTrack;
	if (!Track || Track->WX.Num() < 4) return;
	const int32 N = Track->WX.Num();
	// Grille (race.js) : 8 m entre deux rangs, décalage gauche/droite de 3 m, derrière la ligne
	Idx = ((-(5 + Slot * 4)) % N + N) % N;
	const float Side = Slot % 2 ? 3.f : -3.f;
	Lat = Lane = Side;
	LaneBias = (FMath::FRand() - 0.5f) * 1.5f;
	PassTimer = YieldCd = YieldRun = Slipstream = Wake = 0.f;
	X = Track->WX[Idx] - Track->TZ[Idx] * Side;
	Z = Track->WZ[Idx] + Track->TX[Idx] * Side;
	Y = Track->WY[Idx];
	Heading = Track->H[Idx];
	V = Beta = Ax = Ay = SpinT = 0.f;
	Lap = 0; RaceTime = LapStart = LastLap = 0.f;   // le meilleur tour est gardé
	Ground = 0; OffIdx = -1; OffSide = Strikes = 0; OffT = PenCd = 0.f; bOffGravel = false;
	bFinished = false; FinishTime = 0.f; Penalty = 0.f; bSCMode = false;
	PenHold = PenSum = 0.f; PenKind = 0; PenDue = 0; bPenVisit = bDriveThru = false; SlowMul = 1.f;
	bOut = false; OutT = FinProgress = 0.f; FlagLap = -1; OutWhy.Reset();
	PitPhase = 0; Wear = 0.f; Stops = 0; bBox = false;
	// nouvelle séance : voiture réparée, mode normal
	Mode = 1; PowerMul = AccMul = 1.f;
	Dmg = DmgR = DmgNew = DmgNewR = ContactT = 0.f; DmgKd = RwKd = 1.f;
	bFlat = bAdvised = false;
	RepairWings();
	UpdateTransform();
}

int32 AGPMCar::Gear() const
{
	if (V < 0.5f) return 0;
	static const float T[] = { 0.16f, 0.27f, 0.38f, 0.49f, 0.60f, 0.71f, 0.83f };
	const float F = V / (VTop * Perf);
	int32 Gr = 1;
	for (float t : T) if (F > t) ++Gr;
	return Gr;
}

float AGPMCar::Rpm() const
{
	static const float T[] = { 0.f, 0.16f, 0.27f, 0.38f, 0.49f, 0.60f, 0.71f, 0.83f, 1.02f };
	const float F = FMath::Max(0.f, V) / (VTop * Perf);
	const int32 Gr = FMath::Max(1, Gear());
	const float Lo = T[Gr - 1], Hi = T[FMath::Min(Gr, 8)];
	const float Frac = FMath::Clamp((F - Lo) / FMath::Max(0.01f, Hi - Lo), 0.f, 1.f);
	return V < 0.5f ? 4200.f : 6500.f + Frac * 5500.f + (Throttle > 0.f && bTcOn ? 400.f : 0.f);
}

void AGPMCar::ToggleCamera()
{
	SetCamMode((CamMode + 1) % 3);
	// la caméra choisie est gardée pour les courses suivantes
	if (GConfig && bPlayerControlled)
	{
		if (AGPMGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AGPMGameMode>() : nullptr) GM->bSetCockpit = CamMode == 2;
		GConfig->SetBool(TEXT("GPM"), TEXT("Cockpit"), CamMode == 2, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("GPM"), TEXT("CamMode"), CamMode, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void AGPMCar::SetCamMode(int32 NewMode)
{
	CamMode = FMath::Clamp(NewMode, 0, 2);
	ChaseCam->SetActive(CamMode == 0);
	TCam->SetActive(CamMode == 1);
	CockpitCam->SetActive(CamMode == 2);
}

void AGPMCar::ReadKeyboard(float Dt)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	auto Down = [PC](FKey A, FKey B) { return PC->IsInputKeyDown(A) || PC->IsInputKeyDown(B); };
	const bool L = Down(EKeys::Left, EKeys::Q) || PC->IsInputKeyDown(EKeys::A);
	const bool R = Down(EKeys::Right, EKeys::D);
	const float Want = bTouchActive && TouchSteer != 0.f ? TouchSteer : (R ? 1.f : 0.f) - (L ? 1.f : 0.f);
	// les touches tout-ou-rien sont adoucies (comme input.js)
	const float Rate = Want == 0.f ? 7.f : (Sign(Want) != Sign(SteerSmooth) ? 8.f : 3.2f);
	SteerSmooth += FMath::Clamp(Want - SteerSmooth, -Rate * Dt, Rate * Dt);
	Steer = SteerSmooth;
	Throttle = FMath::Max(TouchGas, Down(EKeys::Up, EKeys::Z) || PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f);
	Brake = FMath::Max(TouchBrake, Down(EKeys::Down, EKeys::S) || PC->IsInputKeyDown(EKeys::SpaceBar) ? 1.f : 0.f);
	if (PC->WasInputKeyJustPressed(EKeys::C)) ToggleCamera();
	if (PC->WasInputKeyJustPressed(EKeys::R) && Track)   // remettre sur la piste, à l'arrêt, au milieu
	{
		X = Track->WX[Idx]; Z = Track->WZ[Idx]; Y = Track->WY[Idx]; Heading = Track->H[Idx];
		Lat = 0.f; V = Beta = SpinT = 0.f;
	}
}

void AGPMCar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HelmetOf != DriverName && !bSafetyBody) SetLivery(LiveryMain, LiveryAccent);   // nouveau pilote dans la voiture : son casque
	if (!Track) return;
	if (bTestDrive) TestDrive();
	else if (bPlayerControlled && Cast<APlayerController>(GetController())) ReadKeyboard(DeltaSeconds);
	else DriveAI(DeltaSeconds);
	if (bHold) { Throttle = 0.f; Brake = 1.f; }
	if (bFinished && V > 20.f) Throttle = 0.f;   // tour de décélération
	if (bOut) { Throttle = 0.f; if (V < 3.f) Brake = 1.f; }   // abandon : la voiture s'arrête en roue libre
	// voiture de sécurité : limiteur doux pour le joueur
	if ((bSCMode || SlowMul < 1.f) && bPlayerControlled && Track && Track->AISpeed.IsValidIndex(Idx) && V > Track->AISpeed[Idx] * (bSCMode ? 0.72f : SlowMul)) Throttle = 0.f;
	if (bSafetyBody) TickBeacons(DeltaSeconds);
	Accum = FMath::Min(Accum + DeltaSeconds, 0.1f);
	const int32 N = Track->WX.Num();
	while (Accum >= PhysDt)
	{
		const int32 Prev = Idx;
		if (PitPhase) PitStep(PhysDt); else Step(PhysDt);
		Accum -= PhysDt;
		RaceTime += PhysDt;
		// Ligne d'arrivée = point 0 : passage de la fin du tour au début, dans le bon sens
		if (Prev > N - 40 && Idx < 40 && V > 0)
		{
			if (Lap > 0)
			{
				LastLap = RaceTime - LapStart;
				if (BestLap <= 0.f || LastLap < BestLap) BestLap = LastLap;
			}
			++Lap;
			LapStart = RaceTime;
		}
	}
	UpdateTransform();
	// sensation de vitesse : le champ de vision s'ouvre avec la vitesse (caméra du joueur seulement)
	if (bPlayerControlled && Cast<APlayerController>(GetController()))
	{
		const float K = FMath::Clamp(V / 85.f, 0.f, 1.f);
		ChaseCam->SetFieldOfView(FMath::FInterpTo(ChaseCam->FieldOfView, 70.f + 14.f * K, DeltaSeconds, 2.5f));
		CockpitCam->SetFieldOfView(FMath::FInterpTo(CockpitCam->FieldOfView, 78.f + 10.f * K, DeltaSeconds, 2.5f));
		TCam->SetFieldOfView(FMath::FInterpTo(TCam->FieldOfView, 80.f + 10.f * K, DeltaSeconds, 2.5f));
		// secousses : légères avec la vitesse, fortes sur les vibreurs, très fortes dans l'herbe et le gravier
		ShakeT += DeltaSeconds;
		const float Amp = 0.12f * K + (bKerb && V > 8.f ? 0.9f : 0.f) + ((Ground == 1 || Ground == 2) && V > 5.f ? (Ground == 2 ? 2.2f : 1.4f) : 0.f);
		const FVector Jit(0.f, Amp * 0.4f * FMath::PerlinNoise1D(ShakeT * 19.f), Amp * (FMath::PerlinNoise1D(ShakeT * 31.f + 7.f) + 0.5f * FMath::Sin(ShakeT * 70.f) * (bKerb ? 1.f : 0.f)));
		TCam->SetRelativeLocation(FVector(-18, 0, 152) + Jit);
		CockpitCam->SetRelativeLocation(FVector(32, 0, 98) + Jit);
		ChaseCam->SetRelativeLocation(Jit * 1.5f);
	}
}

// Portage de stepCar() (js/car.js) sans garage, carburant, dégâts ni aspiration (étapes 7-8).
void AGPMCar::Step(float Dt)
{
	const FGPMTrackData& T = *Track;
	const int32 N = T.WX.Num(), I0 = Idx;
	auto W = [N](int32 i) { return ((i % N) + N) % N; };
	const float Half = T.Width / 2.f, ALat = FMath::Abs(Lat);
	bOffTrack = ALat > Half + 1.3f;
	// hors piste : l'herbe ne freine pas mais glisse ; le gravier (2 m après le bord) freine fort ; la voie de dégagement tient bien
	{
		const uint8 Zone = bOffTrack ? T.RunAt(I0, Lat > 0.f) : 0;
		Ground = !bOffTrack ? 0 : Zone == 2 ? 3 : Zone == 1 && ALat > Half + 2.f ? 2 : 1;
	}
	const float GroundGrip = Ground == 1 ? 0.5f : Ground == 2 ? 0.6f : Ground == 3 ? 0.95f : 1.f;

	const float Curv = T.Curvature.IsValidIndex(I0) ? T.Curvature[I0] : 0.f;
	bKerb = FMath::Abs(Curv) > 1.f / 220.f && ALat > Half - 0.1f && ALat < Half + 1.4f;
	const float Bump = FMath::Sin(I0 * 0.73f + 1.3f) * FMath::Sin(I0 * 0.191f) * FMath::Sin(I0 * 0.047f + 0.5f);
	const float Vc = (T.WY[W(I0 - 6)] - 2 * T.WY[I0] + T.WY[W(I0 + 6)]) / 144.f;
	const float Nz = FMath::Clamp(1.f + V * V * Vc / Grav, 0.45f, 1.6f);
	// réglages : suspension ferme et voiture basse sentent les vibreurs et les bosses, une voiture basse et souple talonne
	const FGPMSetupFx& Fx = SetupFx;
	const float Bottoming = 1.f - 0.15f * Fx.Bottom * FMath::Abs(Bump) * FMath::Clamp((V - 55.f) / 25.f, 0.f, 1.f);
	const float Grip = GroundGrip * (bKerb ? 0.9f * Fx.Kerb : 1.f) * (1.f + 0.03f * Fx.Bump * Bump * FMath::Min(1.f, V / 60.f)) * Bottoming;
	const float VTopNow = VTop * Perf * UpVTop * Fx.VTop * PowerMul * (1.f - 0.02f * Dmg) * (bRwOff ? 1.03f : 1.f) * (Ground == 2 ? 0.4f : 1.f) * (1.f + 0.07f * Slipstream);

	// adhérence par essieu : transfert de masse, appui aéro
	const float Lt = FMath::Clamp(-Ax / 40.f, -0.35f, 0.35f);
	const float Aero = Kd * UpKd * (1.f - 0.3f * Wake) * V * V;
	const float MuB = Mu * UpMu * Grip * GripMul / (1.f + (Mass - 1.f) * 0.25f);
	const float RollF = FMath::Min(1.f, FMath::Abs(Ay) / 45.f);
	// aileron abîmé : moins d'appui sur l'essieu concerné (avant → sous-virage, arrière → survirage)
	const float CapF = FMath::Max(0.5f, MuB * Fx.MuF * (Grav * Nz * FMath::Pow(1 + Lt, 0.85f) + Aero * Fx.KdF * DmgKd) * (1 - 0.05f * RollF * RollF));
	const float CapR = FMath::Max(0.5f, MuB * Fx.MuR * (Grav * Nz * FMath::Pow(1 - Lt, 0.85f) + Aero * Fx.KdR * RwKd) * (1 - 0.05f * RollF * RollF));
	// répartition de freinage : trop à l'avant, les roues avant bloquent plus tôt et la voiture tourne moins bien ;
	// trop à l'arrière, elle pivote mieux mais l'arrière se dérobe
	const float BiasD = Fx.Bias - Fx.BiasIdeal;
	const float BiasK = 1.f - (BiasD > 0.f ? 1.6f * BiasD : -1.f * BiasD);

	// longitudinal
	float A = -Sign(V) * (0.8f + 0.0009f * V * V), Spin = 0.f;
	bLock = false; bAbsOn = bTcOn = false;
	// en marche arrière, l'accélérateur repart tout de suite vers l'avant (pas besoin d'attendre l'arrêt)
	if (Throttle > 0 && V < -0.5f) A = 30.f * Throttle;
	else if (Throttle > 0)
	{
		float Ad = Throttle * Acc * Perf * UpAcc * Fx.Acc * AccMul / Mass * (1 - FMath::Pow(FMath::Max(V, 0.f) / VTopNow, 2.2f));
		const float Uy = FMath::Min(0.95f, FMath::Abs(Ay) / CapR);
		// différentiel bloqué : meilleure motricité en sortie de virage lent
		const float Room = FMath::Max(CapR * 0.2f, CapR * 0.65f * Fx.MuLong * (0.92f + 0.16f * Fx.Diff) * FMath::Sqrt(1 - Uy * Uy));
		if (Ad > Room)
		{
			if (Assist.bTC) { Ad = Room * 0.95f; bTcOn = true; }
			else { Spin = FMath::Min(1.f, (Ad - Room) / CapR); Ad = Room * 0.85f + (Ad - Room) * 0.2f; }
		}
		A = Ad;
	}
	if (Ground == 2 && V > 0.f) A -= V * 0.6f + 3.f;   // bac à gravier : la voiture s'enfonce
	if (Brake > 0)
	{
		if (V > 0.3f)
		{
			float Ab = Brake * (Brake0 + BrakeK * V * V) * UpBrake * Fx.BrakeP / FMath::Sqrt(Mass);
			const float Lim = 1.1f * CapF * Fx.MuLong * BiasK, Uy = FMath::Min(0.95f, FMath::Abs(Ay) / CapF);
			const float Room = Lim * FMath::Sqrt(1 - Uy * Uy);
			if (Ab > Room)
			{
				// l'ABS travaille moins bien quand la pression de freinage dépasse de loin l'adhérence
				if (Assist.bABS) { Ab = Room * (0.97f - 0.15f * FMath::Min(1.f, (Ab - Room) / Room)); bAbsOn = true; }
				else { Ab = Lim * 0.78f; bLock = V > 4; }
			}
			A -= Ab;
		}
		else if (Throttle == 0) A = -Brake * 5.f;   // marche arrière lente pour se dégager
	}
	const float Slope = (T.WY[W(I0 + 1)] - T.WY[I0]) / 2.f;
	const float Fwd = FMath::Cos(Heading - T.H[I0]);
	A -= Grav * Slope * Fwd;
	const float V0 = V;
	V += A * Dt;
	V = FMath::Max(V, -6.f);
	if (bHold) V = 0.f;   // sur la grille, freins serrés : ni marche arrière ni glissade dans la pente
	if (FMath::Abs(V) < 0.05f && Throttle == 0 && Brake == 0) V = 0;

	// latéral : la voiture avance selon Course et pointe selon Heading = Course + Beta
	float Course = Heading - Beta, YawO = 0.f, Under = 0.f;
	if (SpinT > 0)
	{
		SpinT -= Dt; Beta += SpinDir * 5.5f * Dt; V *= FMath::Exp(-1.8f * Dt);
		if (SpinT <= 0) { Course += Beta; Beta = 0; }
		Ay = 0;
	}
	else
	{
		const float Delta = SteerMax * UpSteer * Steer / (1 + FMath::Abs(V) / 20.f) * (bLock ? 0.25f : 1.f);
		float Yaw = V * FMath::Tan(Delta) / WB;
		// avant en virage : le différentiel bloqué pousse tout droit à la remise des gaz, la répartition de freinage
		// change le comportement à l'entrée du virage
		const float CapFy = CapF * (1.f - 0.07f * (Fx.Diff - 0.5f) * Throttle * RollF)
			* (1.f + Brake * (BiasD < 0.f ? -1.f * BiasD : -0.8f * BiasD));
		const float Want = FMath::Abs(V * Yaw), RF = Want / (CapFy * 1.06f);
		float Got = Want;
		if (Want > 0.01f) { Got = CapFy * 1.06f * TyreCurve(RF); Under = FMath::Max(0.f, RF - 1); Yaw *= Got / Want; }
		const float AyNow = FMath::Abs(V * Yaw);
		const float RearBrake = 1.f + 12.f * FMath::Max(0.f, -BiasD);
		const float Ux = FMath::Min(Assist.bTC ? 0.8f : 0.97f, FMath::Max(0.f, A) / (CapR * 0.65f) + (Brake > 0 ? 0.35f * RearBrake * Brake * FMath::Min(1.f, -A / (1.1f * CapF)) : 0.f));
		const float CapRl = CapR * 1.06f * (1 - 0.6f * Spin) * FMath::Sqrt(FMath::Max(0.f, 1 - Ux * Ux));
		if (V > 6 && AyNow > CapRl) YawO = Sign(Yaw) * (AyNow - CapRl) / V * (Assist.bStab ? 0.35f : 1.f);
		if (bKerb && V > 25) Beta += (FMath::FRand() - 0.5f) * 0.05f * FMath::Min(1.f, V / 60.f) * FMath::Sqrt(Dt * 60.f) * (Assist.bStab ? 0.5f : 1.f);
		Beta -= YawO * Dt;
		const float Margin = FMath::Max(0.f, CapRl - AyNow) / CapR;
		const float Counter = FMath::Max(0.f, Sign(Beta) * Steer);
		const float Rec = (1.6f + 5 * Margin + 6 * Counter) * (Assist.bStab ? 2.5f : 1.f) * (V < 6 ? 4.f : 1.f);
		Beta -= Beta * FMath::Min(1.f, Rec * Dt);
		Course -= Yaw * Dt;
		if (FMath::Abs(Beta) > 1.15f && V > 12) { SpinT = 1.4f; SpinDir = Sign(Beta); }
		Ay += (V * (Yaw + YawO) - Ay) * FMath::Min(1.f, Dt * 10.f);
		V -= Sign(V) * (FMath::Min(Want - Got, 40.f) * 0.1f * (RF > 1 ? 1.f : 0.f) + FMath::Abs(FMath::Sin(Beta)) * FMath::Abs(V) * 0.8f) * Dt;
	}
	Ax += ((V - V0) / Dt - Ax) * FMath::Min(1.f, Dt * 8.f);
	Slip = FMath::Max(FMath::Max(Under, FMath::Abs(Beta) * 3.f), FMath::Max(bLock ? 0.6f : 0.f, FMath::Max(Spin, SpinT > 0 ? 0.8f : 0.f)));
	Heading = Course + Beta;
	X += FMath::Sin(Course) * V * Dt;
	Z += FMath::Cos(Course) * V * Dt;

	// position sur la piste
	Idx = T.Locate(X, Z, Idx);
	const int32 i = Idx;
	const float Rx = -T.TZ[i], Rz = T.TX[i];
	const float Dx = X - T.WX[i], Dz = Z - T.WZ[i];
	Lat = Dx * Rx + Dz * Rz;
	const float Along = Dx * T.TX[i] + Dz * T.TZ[i];
	const int32 j = Along >= 0 ? W(i + 1) : W(i - 1);
	const float Tt = FMath::Min(1.f, FMath::Abs(Along) / 2.f);
	Y = T.WY[i] + (T.WY[j] - T.WY[i]) * Tt;
	Pitch = -FMath::Atan((T.WY[W(i + 1)] - T.WY[i]) / 2.f) * Fwd;
	Dive = FMath::Clamp(-Ax * 0.0006f, -0.015f, 0.03f);
	Roll = FMath::Clamp(-Ay * 0.00045f, -0.025f, 0.025f);
	YOff = bKerb && V > 8 ? (FMath::FRand() - 0.5f) * 0.05f : Bump * 0.012f * FMath::Min(1.f, V / 60.f);

	// murs
	HitWall = FMath::Max(0.f, HitWall - Dt);
	ContactT = FMath::Max(0.f, ContactT - Dt);
	const TArray<float>& WallArr = Lat > 0 ? T.WallRight : T.WallLeft;
	const float WLim = (WallArr.IsValidIndex(i) ? WallArr[i] : Half + 9.f) - 1.0f;
	if (FMath::Abs(Lat) > WLim)
	{
		const float S = Sign(Lat), Over = FMath::Abs(Lat) - WLim;
		X -= Rx * S * Over; Z -= Rz * S * Over; Lat = S * WLim;
		const float Rel = WrapPi(Course - T.H[i]);
		const float Dir = FMath::Abs(Rel) < PI / 2 ? 0.f : PI;
		const float Impact = FMath::Abs(FMath::Sin(Rel));
		// choc franc à vitesse : aileron avant abîmé (arrière si la voiture arrive de travers)
		if (V > 25.f && HitWall <= 0.f && Impact > 0.25f)
		{
			const float Hurt = Impact * FMath::Min(1.f, V / 60.f) * 0.25f;
			if (FMath::Abs(Beta) > 1.f || SpinT > 0.f) DmgNewR += Hurt; else DmgNew += Hurt;
		}
		V *= 1 - FMath::Min(0.7f, 0.1f + Impact * 0.9f);
		Heading += WrapPi(T.H[i] + Dir - Heading) * 0.5f;
		Beta *= 0.3f;
		HitWall = 0.4f;
	}
}

void AGPMCar::UpdateTransform()
{
	// Jeu web → Unreal : (x, y, z) m → (x, z, y) cm. Cap web h : direction (sin h, cos h) dans (x, z).
	const FVector Loc = FGPMTrackData::ToWorld(X, Y + YOff, Z);
	const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(FMath::Cos(Heading), FMath::Sin(Heading)));
	SetActorLocationAndRotation(Loc, FRotator(FMath::RadiansToDegrees(-Pitch), YawDeg, 0.f));
	// assiette visible : plongée au freinage, roulis en virage
	Body->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(-Dive), 0.f, FMath::RadiansToDegrees(Roll)));
	// roues avant braquées
	const float SteerDeg = FMath::RadiansToDegrees(SteerMax * Steer / (1 + FMath::Abs(V) / 20.f));
	for (int32 k = 0; k < 2 && k < Wheels.Num(); ++k) Wheels[k]->SetRelativeRotation(FRotator(0, SteerDeg, 90));
}
