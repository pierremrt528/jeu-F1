// Pluie en 3D (comme les gerbes du jeu web) : bouffées d'eau derrière les roues arrière et gouttes autour de la caméra.
#include "GPMRain.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/RotationMatrix.h"

namespace
{
	constexpr int32 MaxPuffs = 420;   // bouffées d'eau en même temps (toutes voitures)
	constexpr int32 NumDrops = 520;   // gouttes autour de la caméra
	constexpr int32 NumLens = 48;     // gouttes posées sur l'objectif (vues embarquées)
	const FVector DropBox(2600.f, 2600.f, 1500.f);   // boîte de pluie (cm) centrée devant la caméra
}

AGPMRain::AGPMRain()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Make = [this, Root](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		if (Mesh) C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->NumCustomDataFloats = 1;
		C->SetMobility(EComponentMobility::Movable);
		C->bNeverDistanceCull = true;
		C->SetVisibility(false);
		return C;
	};
	Spray = Make(TEXT("Gerbes"), Sphere.Object);
	Rain = Make(TEXT("Gouttes"), Cube.Object);
	Lens = Make(TEXT("GouttesObjectif"), Sphere.Object);
	Lens->SetTranslucentSortPriority(10);   // devant le reste de la pluie
}

void AGPMRain::Show(bool bOn)
{
	if (bShown == bOn) return;
	bShown = bOn;
	Spray->SetVisibility(bOn);
	Rain->SetVisibility(bOn);
	Lens->SetVisibility(bOn);
	if (!bOn) return;
	// premier affichage : les instances et le matériau d'eau (sans lui, gris simple)
	if (Spray->GetInstanceCount() == 0)
	{
		auto Mat = [this](UInstancedStaticMeshComponent* C, float Opacity, float Bright)
		{
			if (UMaterialInstanceDynamic* M = GPMArt::Make(TEXT("M_GPM_Water"), this))
			{
				M->SetScalarParameterValue(TEXT("Opacity"), Opacity);
				M->SetScalarParameterValue(TEXT("Bright"), Bright);
				C->SetMaterial(0, M);
			}
		};
		Mat(Spray, 0.16f, 0.95f);
		Mat(Rain, 0.9f, 1.1f);
		Mat(Lens, 0.4f, 0.9f);
		TArray<FTransform> Hidden;
		Hidden.Init(FTransform(FRotator::ZeroRotator, FVector(0, 0, -1e6f), FVector(0.01f)), MaxPuffs);
		Spray->AddInstances(Hidden, false);
		Hidden.SetNum(NumDrops);
		Rain->AddInstances(Hidden, false);
		Hidden.SetNum(NumLens);
		Lens->AddInstances(Hidden, false);
		LensDrops.SetNum(NumLens);
		Puffs.SetNum(MaxPuffs);
	}
}

void AGPMRain::Clear()
{
	for (FPuff& P : Puffs) P.Life = 0.f;
	for (FLensDrop& L : LensDrops) L.Life = 0.f;
	Drops.Reset();
	Emit.Reset();
	Show(false);
}

