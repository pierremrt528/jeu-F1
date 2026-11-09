#include "GPMScenery.h"
#include "GrandPrixManager.h"
#include "GPMTypes.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GrandPrixManager.h"

namespace
{
	// bruit déterministe 0..1 (variations de couleur et de taille sans tirage aléatoire différent à chaque partie)
	float Noise(float X, float Y) { return FMath::Frac(FMath::Sin(X * 12.9898f + Y * 78.233f) * 43758.547f); }
	int64 CellKey(int32 CX, int32 CY) { return (int64(CX) << 32) ^ int64(uint32(CY)); }
	constexpr float HashCell = 5000.f;   // 50 m
	FLinearColor HueColor(float Hue, float Sat, float Val) { return FLinearColor::MakeFromHSV8(uint8(Hue * 255), uint8(Sat * 255), uint8(Val * 255)); }
}

AGPMScenery::AGPMScenery()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	Terrain->SetupAttachment(RootComponent);
	Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Terrain->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SM(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SM.Succeeded()) ShapeMat = SM.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VC(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VC.Succeeded()) VertexMat = VC.Object;
}

UHierarchicalInstancedStaticMeshComponent* AGPMScenery::AddLayer(const TCHAR* Shape, FLinearColor Color, FLinearColor Color2, float Rough, bool bWindows)
{
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	H->SetupAttachment(RootComponent);
	H->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
	H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	H->CastShadow = false;
	H->SetCullDistances(0, 220000);   // 2,2 km (le brouillard cache le reste)
	if (UMaterialInstanceDynamic* Art = GPMArt::Make(TEXT("M_GPM_Inst"), this))
	{
		Art->SetVectorParameterValue(TEXT("Color"), Color);
		Art->SetVectorParameterValue(TEXT("Color2"), Color2.A > 0.f ? Color2 : Color);
		Art->SetScalarParameterValue(TEXT("Rough"), Rough);
		Art->SetScalarParameterValue(TEXT("Windows"), bWindows ? 1.f : 0.f);
		H->SetMaterial(0, Art);
		H->SetNumCustomDataFloats(2);
	}
	else if (ShapeMat)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(ShapeMat, this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		H->SetMaterial(0, M);
	}
	H->RegisterComponent();
	Instances.Add(H);
	return H;
}

void AGPMScenery::Build(const FGPMTrackData* InTrack)
{
	for (UHierarchicalInstancedStaticMeshComponent* H : Instances) if (H) H->DestroyComponent();
	Instances.Reset();
	Hash.Reset();
	Track = InTrack;
	if (!Track || Track->Points.Num() < 4) return;
	// grille de recherche : un point de piste sur 4 (8 m)
	BaseZ = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Track->Points.Num(); ++i)
	{
		const FVector& P = Track->Points[i];
		BaseZ = FMath::Min(BaseZ, float(P.Z));
		if (i % 4 == 0) Hash.FindOrAdd(CellKey(FMath::FloorToInt(P.X / HashCell), FMath::FloorToInt(P.Y / HashCell))).Add(i);
	}
	BaseZ -= 60.f;
	BuildTerrain();
	BuildMountains();
	BuildTrees();
	BuildCity();
	BuildStands();
	TintInstances();
	UE_LOG(LogGPM, Log, TEXT("Décor : terrain %d × %d, %d calques instanciés"), GX, GY, Instances.Num());
}

void AGPMScenery::TintInstances()
{
	for (int32 L = 0; L < Instances.Num(); ++L)
	{
		UHierarchicalInstancedStaticMeshComponent* H = Instances[L];
		if (!H || H->NumCustomDataFloats < 2) continue;
		const int32 Num = H->GetInstanceCount();
		for (int32 k = 0; k < Num; ++k)
		{
			H->SetCustomDataValue(k, 0, Noise(k * 0.61f + L, 3.7f), false);
			H->SetCustomDataValue(k, 1, Noise(L * 1.3f, k * 0.27f + 1.f), false);
		}
		H->MarkRenderStateDirty();
	}
}

