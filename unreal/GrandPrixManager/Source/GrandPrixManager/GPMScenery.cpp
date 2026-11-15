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
}

// Arbres en géométrie fusionnée (formes de scenery.js) : couleurs par sommet (alpha = rugosité du matériau M_GPM_Lit)
namespace GPMTreeGeo
{
	struct FBuf { TArray<FVector> V, N; TArray<FLinearColor> C; TArray<int32> I; };

	FLinearColor Shade(const FLinearColor& C, float K, float Rough)
	{
		return FLinearColor(FMath::Min(C.R * K, 1.f), FMath::Min(C.G * K, 1.f), FMath::Min(C.B * K, 1.f), Rough);
	}

	// triangles ajoutés depuis First : face visible du côté des normales des sommets (normale de face Unreal = −(B−A)×(C−A))
	void Orient(FBuf& B, int32 First)
	{
		for (int32 k = First; k + 2 < B.I.Num(); k += 3)
		{
			const FVector& A = B.V[B.I[k]];
			const FVector Front = -FVector::CrossProduct(B.V[B.I[k + 1]] - A, B.V[B.I[k + 2]] - A);
			if (FVector::DotProduct(Front, B.N[B.I[k]] + B.N[B.I[k + 1]] + B.N[B.I[k + 2]]) < 0.0) Swap(B.I[k + 1], B.I[k + 2]);
		}
	}