void AGPMRain::UpdateRain(float Dt, const FVector& Cam, const FRotator& CamRot, const TArray<AGPMCar*>& Cars, float Wet, float Fall, USceneComponent* OnboardCam, float Speed)
{
	if (Wet < 0.08f && Fall < 0.02f)
	{
		bool bAlive = false;
		for (const FPuff& P : Puffs) if (P.Age < P.Life) { bAlive = true; break; }
		if (!bAlive) { if (bShown) Clear(); return; }
	}
	Show(true);
	Dt = FMath::Min(Dt, 0.05f);
	const FVector CamFwd = CamRot.Vector();

	// --- gerbes : chaque voiture rapide sur piste mouillée soulève de l'eau derrière ses roues arrière ---
	const float WetK = FMath::Clamp((Wet - 0.08f) / 0.6f, 0.f, 1.f);
	for (const AGPMCar* C : Cars)
	{
		if (!C || C->IsHidden() || WetK <= 0.f) continue;
		const FVector Pos = C->GetActorLocation();
		if (FVector::DistSquared(Pos, Cam) > FMath::Square(25000.f)) continue;   // au-delà de 250 m : invisible
		const float V = C->V;   // m/s
		if (V < 12.f) continue;
		float& Acc = Emit.FindOrAdd(C);
		Acc += Dt * WetK * FMath::Clamp((V - 12.f) / 60.f, 0.f, 1.f) * 34.f;   // jusqu'à ~34 bouffées par seconde
		const FVector Fwd = C->GetActorForwardVector(), Right = C->GetActorRightVector();
		while (Acc >= 1.f)
		{
			Acc -= 1.f;
			FPuff& P = Puffs[NextPuff];
			NextPuff = (NextPuff + 1) % MaxPuffs;
			const float Side = FMath::FRand() < 0.5f ? -85.f : 85.f;
			P.P = Pos - Fwd * (190.f + FMath::FRand() * 60.f) + Right * (Side + FMath::FRandRange(-25.f, 25.f)) + FVector(0, 0, 35.f);
			// l'eau part moins vite que la voiture : elle reste derrière, monte et s'étale
			P.Vel = Fwd * (V * 100.f * FMath::FRandRange(0.35f, 0.55f)) + Right * (Side * FMath::FRandRange(0.6f, 1.6f)) + FVector(0, 0, FMath::FRandRange(120.f, 260.f));
			P.Age = 0.f;
			P.Life = FMath::FRandRange(0.7f, 1.2f);
			P.Size = FMath::FRandRange(0.6f, 0.9f) * (0.7f + 0.5f * WetK);
			P.Alpha = (0.55f + 0.45f * WetK) * (C->bPlayerControlled ? 0.45f : 1.f);   // ta propre gerbe ne cache pas la piste
		}
	}
	TArray<FTransform> T;
	T.SetNumUninitialized(MaxPuffs);
	for (int32 i = 0; i < MaxPuffs; ++i)
	{
		FPuff& P = Puffs[i];
		float A = 0.f;
		if (P.Age < P.Life)
		{
			P.Age += Dt;
			P.Vel *= FMath::Max(0.f, 1.f - 2.2f * Dt);   // l'air freine l'eau
			P.Vel.Z -= 300.f * Dt;
			P.P += P.Vel * Dt;
			const float K = FMath::Clamp(P.Age / P.Life, 0.f, 1.f);
			A = P.Alpha * FMath::Min(1.f, K * 6.f) * (1.f - K) * (1.f - K);
			const float S = P.Size * (1.f + 4.f * K);
			A *= FMath::Clamp((FVector::Dist(P.P, Cam) - 250.f) / 900.f, 0.f, 1.f);   // pas de nuage collé à l'objectif
			T[i] = FTransform(FRotator::ZeroRotator, P.P, FVector(S * 1.6f, S, S * 0.75f));
		}
		else T[i] = FTransform(FRotator::ZeroRotator, FVector(0, 0, -1e6f), FVector(0.01f));
		Spray->SetCustomDataValue(i, 0, A, false);
	}
	Spray->BatchUpdateInstancesTransforms(0, T, true, true, true);

	// --- gouttes : boîte qui suit la caméra, chaque goutte revient en haut quand elle sort ---
	const float FallA = FMath::Clamp(Fall, 0.f, 1.f);
	const FVector Centre = Cam + FVector(CamFwd.X, CamFwd.Y, 0.f).GetSafeNormal() * 900.f;
	if (Drops.Num() != NumDrops)
	{
		Drops.SetNum(NumDrops);
		for (FVector& D : Drops) D = Centre + FVector(FMath::FRandRange(-1.f, 1.f) * DropBox.X, FMath::FRandRange(-1.f, 1.f) * DropBox.Y, FMath::FRandRange(-0.5f, 1.f) * DropBox.Z);
	}
	const FVector Wind(150.f, 80.f, -1100.f);   // 11 m/s vers le bas, un peu de vent
	TArray<FTransform> R;
	R.SetNumUninitialized(NumDrops);
	const FRotator Tilt = Wind.GetSafeNormal().Rotation() + FRotator(90.f, 0.f, 0.f);
	for (int32 i = 0; i < NumDrops; ++i)
	{
		FVector& D = Drops[i];
		D += Wind * Dt;
		// la caméra avance vite : on replie la goutte dans la boîte autour d'elle
		FVector L = D - Centre;
		L.X = FMath::Fmod(L.X + DropBox.X * 3.f, DropBox.X * 2.f) - DropBox.X;
		L.Y = FMath::Fmod(L.Y + DropBox.Y * 3.f, DropBox.Y * 2.f) - DropBox.Y;
		if (L.Z < -DropBox.Z * 0.5f) L.Z += DropBox.Z * 1.5f;
		else if (L.Z > DropBox.Z) L.Z -= DropBox.Z * 1.5f;
		D = Centre + L;
		const bool bOn = (i % 100) < FMath::RoundToInt(FallA * 100.f);
		R[i] = bOn ? FTransform(Tilt, D, FVector(0.012f, 0.012f, 0.55f)) : FTransform(FRotator::ZeroRotator, FVector(0, 0, -1e6f), FVector(0.01f));
		Rain->SetCustomDataValue(i, 0, bOn ? 0.22f : 0.f, false);
	}
	Rain->BatchUpdateInstancesTransforms(0, R, true, true, true);

	// --- gouttes sur l'objectif : en vue embarquée seulement ; à l'arrêt elles restent, en roulant elles filent vers le haut et les bords ---
	// accrochées à la caméra embarquée (sinon elles traînent d'une image derrière l'objectif à pleine vitesse)
	const bool bOnboard = OnboardCam != nullptr;
	if (bOnboard && Lens->GetAttachParent() != OnboardCam) Lens->AttachToComponent(OnboardCam, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	if (bOnboard && FallA > 0.05f)
	{
		LensAcc += Dt * FallA * (5.f + Speed * 0.12f);
		while (LensAcc >= 1.f)
		{
			LensAcc -= 1.f;
			for (FLensDrop& L : LensDrops)
				if (L.Age >= L.Life)
				{
					L.P = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f));
					L.Size = FMath::FRandRange(0.35f, 1.f);
					L.Age = 0.f;
					L.Life = FMath::FRandRange(0.8f, 2.4f);
					break;
				}
		}
	}
	TArray<FTransform> LT;
	LT.SetNumUninitialized(NumLens);
	const float Slide = Speed / 60.f;   // m/s → vitesse de glisse sur l'écran
	for (int32 i = 0; i < NumLens; ++i)
	{
		FLensDrop& L = LensDrops[i];
		float A = 0.f;
		if (!bOnboard) L.Age = L.Life;
		if (bOnboard && L.Age < L.Life)
		{
			L.Age += Dt;
			// l'air pousse la goutte du centre vers les bords et vers le haut
			L.P += FVector2D(L.P.X, 0.6f + 0.4f * FMath::Abs(L.P.X)) * Slide * Dt * 0.5f;
			const float K = L.Age / L.Life;
			A = FMath::Min(1.f, K * 8.f) * (1.f - K * K);
			if (FMath::Abs(L.P.X) > 1.1f || FMath::Abs(L.P.Y) > 1.1f) L.Age = L.Life;
			// à 22 cm devant l'objectif : environ ±21 cm de large, ±10 cm de haut ; plus étirée quand elle glisse
			const FVector At(22.f, L.P.X * 21.f, L.P.Y * 10.f);   // repère de la caméra
			const float S = 0.008f * L.Size;
			LT[i] = FTransform(FRotator::ZeroRotator, At, FVector(S * 0.3f, S, S * (1.f + 1.5f * FMath::Min(1.f, Slide))));
		}
		else LT[i] = FTransform(FRotator::ZeroRotator, FVector(0, 0, -1e5f), FVector(0.01f));
		Lens->SetCustomDataValue(i, 0, A, false);
	}
	Lens->BatchUpdateInstancesTransforms(0, LT, false, true, true);
	LastCam = Cam;
}