float AGPMScenery::NearTrack(float X, float Y, float* OutAlt, int32* OutIdx) const
{
	const int32 CX = FMath::FloorToInt(X / HashCell), CY = FMath::FloorToInt(Y / HashCell);
	float Best = 1e12f; int32 BestI = -1;
	for (int32 dx = -1; dx <= 1; ++dx)
		for (int32 dy = -1; dy <= 1; ++dy)
			if (const TArray<int32>* L = Hash.Find(CellKey(CX + dx, CY + dy)))
				for (int32 i : *L)
				{
					const float D = FVector2D::DistSquared(FVector2D(X, Y), FVector2D(Track->Points[i]));
					if (D < Best) { Best = D; BestI = i; }
				}
	if (OutAlt) *OutAlt = BestI >= 0 ? float(Track->Points[BestI].Z) : BaseZ;
	if (OutIdx) *OutIdx = BestI;
	return BestI >= 0 ? FMath::Sqrt(Best) / 100.f : 1e6f;
}

float AGPMScenery::GroundZ(float X, float Y) const
{
	if (GX < 2 || GY < 2) return BaseZ;
	const float FX = FMath::Clamp((X - Origin.X) / Cell, 0.f, GX - 1.001f), FY = FMath::Clamp((Y - Origin.Y) / Cell, 0.f, GY - 1.001f);
	const int32 IX = FMath::FloorToInt(FX), IY = FMath::FloorToInt(FY);
	const float TX = FX - IX, TY = FY - IY;
	auto Hgt = [this](int32 I, int32 J) { return Heights[J * GX + I]; };
	return FMath::Lerp(FMath::Lerp(Hgt(IX, IY), Hgt(IX + 1, IY), TX), FMath::Lerp(Hgt(IX, IY + 1), Hgt(IX + 1, IY + 1), TX), TY);
}

