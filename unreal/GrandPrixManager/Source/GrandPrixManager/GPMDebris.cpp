// Débris des accrochages : éclats de carbone (cubes instanciés) et ailerons arrachés (morceau du modèle de la voiture).
#include "GPMDebris.h"
#include "GPMCar.h"
#include "GrandPrixManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace GPMDebrisCfg
{
	constexpr int32 MaxBits = 160;
	constexpr int32 MaxPieces = 4;
	constexpr float Gravity = 981.f;   // cm/s²
	const FVector Hidden(0.f, 0.f, -100000.f);
}

AGPMDebris::AGPMDebris()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Shards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Eclats"));
	Shards->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Shards->SetStaticMesh(Cube.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SM(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SM.Succeeded()) ShapeMat = SM.Object;
	Shards->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shards->CastShadow = false;
}

void AGPMDebris::BeginPlay()
{
	Super::BeginPlay();
	// carbone sombre et brillant
	const FLinearColor Carbon(0.02f, 0.02f, 0.025f);
	if (UMaterialInstanceDynamic* Art = GPMArt::Make(TEXT("M_GPM_Inst"), this))
	{
		Art->SetVectorParameterValue(TEXT("Color"), Carbon);
		Art->SetVectorParameterValue(TEXT("Color2"), Carbon);
		Art->SetScalarParameterValue(TEXT("Rough"), 0.3f);
		Shards->SetMaterial(0, Art);
	}
	else if (ShapeMat)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(ShapeMat, this);
		M->SetVectorParameterValue(TEXT("Color"), Carbon);
		Shards->SetMaterial(0, M);
	}
	Bits.SetNum(GPMDebrisCfg::MaxBits);
	for (int32 i = 0; i < GPMDebrisCfg::MaxBits; ++i)
		Shards->AddInstance(FTransform(FRotator::ZeroRotator, GPMDebrisCfg::Hidden, FVector(0.001f)), true);
}

void AGPMDebris::Burst(const AGPMCar* From, int32 Count, bool bRear)
{
	if (!From || Bits.Num() == 0) return;
	const FTransform Tr = From->GetActorTransform();
	const FVector Origin = Tr.TransformPosition(bRear ? FVector(-245.f, 0.f, 80.f) : FVector(330.f, 0.f, 18.f));
	const FVector Fwd = Tr.GetUnitAxis(EAxis::X), Side = Tr.GetUnitAxis(EAxis::Y);
	const float Ground = Tr.GetLocation().Z + 3.f;
	for (int32 n = 0; n < Count; ++n)
	{
		FBit& B = Bits[NextBit];
		NextBit = (NextBit + 1) % Bits.Num();
		B.P = Origin + Side * FMath::FRandRange(-80.f, 80.f);
		B.Vel = Fwd * From->V * 100.f * FMath::FRandRange(0.3f, 0.8f) + Side * FMath::FRandRange(-450.f, 450.f) + FVector(0.f, 0.f, FMath::FRandRange(150.f, 520.f));
		B.Rot = FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f));
		B.Spin = FRotator(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-900.f, 900.f));
		B.Size = FVector(FMath::FRandRange(0.05f, 0.13f), FMath::FRandRange(0.03f, 0.08f), 0.012f);
		B.Life = FMath::FRandRange(3.5f, 6.f);
		B.Ground = Ground;
	}
}

