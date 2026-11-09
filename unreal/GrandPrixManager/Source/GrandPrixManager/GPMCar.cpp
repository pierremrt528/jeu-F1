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

	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Perche"));
	Arm->SetupAttachment(Root);
	Arm->TargetArmLength = 750.f;
	Arm->SocketOffset = FVector(0, 0, 220.f);
	Arm->bDoCollisionTest = false;
	Arm->bEnableCameraRotationLag = true;
	Arm->CameraRotationLagSpeed = 8.f;
	Arm->SetRelativeRotation(FRotator(-6.f, 0, 0));
	ChaseCam = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraArriere"));
	ChaseCam->SetupAttachment(Arm);
	ChaseCam->FieldOfView = 75.f;

	CockpitCam = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraCockpit"));
	CockpitCam->SetupAttachment(Body);
	CockpitCam->SetRelativeLocation(FVector(10, 0, 92));
	CockpitCam->FieldOfView = 80.f;
	CockpitCam->SetAutoActivate(false);
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

void AGPMCar::SetLivery(FLinearColor Main, FLinearColor Accent)
{
	auto Paint = [](const TArray<TObjectPtr<UStaticMeshComponent>>& Parts, FLinearColor Col)
	{
		for (UStaticMeshComponent* P : Parts)
			if (UMaterialInstanceDynamic* M = P->CreateDynamicMaterialInstance(0)) M->SetVectorParameterValue(TEXT("Color"), Col);
	};
	Paint(MainParts, Main);
	Paint(AccentParts, Accent);
	for (UStaticMeshComponent* W : Wheels)
		if (UMaterialInstanceDynamic* M = W->CreateDynamicMaterialInstance(0)) M->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.02f));
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
	bFinished = false; FinishTime = 0.f;
	PitPhase = 0; Wear = 0.f; Stops = 0; bBox = false;
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

void AGPMCar::ToggleCamera()
{
	bCockpit = !bCockpit;
	ChaseCam->SetActive(!bCockpit);
	CockpitCam->SetActive(bCockpit);
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
	if (!Track) return;
	if (bPlayerControlled && Cast<APlayerController>(GetController())) ReadKeyboard(DeltaSeconds);
	else DriveAI(DeltaSeconds);
	if (bHold) { Throttle = 0.f; Brake = 1.f; }
	if (bFinished && V > 20.f) Throttle = 0.f;   // tour de décélération
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
}