// Relief : moyenne pondérée des altitudes de la piste proche, jamais au-dessus de la piste près de l'asphalte,
// qui redescend en douceur loin du circuit.
void AGPMScenery::BuildTerrain()
{
	const FBox B(Track->Points);
	const float Margin = 80000.f;   // 800 m
	const FVector2D Size(B.GetSize().X + 2 * Margin, B.GetSize().Y + 2 * Margin);
	Cell = FMath::Max(2000.f, FMath::Sqrt(Size.X * Size.Y / 60000.f));
	GX = FMath::CeilToInt(Size.X / Cell) + 1; GY = FMath::CeilToInt(Size.Y / Cell) + 1;
	Origin = FVector2D(B.Min.X - Margin, B.Min.Y - Margin);
	Heights.SetNum(GX * GY);
	const float Near = (Track->Width / 2.f + 14.f) * 100.f;
	for (int32 j = 0; j < GY; ++j)
		for (int32 i = 0; i < GX; ++i)
		{
			const float X = Origin.X + i * Cell, Y = Origin.Y + j * Cell;
			const int32 CX = FMath::FloorToInt(X / HashCell), CY = FMath::FloorToInt(Y / HashCell);
			double SW = 0, SA = 0; float Cap = 1e12f, DMin = 1e12f;
			for (int32 dx = -3; dx <= 3; ++dx)
				for (int32 dy = -3; dy <= 3; ++dy)
					if (const TArray<int32>* L = Hash.Find(CellKey(CX + dx, CY + dy)))
						for (int32 k : *L)
						{
							const FVector& P = Track->Points[k];
							const float D = FVector2D::Distance(FVector2D(X, Y), FVector2D(P));
							const double W = 1.0 / (double(D) * D / 1e4 + 400.0);   // distance en m
							SW += W; SA += W * P.Z;
							DMin = FMath::Min(DMin, D);
							if (D < Near) Cap = FMath::Min(Cap, float(P.Z) - 40.f);
						}
			float Hh = SW > 0 ? float(SA / SW) - 60.f : BaseZ;
			Hh = FMath::Lerp(Hh, BaseZ, FMath::Clamp((DMin - 6000.f) / 30000.f, 0.f, 1.f));
			Heights[j * GX + i] = FMath::Min(Hh, Cap);
		}

	const FGPMTheme& Th = Track->Theme;
	// sol photo (herbe ou sable, comme les bas-côtés) : les sommets ne gardent que les nuances
	UMaterialInstanceDynamic* TerrainArt = GPMArt::Surface(Th.Surface == TEXT("sand") ? FString(TEXT("sand")) : FString(TEXT("grass")), 30.f, this);
	const FLinearColor Base = TerrainArt ? FLinearColor(0.85f, 0.85f, 0.85f) : Th.Surface == TEXT("sand") ? FLinearColor(0.42f, 0.32f, 0.17f)
		: Th.Surface == TEXT("concrete") ? FLinearColor(0.16f, 0.2f, 0.1f) : FLinearColor(0.14f, 0.3f, 0.08f);
	TArray<FVector> V, N; TArray<FLinearColor> Col; TArray<int32> I;
	V.Reserve(GX * GY); N.Reserve(GX * GY); Col.Reserve(GX * GY);
	for (int32 j = 0; j < GY; ++j)
		for (int32 i = 0; i < GX; ++i)
		{
			const float X = Origin.X + i * Cell, Y = Origin.Y + j * Cell;
			auto Hc = [&](int32 a, int32 b) { return Heights[FMath::Clamp(b, 0, GY - 1) * GX + FMath::Clamp(a, 0, GX - 1)]; };
			V.Add(FVector(X, Y, Hc(i, j)));
			N.Add(FVector(Hc(i - 1, j) - Hc(i + 1, j), Hc(i, j - 1) - Hc(i, j + 1), 2.f * Cell).GetSafeNormal());
			const float Nz = Noise(i * 0.37f, j * 0.53f), Big = Noise(FMath::FloorToFloat(i / 6.f), FMath::FloorToFloat(j / 6.f));
			Col.Add(Base * (0.82f + 0.22f * Nz + 0.16f * Big));
		}
	for (int32 j = 0; j + 1 < GY; ++j)
		for (int32 i = 0; i + 1 < GX; ++i)
		{
			const int32 A = j * GX + i, Bq = A + 1, C = A + GX, D = C + 1;
			I.Append({ A, Bq, C, Bq, D, C });
		}
	// faces vers le haut (normale de face Unreal = −(B−A)×(C−A))
	for (int32 k = 0; k + 2 < I.Num(); k += 3)
		if (FVector::CrossProduct(V[I[k + 1]] - V[I[k]], V[I[k + 2]] - V[I[k]]).Z > 0.f) Swap(I[k + 1], I[k + 2]);
	Terrain->CreateMeshSection_LinearColor(0, V, I, N, TArray<FVector2D>(), Col, TArray<FProcMeshTangent>(), false);
	if (TerrainArt) Terrain->SetMaterial(0, TerrainArt);
	else if (VertexMat) Terrain->SetMaterial(0, VertexMat);

	// mer : grand plan bleu juste au-dessus du terrain le plus bas (le relief loin de la piste passe dessous)
	if (Th.bSea)
	{
		UHierarchicalInstancedStaticMeshComponent* Sea = AddLayer(TEXT("Plane"), FLinearColor(0.015f, 0.07f, 0.13f), FLinearColor(0.02f, 0.09f, 0.15f), 0.06f);
		Sea->SetCullDistances(0, 0);
		const FVector C = B.GetCenter();
		Sea->AddInstance(FTransform(FRotator::ZeroRotator, FVector(C.X, C.Y, BaseZ + 25.f), FVector(Size.X / 100.f * 3.f, Size.Y / 100.f * 3.f, 1.f)));
	}
}