	// tronc : tube à 5 pans passant par Pts (anneaux horizontaux : penche un peu sans se tordre)
	void Tube(FBuf& B, const FVector* Pts, const float* Rad, int32 Num, const FLinearColor& Col)
	{
		const int32 S = 5, O = B.V.Num();
		for (int32 j = 0; j < Num; ++j)
			for (int32 k = 0; k < S; ++k)
			{
				const float A = 2.f * PI * (k + 0.3f * j) / S;
				const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f);
				B.V.Add(Pts[j] + Rd * Rad[j]);
				B.N.Add(Rd);
				B.C.Add(Shade(Col, 0.6f + 0.4f * float(j) / float(FMath::Max(1, Num - 1)), 0.95f));
			}
		const int32 First = B.I.Num();
		for (int32 j = 0; j + 1 < Num; ++j)
			for (int32 k = 0; k < S; ++k)
			{
				const int32 a = O + j * S + k, b = O + j * S + (k + 1) % S;
				B.I.Append({ a, b, a + S, b, b + S, a + S });
			}
		Orient(B, First);
	}

	// sphère géodésique unité : icosaèdre subdivisé une fois (42 sommets, 80 faces)
	struct FIco
	{
		TArray<FVector> P; TArray<int32> T;
		FIco()
		{
			const float G = (1.f + FMath::Sqrt(5.f)) / 2.f;
			P = { FVector(-1, G, 0), FVector(1, G, 0), FVector(-1, -G, 0), FVector(1, -G, 0), FVector(0, -1, G), FVector(0, 1, G),
				FVector(0, -1, -G), FVector(0, 1, -G), FVector(G, 0, -1), FVector(G, 0, 1), FVector(-G, 0, -1), FVector(-G, 0, 1) };
			T = { 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
				3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1 };
			for (FVector& Q : P) Q.Normalize();
			TMap<uint64, int32> Mids;
			auto MidPoint = [&](int32 a, int32 b)
			{
				const uint64 Key = a < b ? (uint64(a) << 32) | uint32(b) : (uint64(b) << 32) | uint32(a);
				if (const int32* Found = Mids.Find(Key)) return *Found;
				const FVector M = ((P[a] + P[b]) * 0.5).GetSafeNormal();
				const int32 Id = P.Add(M);
				Mids.Add(Key, Id);
				return Id;
			};
			TArray<int32> T2;
			for (int32 k = 0; k + 2 < T.Num(); k += 3)
			{
				const int32 a = T[k], b = T[k + 1], c = T[k + 2];
				const int32 ab = MidPoint(a, b), bc = MidPoint(b, c), ca = MidPoint(c, a);
				T2.Append({ a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca });
			}
			T = MoveTemp(T2);
		}
	};
	const FIco& Ico() { static const FIco Shared; return Shared; }

	// couronne de feuillu : boule bosselée (rayons Rad, cm), plus sombre dessous, normales un peu vers le ciel
	void Crown(FBuf& B, const FVector& Ctr, const FVector& Rad, float Seed, const FLinearColor& Col)
	{
		const FIco& S = Ico();
		const int32 O = B.V.Num();
		for (const FVector& P : S.P)
		{
			const float Lump = 0.84f + 0.3f * Noise(P.X * 3.1f + Seed, P.Y * 2.7f + P.Z * 1.9f);
			B.V.Add(Ctr + P * Rad * Lump);
			B.N.Add((P + FVector(0, 0, 0.3)).GetSafeNormal());
			B.C.Add(Shade(Col, 0.42f + 0.7f * float(P.Z * 0.5 + 0.5), 0.8f));
		}
		const int32 First = B.I.Num();
		for (int32 t : S.T) B.I.Add(O + t);
		Orient(B, First);
	}

	// étage de sapin : cône à 7 pans (pointe dédoublée pour des pans nets) et dessous légèrement creux
	void Tier(FBuf& B, const FVector& Base, float R, float H, float Yaw, const FLinearColor& Col)
	{
		const int32 S = 7, O = B.V.Num();
		const float Sl = R / FMath::Max(H, 1.f);
		for (int32 k = 0; k < S; ++k)
		{
			const float A = Yaw + 2.f * PI * k / S;
			const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f);
			B.V.Add(Base + Rd * R); B.N.Add((Rd + FVector(0, 0, Sl)).GetSafeNormal()); B.C.Add(Shade(Col, 0.5f, 0.85f));
		}
		for (int32 k = 0; k < S; ++k)
		{
			const float A = Yaw + 2.f * PI * (k + 0.5f) / S;
			const FVector Rd(FMath::Cos(A), FMath::Sin(A), 0.f);
			B.V.Add(Base + FVector(0, 0, H)); B.N.Add((Rd + FVector(0, 0, Sl)).GetSafeNormal()); B.C.Add(Shade(Col, 1.1f, 0.85f));
		}
		const int32 Ctr = B.V.Num();
		B.V.Add(Base + FVector(0, 0, H * 0.12f)); B.N.Add(FVector(0, 0, -1)); B.C.Add(Shade(Col, 0.3f, 0.85f));
		for (int32 k = 0; k < S; ++k)
		{
			const FVector Rim = B.V[O + k];
			B.V.Add(Rim); B.N.Add(FVector(0, 0, -1)); B.C.Add(Shade(Col, 0.35f, 0.85f));
		}
		const int32 First = B.I.Num();
		for (int32 k = 0; k < S; ++k)
		{
			const int32 k1 = (k + 1) % S;
			B.I.Append({ O + k, O + k1, O + S + k });
			B.I.Append({ Ctr, Ctr + 1 + k1, Ctr + 1 + k });
		}
		Orient(B, First);
	}

	// palme (palmGeo de scenery.js) : bande effilée qui monte puis retombe, nervure relevée, deux faces
	void Frond(FBuf& B, const FVector& Top, float A, float Len, float Hgt, float Lift, const FLinearColor& Col)
	{
		const int32 S = 4;
		const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f), Side(-FMath::Sin(A), FMath::Cos(A), 0.f);
		for (int32 Face = 0; Face < 2; ++Face)
		{
			const int32 O = B.V.Num();
			const FVector Up(0, 0, Face == 0 ? 1.f : -1.f);
			for (int32 k = 0; k <= S; ++k)
			{
				const float t = float(k) / S;
				const float Wd = 0.17f * FMath::Sin(PI * FMath::Min(1.f, t * 1.15f + 0.08f)) * Len;
				const FVector Mid = Top + Dir * (t * Len) + FVector(0, 0, (Lift * FMath::Sin(t * 2.2f) - 0.9f * t * t) * Hgt);
				const FLinearColor Cc = Shade(Col, Face == 0 ? 0.75f + 0.4f * t : 0.5f, 0.8f);
				B.V.Add(Mid - Side * Wd); B.V.Add(Mid + FVector(0, 0, Wd * 0.3f)); B.V.Add(Mid + Side * Wd);
				for (int32 m = 0; m < 3; ++m) { B.N.Add(Up); B.C.Add(Cc); }
			}
			const int32 First = B.I.Num();
			for (int32 k = 0; k < S; ++k)
			{
				const int32 a = O + k * 3, b = a + 3;
				B.I.Append({ a, b, a + 1, a + 1, b, b + 1, a + 1, b + 1, a + 2, a + 2, b + 1, b + 2 });
			}
			Orient(B, First);
		}
	}
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
	LoadDecor();
	BuildTerrain();
	BuildTrees();
	BuildCity();
	BuildStands();
	PlaceDecor();
	BuildDecorMeshes();
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
	// mer au sud du circuit (seaZ = minZ − 140 m du jeu web) : plage plate au bord, puis le sol plonge sous l'eau
	bSeaSide = Th.bSea;
	SeaY = float(B.Min.Y) - 14000.f;
	if (bSeaSide)
		for (int32 j = 0; j < GY; ++j)
		{
			const float Y = Origin.Y + j * Cell;
			if (Y > SeaY + 6000.f) continue;
			const float Under = FMath::Clamp((SeaY + 1000.f - Y) / 5000.f, 0.f, 1.f);
			for (int32 i = 0; i < GX; ++i)
			{
				float& Hg = Heights[j * GX + i];
				Hg = FMath::Min(Hg, BaseZ) - Under * 500.f;
			}
		}
	// sol photo (herbe ou sable, comme les bas-côtés) : les sommets ne gardent que les nuances
	UMaterialInstanceDynamic* TerrainArt = GPMArt::Surface(Th.Surface == TEXT("sand") ? FString(TEXT("sand")) : FString(TEXT("grass")), 30.f, this);
	const FLinearColor Base = TerrainArt ? FLinearColor(0.85f, 0.85f, 0.85f) : Th.Surface == TEXT("sand") ? FLinearColor(0.42f, 0.32f, 0.17f)
		: Th.Surface == TEXT("concrete") ? FLinearColor(0.16f, 0.2f, 0.1f) : FLinearColor(0.14f, 0.3f, 0.08f);
	Terrain->ClearAllMeshSections();
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

	// Sol jusqu'à l'horizon : anneau plat de 9 km autour de la grille, noyé dans le brouillard bien avant sa coupure
	// (12 km, au-delà commencent le ciel et les montagnes). 30 cm sous le bord de la grille : pas de raccord visible.
	const FVector2D G0 = Origin, G1 = Origin + FVector2D((GX - 1) * Cell, (GY - 1) * Cell);
	const FVector2D Mid2 = (G0 + G1) / 2, Ext = (G1 - G0) / 2;
	const float ROut = FMath::Max(900000.f, float(Ext.Size()) + 100000.f);
	{
		const int32 S = 48;
		TArray<FVector> SV, SN; TArray<FLinearColor> SC; TArray<int32> SI;
		for (int32 k = 0; k < S; ++k)
		{
			const float A = 2.f * PI * k / S;
			const FVector2D D(FMath::Cos(A), FMath::Sin(A));
			const float Reach = FMath::Min(Ext.X / FMath::Max(1e-4f, FMath::Abs(float(D.X))), Ext.Y / FMath::Max(1e-4f, FMath::Abs(float(D.Y))));
			const FVector2D InP = Mid2 + D * Reach, OutP = Mid2 + D * ROut;
			SV.Add(FVector(InP.X, InP.Y, BaseZ - 30.f)); SV.Add(FVector(OutP.X, OutP.Y, BaseZ - 30.f));
			SN.Add(FVector::UpVector); SN.Add(FVector::UpVector);
			SC.Add(Base * 0.92f); SC.Add(Base * 0.92f);
		}
		for (int32 k = 0; k < S; ++k)
		{
			const int32 a = k * 2, b = ((k + 1) % S) * 2;
			SI.Append({ a, b, a + 1, b, b + 1, a + 1 });
		}
		for (int32 k = 0; k + 2 < SI.Num(); k += 3)
			if (FVector::CrossProduct(SV[SI[k + 1]] - SV[SI[k]], SV[SI[k + 2]] - SV[SI[k]]).Z > 0.f) Swap(SI[k + 1], SI[k + 2]);
		Terrain->CreateMeshSection_LinearColor(1, SV, SI, SN, TArray<FVector2D>(), SC, TArray<FProcMeshTangent>(), false);
		Terrain->SetMaterial(1, TerrainArt ? static_cast<UMaterialInterface*>(TerrainArt) : VertexMat.Get());
	}

	// mer d'un seul côté (au sud, comme le jeu web) jusqu'au bout du sol, avec une plage de sable de 50 m
	if (bSeaSide)
	{
		const float Dy = SeaY - float(Mid2.Y);
		const float HalfChord = FMath::Sqrt(FMath::Max(1.f, ROut * ROut - Dy * Dy));
		auto Quad = [&](int32 Section, float Y0, float Y1, float Z, const FLinearColor& C, UMaterialInterface* M)
		{
			TArray<FVector> QV = { FVector(Mid2.X - HalfChord, Y0, Z), FVector(Mid2.X + HalfChord, Y0, Z), FVector(Mid2.X - HalfChord, Y1, Z), FVector(Mid2.X + HalfChord, Y1, Z) };
			TArray<int32> QI = { 0, 2, 1, 1, 2, 3 };
			if (FVector::CrossProduct(QV[QI[1]] - QV[QI[0]], QV[QI[2]] - QV[QI[0]]).Z > 0.f) { Swap(QI[1], QI[2]); Swap(QI[4], QI[5]); }
			TArray<FVector> QN; QN.Init(FVector::UpVector, 4);
			TArray<FLinearColor> QC; QC.Init(C, 4);
			Terrain->CreateMeshSection_LinearColor(Section, QV, QI, QN, TArray<FVector2D>(), QC, TArray<FProcMeshTangent>(), false);
			Terrain->SetMaterial(Section, M);
		};
		UMaterialInstanceDynamic* SandArt = GPMArt::Surface(TEXT("sand"), 8.f, this);
		Quad(2, SeaY - 500.f, SeaY + 4500.f, BaseZ + 5.f, SandArt ? FLinearColor(0.9f, 0.9f, 0.9f) : FLinearColor(0.5f, 0.42f, 0.28f),
			SandArt ? static_cast<UMaterialInterface*>(SandArt) : VertexMat.Get());
		// eau : demi-disque sous le rivage (éventail depuis le milieu du rivage), bleu profond très lisse (alpha = rugosité)
		TArray<FVector> WV; TArray<FVector> WN; TArray<FLinearColor> WC; TArray<int32> WI;
		const FLinearColor Water(0.012f, 0.075f, 0.17f, 0.09f);
		WV.Add(FVector(Mid2.X, SeaY, BaseZ + 10.f));
		// le rivage est au sud du centre (Dy < 0) : de l'ouest (A0, entre −π et −π/2) à l'est (A1, entre −π/2 et 0) par le sud
		const float A0 = FMath::Atan2(Dy, -HalfChord), A1 = FMath::Atan2(Dy, HalfChord);
		const int32 S = 40;
		for (int32 k = 0; k <= S; ++k)
		{
			const float A = A0 + (A1 - A0) * k / S;
			FVector2D Q = Mid2 + FVector2D(FMath::Cos(A), FMath::Sin(A)) * ROut;
			if (k == 0) Q = FVector2D(Mid2.X - HalfChord, SeaY);
			if (k == S) Q = FVector2D(Mid2.X + HalfChord, SeaY);
			WV.Add(FVector(Q.X, FMath::Min(float(Q.Y), SeaY), BaseZ + 10.f));
		}
		for (int32 k = 0; k < S; ++k) WI.Append({ 0, k + 1, k + 2 });
		for (int32 k = 0; k + 2 < WI.Num(); k += 3)
			if (FVector::CrossProduct(WV[WI[k + 1]] - WV[WI[k]], WV[WI[k + 2]] - WV[WI[k]]).Z > 0.f) Swap(WI[k + 1], WI[k + 2]);
		WN.Init(FVector::UpVector, WV.Num());
		WC.Init(Water, WV.Num());
		Terrain->CreateMeshSection_LinearColor(3, WV, WI, WN, TArray<FVector2D>(), WC, TArray<FProcMeshTangent>(), false);
		UMaterialInstanceDynamic* SeaM = GPMArt::Make(TEXT("M_GPM_Lit"), this);
		if (SeaM)
		{
			SeaM->SetScalarParameterValue(TEXT("Rough"), 1.f);
			SeaM->SetScalarParameterValue(TEXT("Metal"), 0.25f);
			SeaM->SetScalarParameterValue(TEXT("Spec"), 0.6f);
		}
		Terrain->SetMaterial(3, SeaM ? static_cast<UMaterialInterface*>(SeaM) : VertexMat.Get());
	}
}