// Portage de stepCar() (js/car.js) sans garage, carburant, dégâts ni aspiration (étapes 7-8).
void AGPMCar::Step(float Dt)
{
	const FGPMTrackData& T = *Track;
	const int32 N = T.WX.Num(), I0 = Idx;
	auto W = [N](int32 i) { return ((i % N) + N) % N; };
	const float Half = T.Width / 2.f, ALat = FMath::Abs(Lat);
	bOffTrack = ALat > Half + 1.3f;

	const float Curv = T.Curvature.IsValidIndex(I0) ? T.Curvature[I0] : 0.f;
	bKerb = FMath::Abs(Curv) > 1.f / 220.f && ALat > Half - 0.1f && ALat < Half + 1.4f;
	const float Bump = FMath::Sin(I0 * 0.73f + 1.3f) * FMath::Sin(I0 * 0.191f) * FMath::Sin(I0 * 0.047f + 0.5f);
	const float Vc = (T.WY[W(I0 - 6)] - 2 * T.WY[I0] + T.WY[W(I0 + 6)]) / 144.f;
	const float Nz = FMath::Clamp(1.f + V * V * Vc / Grav, 0.45f, 1.6f);
	const float Grip = (bOffTrack ? 0.55f : 1.f) * (bKerb ? 0.9f : 1.f) * (1.f + 0.03f * Bump * FMath::Min(1.f, V / 60.f));
	const float VTopNow = VTop * Perf * (bOffTrack ? 0.5f : 1.f) * (1.f + 0.07f * Slipstream);

	// adhérence par essieu : transfert de masse, appui aéro
	const float Lt = FMath::Clamp(-Ax / 40.f, -0.35f, 0.35f);
	const float Aero = Kd * (1.f - 0.3f * Wake) * V * V;
	const float MuB = Mu * Grip * GripMul / (1.f + (Mass - 1.f) * 0.25f);
	const float RollF = FMath::Min(1.f, FMath::Abs(Ay) / 45.f);
	const float CapF = FMath::Max(0.5f, MuB * (Grav * Nz * FMath::Pow(1 + Lt, 0.85f) + Aero) * (1 - 0.05f * RollF * RollF));
	const float CapR = FMath::Max(0.5f, MuB * (Grav * Nz * FMath::Pow(1 - Lt, 0.85f) + Aero) * (1 - 0.05f * RollF * RollF));

	// longitudinal
	float A = -Sign(V) * (0.8f + 0.0009f * V * V), Spin = 0.f;
	bLock = false; bAbsOn = bTcOn = false;
	if (Throttle > 0 && V >= -0.5f)
	{
		float Ad = Throttle * Acc * Perf / Mass * (1 - FMath::Pow(FMath::Max(V, 0.f) / VTopNow, 2.2f));
		const float Uy = FMath::Min(0.95f, FMath::Abs(Ay) / CapR);
		const float Room = FMath::Max(CapR * 0.2f, CapR * 0.65f * FMath::Sqrt(1 - Uy * Uy));
		if (Ad > Room)
		{
			if (Assist.bTC) { Ad = Room * 0.95f; bTcOn = true; }
			else { Spin = FMath::Min(1.f, (Ad - Room) / CapR); Ad = Room * 0.85f + (Ad - Room) * 0.2f; }
		}
		A = Ad;
	}
	if (bOffTrack) A -= V * 0.35f;
	if (Brake > 0)
	{
		if (V > 0.3f)
		{
			float Ab = Brake * (Brake0 + BrakeK * V * V) / FMath::Sqrt(Mass);
			const float Lim = 1.1f * CapF, Uy = FMath::Min(0.95f, FMath::Abs(Ay) / CapF);
			const float Room = Lim * FMath::Sqrt(1 - Uy * Uy);
			if (Ab > Room)
			{
				if (Assist.bABS) { Ab = Room * 0.97f; bAbsOn = true; }
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
		const float Delta = SteerMax * Steer / (1 + FMath::Abs(V) / 20.f) * (bLock ? 0.25f : 1.f);
		float Yaw = V * FMath::Tan(Delta) / WB;
		const float Want = FMath::Abs(V * Yaw), RF = Want / (CapF * 1.06f);
		float Got = Want;
		if (Want > 0.01f) { Got = CapF * 1.06f * TyreCurve(RF); Under = FMath::Max(0.f, RF - 1); Yaw *= Got / Want; }
		const float AyNow = FMath::Abs(V * Yaw);
		const float Ux = FMath::Min(Assist.bTC ? 0.8f : 0.97f, FMath::Max(0.f, A) / (CapR * 0.65f) + (Brake > 0 ? 0.35f * Brake * FMath::Min(1.f, -A / (1.1f * CapF)) : 0.f));
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
	const TArray<float>& WallArr = Lat > 0 ? T.WallRight : T.WallLeft;
	const float WLim = (WallArr.IsValidIndex(i) ? WallArr[i] : Half + 9.f) - 1.0f;
	if (FMath::Abs(Lat) > WLim)
	{
		const float S = Sign(Lat), Over = FMath::Abs(Lat) - WLim;
		X -= Rx * S * Over; Z -= Rz * S * Over; Lat = S * WLim;
		const float Rel = WrapPi(Course - T.H[i]);
		const float Dir = FMath::Abs(Rel) < PI / 2 ? 0.f : PI;
		const float Impact = FMath::Abs(FMath::Sin(Rel));
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