void AGPMScenery::BuildMountains()
{
	const FGPMTheme& Th = Track->Theme;
	const FBox B(Track->Points);
	const FVector C = B.GetCenter();
	const float R = B.GetExtent().Size2D();
	UHierarchicalInstancedStaticMeshComponent* Far = AddLayer(TEXT("Cone"), Th.MountFar, Th.MountFar * 0.8f, 1.f);
	UHierarchicalInstancedStaticMeshComponent* NearL = AddLayer(TEXT("Cone"), Th.MountNear, Th.MountNear * 1.25f, 1.f);
	Far->SetCullDistances(0, 0); NearL->SetCullDistances(0, 0);
	for (int32 k = 0; k < 34; ++k)
	{
		const bool bNear = k % 2 == 1;
		const float A = k / 34.f * 2.f * PI + Noise(k, 1.f) * 0.3f;
		const float Dist = R + (bNear ? 50000.f + Noise(k, 2.f) * 40000.f : 110000.f + Noise(k, 3.f) * 70000.f);
		const float Hm = bNear ? FMath::Lerp(Th.NearH0, Th.NearH1, Noise(k, 4.f)) : FMath::Lerp(Th.FarH0, Th.FarH1, Noise(k, 5.f));
		if (Hm < 5.f) continue;
		const float Dia = Hm * (3.f + 2.f * Noise(k, 6.f));
		const FVector P(C.X + FMath::Cos(A) * Dist, C.Y + FMath::Sin(A) * Dist, BaseZ - 500.f + Hm * 50.f);
		(bNear ? NearL : Far)->AddInstance(FTransform(FRotator(0, Noise(k, 7.f) * 360.f, 0), P, FVector(Dia, Dia, Hm)));
	}
}