void AGPMDebris::LooseWing(const AGPMCar* From, bool bRear)
{
	if (!From) return;
	TArray<FVector> P, N;
	TArray<FLinearColor> C;
	if (!From->WingPiece(bRear, P, N, C)) return;
	FVector Mid = FVector::ZeroVector;
	for (const FVector& V : P) Mid += V;
	Mid /= float(P.Num());
	for (FVector& V : P) V -= Mid;
	TArray<int32> Tri;
	Tri.SetNum(P.Num());
	for (int32 i = 0; i + 2 < P.Num(); i += 3)
	{
		const bool bFlip = FVector::DotProduct(FVector::CrossProduct(P[i + 1] - P[i], P[i + 2] - P[i]), N[i] + N[i + 1] + N[i + 2]) > 0.f;
		Tri[i] = i; Tri[i + 1] = bFlip ? i + 2 : i + 1; Tri[i + 2] = bFlip ? i + 1 : i + 2;
	}
	const int32 Wi = NextPiece;
	NextPiece = (NextPiece + 1) % GPMDebrisCfg::MaxPieces;
	if (!PieceMeshes.IsValidIndex(Wi))
	{
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
		PM->SetupAttachment(RootComponent);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->RegisterComponent();
		PieceMeshes.Add(PM);
		Pieces.AddDefaulted();
	}
	if (!PieceMeshes.IsValidIndex(Wi) || !Pieces.IsValidIndex(Wi)) return;
	UProceduralMeshComponent* PM = PieceMeshes[Wi];
	PM->ClearAllMeshSections();
	PM->CreateMeshSection_LinearColor(0, P, Tri, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
	if (UMaterialInterface* Mat = From->BodyMaterial()) PM->SetMaterial(0, Mat);
	PM->SetVisibility(true);
	const FTransform Tr = From->GetActorTransform();
	FBit& B = Pieces[Wi];
	B.P = Tr.TransformPosition(Mid);
	B.Rot = Tr.Rotator();
	B.Vel = Tr.GetUnitAxis(EAxis::X) * From->V * 100.f * (bRear ? 0.75f : 0.55f) + Tr.GetUnitAxis(EAxis::Y) * FMath::FRandRange(-300.f, 300.f) + FVector(0.f, 0.f, FMath::FRandRange(250.f, 450.f));
	B.Spin = FRotator(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-500.f, 500.f));
	B.Life = 7.f;
	B.Ground = Tr.GetLocation().Z + 5.f;
	PM->SetWorldLocationAndRotation(B.P, B.Rot);
}

void AGPMDebris::Clear()
{
	for (FBit& B : Bits) B.Life = 0.f;
	for (FBit& B : Pieces) B.Life = 0.f;
	for (UProceduralMeshComponent* PM : PieceMeshes) if (PM) PM->SetVisibility(false);
	bBitsShown = true;   // les éclats sont cachés à la prochaine image
}

void AGPMDebris::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	if (Dt <= 0.f) return;
	// mouvement balistique : rebonds amortis sur la piste, glissade, rotation qui ralentit
	auto Move = [Dt](FBit& B)
	{
		B.Life -= Dt;
		B.Vel.Z -= GPMDebrisCfg::Gravity * Dt;
		B.P += B.Vel * Dt;
		if (B.P.Z < B.Ground)
		{
			B.P.Z = B.Ground;
			B.Vel.Z = FMath::Abs(B.Vel.Z) * 0.32f;
			B.Vel.X *= 0.72f; B.Vel.Y *= 0.72f;
			B.Spin = B.Spin * 0.6f;
		}
		B.Rot += B.Spin * Dt;
	};
	bool bAny = false;
	for (FBit& B : Bits) if (B.Life > 0.f) { Move(B); bAny = true; }
	if (bAny || bBitsShown)
	{
		TArray<FTransform> Xf;
		Xf.Reserve(Bits.Num());
		for (const FBit& B : Bits)
			Xf.Add(B.Life > 0.f ? FTransform(B.Rot, B.P, B.Size * FMath::Min(1.f, B.Life / 0.6f)) : FTransform(FRotator::ZeroRotator, GPMDebrisCfg::Hidden, FVector(0.001f)));
		Shards->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
		bBitsShown = bAny;
	}
	for (int32 i = 0; i < Pieces.Num() && i < PieceMeshes.Num(); ++i)
	{
		FBit& B = Pieces[i];
		if (B.Life <= 0.f) continue;
		Move(B);
		if (UProceduralMeshComponent* PM = PieceMeshes[i])
		{
			if (B.Life <= 0.f) PM->SetVisibility(false);
			else PM->SetWorldLocationAndRotation(B.P, B.Rot);
		}
	}
}
