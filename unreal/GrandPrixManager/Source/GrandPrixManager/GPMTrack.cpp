#include "GPMTrack.h"
#include "GPMDataSubsystem.h"
#include "GrandPrixManager.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Section de maillage en construction (une bande ou des quads).
	struct FSection
	{
		TArray<FVector> V;
		TArray<int32> I;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> Col;

		int32 Add(const FVector& P, const FVector& Nrm, const FVector2D& T, const FLinearColor& C)
		{
			N.Add(Nrm); UV.Add(T); Col.Add(C);
			return V.Add(P);
		}
		// Quad a-b-c-d (a,b sur la rangée i ; c,d sur la rangée i+1). Ordre des sommets pour une face visible du dessus.
		void Quad(int32 A, int32 B, int32 C, int32 D)
		{
			I.Append({ A, C, B, B, C, D });
		}
	};

	const FLinearColor Asphalt(0.09f, 0.09f, 0.10f);
	const FLinearColor Grass(0.16f, 0.36f, 0.10f);
	const FLinearColor KerbRed(0.75f, 0.06f, 0.05f);
	const FLinearColor KerbWhite(0.92f, 0.92f, 0.92f);
	const FLinearColor Wall(0.80f, 0.80f, 0.82f);
	const FLinearColor Ground(0.13f, 0.30f, 0.09f);
}

AGPMTrack::AGPMTrack()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Piste"));
	RootComponent = Mesh;
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->CastShadow = false;

	// Matériau par défaut qui affiche les couleurs par sommet, en attendant les vrais matériaux (étape 9).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VC(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VC.Succeeded())
	{
		AsphaltMaterial = KerbMaterial = GrassMaterial = WallMaterial = VC.Object;
	}
}

FVector AGPMTrack::Right(int32 i) const
{
	const int32 N = Track->Num();
	const FVector T = (Track->Points[(i + 1) % N] - Track->Points[(i - 1 + N) % N]).GetSafeNormal2D();
	// Jeu web : droite = (-TZ, TX) dans le plan (x, z) ; ici le z du web est l'axe Y d'Unreal.
	return FVector(-T.Y, T.X, 0.f);
}