void AGPMScenery::BuildTrees()
{
	const FGPMTheme& Th = Track->Theme;
	const int32 Want = FMath::Min(Th.Trees, 2400);
	if (Want <= 0) return;
	const bool bAutumn = Th.TreeHue >= 0.f;
	// chaque arbre : tronc + 2 étages de feuillage (sapins : deux cônes ; feuillus : deux boules décalées)
	UHierarchicalInstancedStaticMeshComponent* Trunk = AddLayer(TEXT("Cylinder"), FLinearColor(0.09f, 0.05f, 0.025f), FLinearColor(0.06f, 0.04f, 0.03f), 0.95f);
	UHierarchicalInstancedStaticMeshComponent* PineA = AddLayer(TEXT("Cone"), FLinearColor(0.02f, 0.08f, 0.03f), FLinearColor(0.04f, 0.12f, 0.04f));
	UHierarchicalInstancedStaticMeshComponent* PineB = AddLayer(TEXT("Cone"), FLinearColor(0.035f, 0.12f, 0.04f), FLinearColor(0.06f, 0.16f, 0.05f));
	UHierarchicalInstancedStaticMeshComponent* LeafA = AddLayer(TEXT("Sphere"), bAutumn ? HueColor(Th.TreeHue, 0.75f, 0.55f) : FLinearColor(0.05f, 0.16f, 0.03f),
		bAutumn ? HueColor(FMath::Frac(Th.TreeHue + 0.06f), 0.8f, 0.5f) : FLinearColor(0.1f, 0.22f, 0.04f));
	UHierarchicalInstancedStaticMeshComponent* LeafB = AddLayer(TEXT("Sphere"), bAutumn ? HueColor(FMath::Frac(Th.TreeHue + 0.04f), 0.65f, 0.45f) : FLinearColor(0.09f, 0.22f, 0.04f),
		bAutumn ? HueColor(FMath::Frac(Th.TreeHue - 0.03f), 0.7f, 0.4f) : FLinearColor(0.14f, 0.27f, 0.06f));
	UHierarchicalInstancedStaticMeshComponent* Palm = AddLayer(TEXT("Sphere"), FLinearColor(0.1f, 0.22f, 0.04f), FLinearColor(0.16f, 0.26f, 0.05f));
	const int32 N = Track->Points.Num();
	const float Half = Track->Width / 2.f;
	FRandomStream Rnd(Track->Id.Len() * 7919 + N);
	int32 Placed = 0;
	for (int32 Try = 0; Try < Want * 8 && Placed < Want; ++Try)
	{
		const int32 i = Rnd.RandRange(0, N - 1);
		const float Side = Rnd.FRand() < 0.5f ? -1.f : 1.f;
		const float Wall = (Side < 0 ? (Track->WallLeft.IsValidIndex(i) ? Track->WallLeft[i] : Half + 9.f) : (Track->WallRight.IsValidIndex(i) ? Track->WallRight[i] : Half + 9.f));
		const float Dist = Wall + 6.f + FMath::Pow(Rnd.FRand(), 1.6f) * 200.f;
		// pas sur la voie des stands ni ses bâtiments
		const int32 PK = Track->PitK(i);
		if (Side > 0 && PK > Track->PitE0 - 20 && PK < Track->PitX1 + 20 && Dist < Track->PitLat + 30.f) continue;
		const FVector Rt(-Track->TZ[i], Track->TX[i], 0.f);   // droite (lat > 0) en repère Unreal
		const FVector P = Track->Points[i] + Rt * Side * Dist * 100.f;
		if (NearTrack(P.X, P.Y) < Half + 12.f) continue;   // trop près d'une autre portion de piste
		const float Z = GroundZ(P.X, P.Y);
		const float Hm = 7.f + Rnd.FRand() * 9.f;          // hauteur (m)
		const FRotator Yaw(0, Rnd.FRand() * 360.f, 0);
		FString Kind = Th.TreeKind;
		if (Kind == TEXT("palmmix")) Kind = Rnd.FRand() < 0.5f ? TEXT("palm") : TEXT("mix");
		if (Kind == TEXT("mix") && Rnd.FRand() < 0.3f) Kind = TEXT("pine");
		if (Kind == TEXT("pine"))
		{
			Trunk->AddInstance(FTransform(Yaw, FVector(P.X, P.Y, Z + Hm * 12.5f), FVector(0.45f, 0.45f, Hm * 0.25f)));
			UHierarchicalInstancedStaticMeshComponent* Pine = Rnd.FRand() < 0.5f ? PineA : PineB;
			Pine->AddInstance(FTransform(Yaw, FVector(P.X, P.Y, Z + Hm * 50.f), FVector(Hm * 0.46f, Hm * 0.46f, Hm * 0.6f)));
			Pine->AddInstance(FTransform(Yaw, FVector(P.X, P.Y, Z + Hm * 80.f), FVector(Hm * 0.32f, Hm * 0.32f, Hm * 0.45f)));
		}
		else if (Kind == TEXT("palm"))
		{
			Trunk->AddInstance(FTransform(Yaw, FVector(P.X, P.Y, Z + Hm * 50.f), FVector(0.35f, 0.35f, Hm)));
			Palm->AddInstance(FTransform(Yaw, FVector(P.X, P.Y, Z + Hm * 100.f), FVector(Hm * 0.5f, Hm * 0.5f, Hm * 0.16f)));
		}
		else
		{
			Trunk->AddInstance(FTransform(Yaw, FVector(P.X, P.Y, Z + Hm * 20.f), FVector(0.55f, 0.55f, Hm * 0.4f)));
			UHierarchicalInstancedStaticMeshComponent* Leaf = Rnd.FRand() < 0.5f ? LeafA : LeafB;
			Leaf->AddInstance(FTransform(Yaw, FVector(P.X, P.Y, Z + Hm * 60.f), FVector(Hm * 0.62f, Hm * 0.62f, Hm * 0.52f)));
			const FVector Off = Yaw.RotateVector(FVector(Hm * 16.f, 0.f, 0.f));
			Leaf->AddInstance(FTransform(Yaw, FVector(P.X + Off.X, P.Y + Off.Y, Z + Hm * 74.f), FVector(Hm * 0.42f, Hm * 0.42f, Hm * 0.38f)));
		}
		++Placed;
	}
}