void AGPMScenery::BuildTrees()
{
	for (UProceduralMeshComponent* F : Foliage) if (F) F->DestroyComponent();
	Foliage.Reset();
	const FGPMTheme& Th = Track->Theme;
	const int32 Want = FMath::Min(Th.Trees, 2600);
	if (Want <= 0) return;
	const int32 N = Track->Points.Num();
	FRandomStream Rnd(Track->Id.Len() * 7919 + N);
	TMap<int64, GPMTreeGeo::FBuf> Chunks;
	// teintes (HSV linéaire) : feuillus verts ou teinte du thème (automne, vignes…), sapins toujours sombres, palmiers clairs
	auto Leaf = [&]() -> FLinearColor
	{
		if (Th.TreeHue >= 0.f)
			return FLinearColor(FMath::Frac(Th.TreeHue + Rnd.FRandRange(-1.f, 1.f) * FMath::Max(0.02f, Th.TreeHueSpan)) * 360.f, Rnd.FRandRange(0.6f, 0.85f), Rnd.FRandRange(0.12f, 0.24f)).HSVToLinearRGB();
		return FLinearColor(Rnd.FRandRange(78.f, 115.f), Rnd.FRandRange(0.6f, 0.85f), Rnd.FRandRange(0.08f, 0.17f)).HSVToLinearRGB();
	};
	int32 Placed = 0;
	for (int32 Try = 0; Try < Want * 3 && Placed < Want; ++Try)
	{
		// bosquet de 1 à 7 arbres de la même espèce (ou presque), avec des clairières entre les bosquets
		const int32 i = Rnd.RandRange(0, N - 1);
		const float Side = Rnd.FRand() < 0.5f ? -1.f : 1.f;
		const float Half = Track->Width / 2.f;
		const float Wall = Side < 0 ? (Track->WallLeft.IsValidIndex(i) ? Track->WallLeft[i] : Half + 9.f) : (Track->WallRight.IsValidIndex(i) ? Track->WallRight[i] : Half + 9.f);
		const float Dist = Wall + 12.f + FMath::Pow(Rnd.FRand(), 1.5f) * 260.f;
		const FVector Rt(-Track->TZ[i], Track->TX[i], 0.f);   // droite (lat > 0) en repère Unreal
		const FVector C0 = Track->Points[i] + Rt * Side * Dist * 100.f;
		const int32 Clump = 1 + Rnd.RandRange(0, 6);
		const float Spread = (4.f + 2.5f * Clump) * 100.f;
		const float R = Rnd.FRand();
		const TCHAR* Kind = Th.TreeKind == TEXT("pine") ? (R < 0.85f ? TEXT("p") : TEXT("c"))
			: Th.TreeKind == TEXT("palm") ? TEXT("l")
			: Th.TreeKind == TEXT("palmmix") ? (R < 0.55f ? TEXT("l") : TEXT("c"))
			: (R < 0.62f ? TEXT("c") : TEXT("p"));
		for (int32 m = 0; m < Clump && Placed < Want; ++m)
		{
			const FVector P = C0 + FVector(Rnd.FRandRange(-1.f, 1.f), Rnd.FRandRange(-1.f, 1.f), 0.f) * Spread;
			int32 Near = -1;
			const float Dm = NearTrack(P.X, P.Y, nullptr, &Near);
			if (Near >= 0)
			{
				const float WallN = FMath::Max(Track->WallLeft.IsValidIndex(Near) ? Track->WallLeft[Near] : Half + 9.f, Track->WallRight.IsValidIndex(Near) ? Track->WallRight[Near] : Half + 9.f);
				if (Dm < WallN + 7.f) continue;                                   // derrière les murs et grillages
				if (FMath::Min(Near, N - Near) < 200 && Dm < 75.f) continue;        // stands, voie des stands et tribunes
			}
			if (InSea(P.Y)) continue;
			const float Z = GroundZ(P.X, P.Y);
			const float Sc = 0.75f + Rnd.FRand() * 0.7f;   // taille (le jeu web : 0,75 à 1,45)
			const float Yaw = Rnd.FRand() * 2.f * PI;
			FString K = Kind;
			if (Rnd.FRand() < 0.2f && K != TEXT("l")) K = K == TEXT("c") ? TEXT("p") : TEXT("c");
			GPMTreeGeo::FBuf& Buf = Chunks.FindOrAdd(CellKey(FMath::FloorToInt(P.X / 60000.f), FMath::FloorToInt(P.Y / 60000.f)));
			const FVector G(P.X, P.Y, Z - 20.f);
			if (K == TEXT("c"))
			{
				// feuillu : tronc court, grosse couronne bosselée (parfois une seconde boule décalée)
				const FVector Pts[2] = { G, G + FVector(0, 0, 360.f * Sc) };
				const float Rad[2] = { 40.f * Sc, 24.f * Sc };
				GPMTreeGeo::Tube(Buf, Pts, Rad, 2, FLinearColor(0.085f, 0.055f, 0.03f));
				const FLinearColor Col = Leaf();
				GPMTreeGeo::Crown(Buf, G + FVector(0, 0, 650.f * Sc), FVector(420.f, 420.f, 440.f) * Sc, Rnd.FRand() * 10.f, Col);
				if (Rnd.FRand() < 0.5f)
				{
					const FVector Off = FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f) * (260.f * Sc);
					GPMTreeGeo::Crown(Buf, G + Off + FVector(0, 0, 820.f * Sc), FVector(270.f, 270.f, 280.f) * Sc, Rnd.FRand() * 10.f, Col * 1.12f);
				}
			}
			else if (K == TEXT("p"))
			{
				// sapin : 4 étages de plus en plus étroits sur un tronc sombre (13 m × taille)
				const FVector Pts[2] = { G, G + FVector(0, 0, 220.f * Sc) };
				const float Rad[2] = { 32.f * Sc, 22.f * Sc };
				GPMTreeGeo::Tube(Buf, Pts, Rad, 2, FLinearColor(0.06f, 0.035f, 0.02f));
				const FLinearColor Col = FLinearColor(Rnd.FRandRange(100.f, 140.f), Rnd.FRandRange(0.5f, 0.72f), Rnd.FRandRange(0.045f, 0.09f)).HSVToLinearRGB();
				const float H = 1300.f * Sc;
				for (int32 Lv = 0; Lv < 4; ++Lv)
					GPMTreeGeo::Tier(Buf, G + FVector(0, 0, 150.f * Sc + H * 0.22f * Lv), 300.f * Sc * (1.f - 0.2f * Lv), H * 0.36f, Yaw + Lv * 0.7f, Col * (1.f + 0.08f * Lv));
			}
			else
			{
				// palmier : tronc qui penche en s'incurvant, 8 palmes arquées et un cœur sombre
				const FVector Lean = FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f) * (Rnd.FRandRange(0.4f, 1.6f) * 100.f * Sc);
				const float Ht = 1050.f * Sc;
				FVector Pts[4]; float Rad[4];
				for (int32 j = 0; j < 4; ++j)
				{
					const float t = j / 3.f;
					Pts[j] = G + Lean * (t * t) + FVector(0, 0, Ht * t);
					Rad[j] = (24.f - 8.f * t) * Sc;
				}
				GPMTreeGeo::Tube(Buf, Pts, Rad, 4, FLinearColor(0.2f, 0.15f, 0.1f));
				const FLinearColor Col = FLinearColor(Rnd.FRandRange(72.f, 98.f), Rnd.FRandRange(0.6f, 0.8f), Rnd.FRandRange(0.12f, 0.22f)).HSVToLinearRGB();
				const FVector Top = Pts[3] + FVector(0, 0, 30.f * Sc);
				for (int32 f = 0; f < 8; ++f)
					GPMTreeGeo::Frond(Buf, Top, Yaw + f / 8.f * 2.f * PI + (f % 2) * 0.2f, 420.f * Sc, 260.f * Sc, f % 2 ? 0.35f : 0.15f, Col);
				GPMTreeGeo::Crown(Buf, Top, FVector(45.f, 45.f, 55.f) * Sc, 1.f, FLinearColor(0.09f, 0.08f, 0.04f));
			}
			++Placed;
		}
	}
	UMaterialInstanceDynamic* Mat = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	if (Mat)
	{
		Mat->SetScalarParameterValue(TEXT("Rough"), 1.f);   // rugosité = alpha des sommets (feuilles 0,8, écorce 0,95)
		Mat->SetScalarParameterValue(TEXT("Spec"), 0.3f);
	}
	for (TPair<int64, GPMTreeGeo::FBuf>& It : Chunks)
	{
		GPMTreeGeo::FBuf& Buf = It.Value;
		if (Buf.I.Num() == 0) continue;
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
		PM->SetupAttachment(RootComponent);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->CastShadow = false;
		PM->bAffectDistanceFieldLighting = false;
		PM->CreateMeshSection_LinearColor(0, Buf.V, Buf.I, Buf.N, TArray<FVector2D>(), Buf.C, TArray<FProcMeshTangent>(), false);
		PM->SetMaterial(0, Mat ? static_cast<UMaterialInterface*>(Mat) : VertexMat.Get());
		PM->SetCullDistance(260000.f);   // 2,6 km (le brouillard cache le reste)
		PM->RegisterComponent();
		Foliage.Add(PM);
	}
	UE_LOG(LogGPM, Log, TEXT("Décor : %d arbres en %d carrés"), Placed, Foliage.Num());
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
		if (NearTrack(P.X, P.Y) < Track->Width / 2.f + 18.f + Fp / 2 || InSea(P.Y - Fp * 50.f)) continue;
		const float Hm = FMath::Lerp(Th.CityHMin, Th.CityHMax, FMath::Pow(Rnd.FRand(), 2.f));
		if (HasDecor(TEXT("imm-verre-16")))
		{
			// immeuble Blender : style selon la hauteur, variante au nombre d'étages le plus proche (scenery.js)
			const float R2 = Rnd.FRand();
			const TCHAR* Kind = Hm > 60.f ? TEXT("verre") : Hm > 25.f ? (R2 < 0.45f ? TEXT("verre") : TEXT("logements")) : (R2 < 0.5f ? TEXT("logements") : TEXT("ancien"));
			const TArray<int32> Fl = FString(Kind) == TEXT("verre") ? TArray<int32>{ 8, 16, 32 } : FString(Kind) == TEXT("logements") ? TArray<int32>{ 5, 10, 18 } : TArray<int32>{ 3, 5, 8 };
			int32 F = Fl[0];
			for (int32 f : Fl) if (FMath::Abs(FMath::Loge(f * 3.4f / Hm)) < FMath::Abs(FMath::Loge(F * 3.4f / Hm))) F = f;
			static const uint32 Pal[] = { 0xd8d2c4, 0xb9c2cc, 0x9aa8b8, 0xe6e0d4, 0x8f9aa6, 0xc7b299, 0xa7b7c4, 0xd9c7b0 }, Glass[] = { 0xe8ecf0, 0xc9d2da, 0x9fb3c4 };
			const uint32 Hex = FString(Kind) == TEXT("verre") ? Glass[Rnd.RandRange(0, 2)] : Pal[Rnd.RandRange(0, 7)];
			FDecorItem It;
			It.X = P.X / 100.f; It.Z = P.Y / 100.f; It.Y = GroundZ(P.X, P.Y) / 100.f - 1.f;
			It.Ry = Track->H[i];
			const float Bw = Fp, Bd = Fp * (0.7f + Rnd.FRand() * 0.6f);
			It.Sx = Bw / 20.f; It.Sz = Bd / 20.f; It.Sy = Hm / (F * 3.4f);
			It.Col = FLinearColor(FColor((Hex >> 16) & 255, (Hex >> 8) & 255, Hex & 255));
			DecorItems.FindOrAdd(FString::Printf(TEXT("imm-%s-%d"), Kind, F)).Add(It);
			++Placed;
			continue;
		}
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
	// bâtiment des stands : blocs de 20 m derrière la voie des stands (sans les modules Blender)
	if (!HasDecor(TEXT("garage")))
	for (int32 K = -130; K <= -10; K += 10)
	{
		FVector P; float Yaw;
		Frame(K, Track->PitLat + Track->PitHalf + 7.f, P, Yaw);
		Pits->AddInstance(FTransform(FRotator(0, Yaw, 0), FVector(P.X, P.Y, P.Z + 350.f), FVector(20.5f, 12.f, 7.f)));
	}
}