bool AGPMTrack::BuildCircuit(const FString& Id)
{
	UGPMDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	Track = Data ? Data->GetTrack(Id) : nullptr;
	if (!Track || Track->Num() < 4)
	{
		UE_LOG(LogGPM, Warning, TEXT("Circuit introuvable : %s"), *Id);
		return false;
	}
	CircuitId = Id;
	Mesh->ClearAllMeshSections();

	const int32 N = Track->Num();
	const float Half = Track->Width * 50.f;         // demi-largeur (cm)
	const float KerbW = 120.f;                      // largeur des vibreurs (cm)
	const float WallH = 100.f;                      // hauteur des murs (cm)
	const FVector Up = FVector::UpVector;
	auto WallDist = [&](const TArray<float>& W, int32 i) { return FMath::Max((W.IsValidIndex(i) ? W[i] : Track->Width * 0.5f + 9.f) * 100.f, Half + KerbW + 50.f); };
	auto Kerb = [&](int32 i)
	{
		float C = 0.f;   // courbure max sur ±3 points, comme le décor web
		for (int32 k = -3; k <= 3; ++k) C = FMath::Max(C, FMath::Abs(Track->Curvature.IsValidIndex((i + k + N) % N) ? Track->Curvature[(i + k + N) % N] : 0.f));
		return C > KerbCurvature;
	};

	FSection Road, Kerbs, Green, Walls, Floor, Start, Pit;
	float MinZ = TNumericLimits<float>::Max();
	float Dist = 0.f;

	for (int32 i = 0; i <= N; ++i)
	{
		const int32 j = i % N;
		const FVector P = Track->Points[j];
		const FVector R = Right(j);
		MinZ = FMath::Min(MinZ, (float)P.Z);
		if (i > 0) Dist += FVector::Dist(P, Track->Points[i - 1]) / 100.f;
		const float U = Dist / 10.f;   // UV : 1 répétition tous les 10 m

		// Asphalte : 2 sommets par point (gauche = -R, droite = +R)
		Road.Add(P - R * Half, Up, FVector2D(0, U), Asphalt);
		Road.Add(P + R * Half, Up, FVector2D(1, U), Asphalt);
		if (i > 0) { const int32 b = Road.V.Num() - 4; Road.Quad(b, b + 1, b + 2, b + 3); }

		// Herbe de chaque côté, du bord de la piste jusqu'au mur (légèrement plus basse)
		const float DL = WallDist(Track->WallLeft, j), DR = WallDist(Track->WallRight, j);
		const FVector Low = -Up * 3.f;
		const FLinearColor GrassC = (i / 5) % 2 ? Grass : Grass * 1.18f;   // bandes de tonte tous les 10 m
		Green.Add(P - R * DL + Low, Up, FVector2D(0, U), GrassC);
		Green.Add(P - R * Half + Low, Up, FVector2D(1, U), GrassC);
		Green.Add(P + R * Half + Low, Up, FVector2D(0, U), GrassC);
		Green.Add(P + R * DR + Low, Up, FVector2D(1, U), GrassC);
		if (i > 0) { const int32 b = Green.V.Num() - 8; Green.Quad(b, b + 1, b + 4, b + 5); Green.Quad(b + 2, b + 3, b + 6, b + 7); }

		// Murs : faces tournées vers la piste, gauche et droite
		const FVector WLb = P - R * DL, WRb = P + R * DR;
		Walls.Add(WLb, R, FVector2D(0, U), Wall);
		Walls.Add(WLb + Up * WallH, R, FVector2D(1, U), Wall);
		const int32 PK = Track->PitK(j);
		const bool bPitOpen = PK >= Track->PitE0 && PK <= Track->PitX1;   // mur droit ouvert le long de la voie des stands
		Walls.Add(WRb + Up * (bPitOpen ? 0.f : WallH), -R, FVector2D(0, U), Wall);
		Walls.Add(WRb, -R, FVector2D(1, U), Wall);
		if (i > 0) { const int32 b = Walls.V.Num() - 8; Walls.Quad(b, b + 1, b + 4, b + 5); Walls.Quad(b + 2, b + 3, b + 6, b + 7); }
	}

	// Vibreurs : quads séparés là où la courbure dépasse le seuil, rouge et blanc alternés tous les 2 points (4 m)
	for (int32 i = 0; i < N; ++i)
	{
		if (!Kerb(i)) continue;
		const int32 j = (i + 1) % N;
		const FLinearColor C = (i / 2) % 2 ? KerbRed : KerbWhite;
		const FVector Lift = Up * 2.f;
		for (float Side : { -1.f, 1.f })
		{
			const FVector A0 = Track->Points[i] + Right(i) * Side * Half + Lift, A1 = Track->Points[i] + Right(i) * Side * (Half + KerbW) + Lift;
			const FVector B0 = Track->Points[j] + Right(j) * Side * Half + Lift, B1 = Track->Points[j] + Right(j) * Side * (Half + KerbW) + Lift;
			const int32 a = Kerbs.Add(Side < 0 ? A1 : A0, Up, FVector2D(0, 0), C);
			Kerbs.Add(Side < 0 ? A0 : A1, Up, FVector2D(1, 0), C);
			Kerbs.Add(Side < 0 ? B1 : B0, Up, FVector2D(0, 1), C);
			Kerbs.Add(Side < 0 ? B0 : B1, Up, FVector2D(1, 1), C);
			Kerbs.Quad(a, a + 1, a + 2, a + 3);
		}
	}

	// Voie des stands : bande d'asphalte de 8 m qui s'écarte de la piste (côté droit), plus des marques blanches aux boxes
	for (int32 K = Track->PitE0; K <= Track->PitX1; ++K)
	{
		const int32 i = ((K % N) + N) % N;
		const FVector P = Track->Points[i] + Up * 2.f, R = Right(i);
		const float L = Track->PitLatAt(K) * 100.f;
		const FLinearColor C = (K >= -125 && K <= -125 + 9 * 13 && ((K + 125) % 13) == 0) ? KerbWhite : Asphalt;
		Pit.Add(P + R * (L - 400.f), Up, FVector2D(0, K / 5.f), C);
		Pit.Add(P + R * (L + 400.f), Up, FVector2D(1, K / 5.f), C);
		if (K > Track->PitE0) { const int32 b = Pit.V.Num() - 4; Pit.Quad(b, b + 1, b + 2, b + 3); }
	}

	// Ligne de départ (point 0) : bande blanche de 1 m
	{
		const FVector P0 = Track->Points[0] + Up * 1.f, P1 = Track->Points[1 % N] + Up * 1.f;
		const FVector T = (P1 - P0).GetSafeNormal() * 50.f, R = Right(0) * Half;
		const int32 a = Start.Add(P0 - R - T, Up, FVector2D(0, 0), KerbWhite);
		Start.Add(P0 + R - T, Up, FVector2D(1, 0), KerbWhite);
		Start.Add(P0 - R + T, Up, FVector2D(0, 1), KerbWhite);
		Start.Add(P0 + R + T, Up, FVector2D(1, 1), KerbWhite);
		Start.Quad(a, a + 1, a + 2, a + 3);
	}

	// Sol : grand plan sous le circuit (comme track.ground = altitude min - 0,6 m)
	{
		const FBox B(Track->Points);
		const FVector C = B.GetCenter();
		const float S = FMath::Max(B.GetExtent().X, B.GetExtent().Y) + 60000.f;   // 600 m de marge
		const float Z = MinZ - 800.f;   // sous le relief du décor (GPMScenery)
		const int32 a = Floor.Add(FVector(C.X - S, C.Y - S, Z), Up, FVector2D(0, 0), Ground);
		Floor.Add(FVector(C.X + S, C.Y - S, Z), Up, FVector2D(1, 0), Ground);
		Floor.Add(FVector(C.X - S, C.Y + S, Z), Up, FVector2D(0, 1), Ground);
		Floor.Add(FVector(C.X + S, C.Y + S, Z), Up, FVector2D(1, 1), Ground);
		Floor.Quad(a, a + 1, a + 2, a + 3);
	}

	// Sens des triangles : Unreal calcule la normale d'une face comme -(B-A)x(C-A) (voir KismetProceduralMeshLibrary).
	// On oriente chaque triangle selon la normale voulue de ses sommets, quel que soit le sens de rotation du circuit.
	auto FixWinding = [](FSection& S)
	{
		for (int32 k = 0; k + 2 < S.I.Num(); k += 3)
		{
			const FVector A = S.V[S.I[k]], B = S.V[S.I[k + 1]], C = S.V[S.I[k + 2]];
			if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), S.N[S.I[k]]) > 0.f) Swap(S.I[k + 1], S.I[k + 2]);
		}
	};
	for (FSection* S : { &Road, &Kerbs, &Green, &Walls, &Floor, &Start, &Pit }) FixWinding(*S);

	const TArray<FProcMeshTangent> NoTangents;
	auto Make = [&](int32 Index, FSection& S, UMaterialInterface* M, bool bCollide)
	{
		Mesh->CreateMeshSection_LinearColor(Index, S.V, S.I, S.N, S.UV, S.Col, NoTangents, bCollide);
		if (M) Mesh->SetMaterial(Index, M);
	};
	Make(0, Road, AsphaltMaterial, true);
	Make(1, Kerbs, KerbMaterial, false);
	Make(2, Green, GrassMaterial, true);
	Make(3, Walls, WallMaterial, true);
	Make(4, Floor, GrassMaterial, true);
	Make(5, Start, KerbMaterial, false);
	Make(6, Pit, AsphaltMaterial, false);

	UE_LOG(LogGPM, Log, TEXT("Circuit %s construit : %d points, %d triangles"), *Track->Name, N,
		(Road.I.Num() + Kerbs.I.Num() + Green.I.Num() + Walls.I.Num() + 6) / 3);
	return true;
}

FTransform AGPMTrack::GetStartTransform() const
{
	if (!Track || Track->Num() < 2) return GetActorTransform();
	const FVector P = Track->Points[0];
	const FVector Dir = (Track->Points[1] - P).GetSafeNormal();
	return FTransform(Dir.Rotation(), P);
}