void AGPMScenery::BuildCity()
{
	const FGPMTheme& Th = Track->Theme;
	const int32 Want = FMath::Min(Th.City, 500);
	if (Want <= 0) return;
	// immeubles : bétons clairs, pierre beige, façades sombres ; bandeaux vitrés à chaque étage
	UHierarchicalInstancedStaticMeshComponent* L[3] = {
		AddLayer(TEXT("Cube"), FLinearColor(0.45f, 0.44f, 0.42f), FLinearColor(0.62f, 0.6f, 0.56f), 0.85f, true),
		AddLayer(TEXT("Cube"), FLinearColor(0.55f, 0.48f, 0.38f), FLinearColor(0.42f, 0.33f, 0.25f), 0.85f, true),
		AddLayer(TEXT("Cube"), FLinearColor(0.16f, 0.2f, 0.26f), FLinearColor(0.3f, 0.34f, 0.38f), 0.5f, true) };
	const int32 N = Track->Points.Num();
	FRandomStream Rnd(N * 31 + 7);
	int32 Placed = 0;
	for (int32 Try = 0; Try < Want * 8 && Placed < Want; ++Try)
	{
		const int32 i = Rnd.RandRange(0, N - 1);
		const float Side = Rnd.FRand() < 0.5f ? -1.f : 1.f;
		const float Fp = 14.f + Rnd.FRand() * 26.f;                 // emprise (m)
		const float Dist = Track->Width / 2.f + 30.f + Fp / 2 + FMath::Pow(Rnd.FRand(), 1.3f) * 380.f;
		const int32 PK = Track->PitK(i);
		if (Side > 0 && PK > Track->PitE0 - 20 && PK < Track->PitX1 + 20 && Dist < Track->PitLat + 40.f) continue;
		const FVector Rt(-Track->TZ[i], Track->TX[i], 0.f);
		const FVector P = Track->Points[i] + Rt * Side * Dist * 100.f;
		if (NearTrack(P.X, P.Y) < Track->Width / 2.f + 18.f + Fp / 2) continue;
		const float Hm = FMath::Lerp(Th.CityHMin, Th.CityHMax, FMath::Pow(Rnd.FRand(), 2.f));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Track->TX[i], -Track->TZ[i]));
		L[Rnd.RandRange(0, 2)]->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(P.X, P.Y, GroundZ(P.X, P.Y) + Hm * 50.f), FVector(Fp, Fp * (0.7f + Rnd.FRand() * 0.6f), Hm)));
		++Placed;
	}
}

// Tribunes en gradins le long de la ligne droite des stands (côté gauche) et bâtiment des stands (côté droit)
void AGPMScenery::BuildStands()
{
	UHierarchicalInstancedStaticMeshComponent* Concrete = AddLayer(TEXT("Cube"), FLinearColor(0.42f, 0.42f, 0.44f), FLinearColor(0.5f, 0.5f, 0.52f), 0.9f);
	UHierarchicalInstancedStaticMeshComponent* Seats = AddLayer(TEXT("Cube"), FLinearColor(0.05f, 0.14f, 0.45f), FLinearColor(0.04f, 0.1f, 0.35f), 0.6f);
	UHierarchicalInstancedStaticMeshComponent* Pits = AddLayer(TEXT("Cube"), FLinearColor(0.75f, 0.76f, 0.78f), FLinearColor(0.82f, 0.83f, 0.85f), 0.6f, true);
	UHierarchicalInstancedStaticMeshComponent* Roof = AddLayer(TEXT("Cube"), FLinearColor(0.85f, 0.86f, 0.88f), FLinearColor(0.7f, 0.72f, 0.75f), 0.4f);
	// spectateurs : petits blocs de toutes les couleurs sur les gradins (deux calques = quatre couleurs mélangées)
	UHierarchicalInstancedStaticMeshComponent* FansA = AddLayer(TEXT("Cube"), FLinearColor(0.75f, 0.08f, 0.06f), FLinearColor(0.9f, 0.75f, 0.1f), 0.8f);
	UHierarchicalInstancedStaticMeshComponent* FansB = AddLayer(TEXT("Cube"), FLinearColor(0.08f, 0.25f, 0.8f), FLinearColor(0.85f, 0.85f, 0.85f), 0.8f);
	FansA->SetCullDistances(0, 60000); FansB->SetCullDistances(0, 60000);   // 600 m : invisibles de plus loin
	const int32 N = Track->Points.Num();
	auto Frame = [&](int32 K, float Lat, FVector& OutP, float& OutYaw)
	{
		const int32 i = ((K % N) + N) % N;
		OutP = Track->Points[i] + FVector(-Track->TZ[i], Track->TX[i], 0.f) * Lat * 100.f;
		OutYaw = FMath::RadiansToDegrees(FMath::Atan2(Track->TZ[i], Track->TX[i]));
	};
	// 4 tribunes de 40 m, 3 gradins de 4 m de profondeur
	for (int32 S = 0; S < 4; ++S)
	{
		const int32 K = -60 + S * 24;
		const int32 i = ((K % N) + N) % N;
		const float Wall = Track->WallLeft.IsValidIndex(i) ? Track->WallLeft[i] : Track->Width / 2.f + 9.f;
		for (int32 Step = 0; Step < 3; ++Step)
		{
			FVector P; float Yaw;
			Frame(K, -(Wall + 4.f + Step * 4.f), P, Yaw);
			const float Hm = 2.f + Step * 2.5f;
			Concrete->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(P.X, P.Y, P.Z + Hm * 50.f), FVector(40.f, 4.f, Hm)));
			Seats->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(P.X, P.Y, P.Z + Hm * 100.f + 20.f), FVector(40.f, 3.6f, 0.4f)));
			// une rangée de spectateurs (65 cm par place, quelques places vides)
			const FVector Along = FRotator(0, Yaw, 0).RotateVector(FVector(100.f, 0.f, 0.f));
			for (int32 Seat = 0; Seat < 58; ++Seat)
			{
				const float Pick = Noise(S * 7.f + Step, Seat * 0.71f);
				if (Pick < 0.12f) continue;
				const FVector At = FVector(P.X, P.Y, P.Z + Hm * 100.f + 70.f) + Along * ((Seat - 28.5f) * 0.68f);
				(Pick < 0.56f ? FansA : FansB)->AddInstance(FTransform(FRotator(0, Yaw, 0), At, FVector(0.42f, 0.36f, 0.65f + 0.25f * Noise(Seat, Step + S))));
			}
		}
		// toit de la tribune sur 4 poteaux
		{
			FVector P; float Yaw;
			Frame(K, -(Wall + 8.f), P, Yaw);
			const FRotator Rot(0, Yaw, 0);
			Roof->AddInstance(FTransform(Rot, FVector(P.X, P.Y, P.Z + 1250.f), FVector(42.f, 14.f, 0.35f)));
			for (int32 Post = 0; Post < 4; ++Post)
			{
				// poteaux derrière le dernier gradin (+Y local = vers la piste)
				const FVector Q = FVector(P.X, P.Y, P.Z + 620.f) + Rot.RotateVector(FVector((Post - 1.5f) * 1300.f, -650.f, 0.f));
				Concrete->AddInstance(FTransform(Rot, Q, FVector(0.4f, 0.4f, 12.4f)));
			}
		}
	}
	// bâtiment des stands : blocs de 20 m derrière la voie des stands
	for (int32 K = -130; K <= -10; K += 10)
	{
		FVector P; float Yaw;
		Frame(K, Track->PitLat + Track->PitHalf + 7.f, P, Yaw);
		Pits->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(P.X, P.Y, P.Z + 350.f), FVector(20.5f, 12.f, 7.f)));
	}
}
