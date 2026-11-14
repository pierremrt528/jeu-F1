#include "GPMTrack.h"
#include "GPMDataSubsystem.h"
#include "GrandPrixManager.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	const FLinearColor Ground(0.13f, 0.30f, 0.09f);
}

namespace GPMTrackLook
{
	// barrières : béton sali en bas, chaperon clair, poteaux du grillage
	const FLinearColor Dirty(0.3f, 0.3f, 0.31f);
	const FLinearColor Cap(0.7f, 0.7f, 0.71f);
	const FLinearColor Post(0.16f, 0.17f, 0.18f);
	// panneaux publicitaires de marques inventées : fond et bandeau du logo
	constexpr int32 NumAds = 8;
	const FLinearColor AdBg[NumAds] = { FLinearColor(0.75f, 0.05f, 0.05f), FLinearColor(0.02f, 0.05f, 0.22f), FLinearColor(0.95f, 0.72f, 0.04f),
		FLinearColor(0.03f, 0.03f, 0.035f), FLinearColor(0.04f, 0.32f, 0.12f), FLinearColor(0.92f, 0.38f, 0.03f), FLinearColor(0.9f, 0.9f, 0.9f),
		FLinearColor(0.06f, 0.28f, 0.75f) };
	const FLinearColor AdInk[NumAds] = { FLinearColor(0.95f, 0.95f, 0.95f), FLinearColor(0.95f, 0.78f, 0.1f), FLinearColor(0.03f, 0.03f, 0.04f),
		FLinearColor(0.95f, 0.2f, 0.15f), FLinearColor(0.95f, 0.95f, 0.95f), FLinearColor(0.97f, 0.97f, 0.97f), FLinearColor(0.75f, 0.05f, 0.05f),
		FLinearColor(0.95f, 0.95f, 0.95f) };
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

	// Vrais matériaux (Tools/gpm_assets.py) : asphalte et bas-côtés en photo, vibreurs et murs éclairés.
	// Sans eux : couleurs par sommet. Avec : sommets blancs (la photo donne la couleur), bandes de tonte en nuances.
	const FString Verge = Track->Theme.Surface == TEXT("sand") ? FString(TEXT("sand")) : Track->Theme.Surface == TEXT("concrete") ? FString(TEXT("concrete")) : FString(TEXT("grass"));
	UMaterialInstanceDynamic* RoadM = GPMArt::Surface(TEXT("asphalt"), 6.f, this);
	UMaterialInstanceDynamic* GreenM = GPMArt::Surface(Verge, 9.f, this);
	UMaterialInstanceDynamic* FloorM = GPMArt::Surface(Verge, 30.f, this);
	UMaterialInstanceDynamic* KerbM = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	UMaterialInstanceDynamic* WallM = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	const bool bArt = RoadM && GreenM && FloorM && KerbM && WallM;
	if (bArt)
	{
		KerbM->SetScalarParameterValue(TEXT("Rough"), 0.5f);
		WallM->SetScalarParameterValue(TEXT("Rough"), 0.8f);
	}
	RoadMID = bArt ? RoadM : nullptr;
	GreenMID = bArt ? GreenM : nullptr;
	KerbMID = bArt ? KerbM : nullptr;
	const FLinearColor RoadC = bArt ? FLinearColor::White : Asphalt;
	const FLinearColor GroundC = bArt ? FLinearColor::White : Ground;
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

		// Asphalte : 7 sommets en travers. Bords un peu poussiéreux (plus clairs), trajectoire idéale gommée (plus sombre,
		// surtout dans les virages) comme sur un vrai circuit ; la couleur du sommet multiplie la photo d'asphalte.
		{
			const float Lim = FMath::Max(0.f, Half - 340.f);
			const float Rc = FMath::Clamp((Track->RL.IsValidIndex(j) ? Track->RL[j] : 0.f) * 85.f, -Lim, Lim);
			const float Bw = FMath::Clamp(Half - 80.f - FMath::Abs(Rc), 10.f, 260.f);
			const float Cv = Track->Curvature.IsValidIndex(j) ? FMath::Abs(Track->Curvature[j]) : 0.f;
			const float Rub = 0.8f - 0.12f * FMath::Clamp(Cv * 40.f, 0.f, 1.f);
			const float Off[7] = { -Half, -Half + 70.f, Rc - Bw, Rc, Rc + Bw, Half - 70.f, Half };
			const float Shade[7] = { 1.07f, 1.f, 1.f, Rub, 1.f, 1.f, 1.07f };
			for (int32 k = 0; k < 7; ++k)
				Road.Add(P + R * Off[k], Up, FVector2D(k / 6.f, U), FLinearColor(RoadC.R * Shade[k], RoadC.G * Shade[k], RoadC.B * Shade[k], 1.f));
			if (i > 0) { const int32 b = Road.V.Num() - 14; for (int32 k = 0; k < 6; ++k) Road.Quad(b + k, b + k + 1, b + 7 + k, b + 8 + k); }
		}

		// Herbe de chaque côté, du bord de la piste jusqu'au mur (légèrement plus basse), bandes de tonte tous les 10 m
		const float DL = WallDist(Track->WallLeft, j), DR = WallDist(Track->WallRight, j);
		const FVector Low = -Up * 3.f;
		const FLinearColor GrassC = bArt ? ((i / 5) % 2 ? FLinearColor(0.8f, 0.8f, 0.8f) : FLinearColor::White)
			: ((i / 5) % 2 ? Grass : Grass * 1.18f);
		Green.Add(P - R * DL + Low, Up, FVector2D(0, U), GrassC);
		Green.Add(P - R * Half + Low, Up, FVector2D(1, U), GrassC);
		Green.Add(P + R * Half + Low, Up, FVector2D(0, U), GrassC);
		Green.Add(P + R * DR + Low, Up, FVector2D(1, U), GrassC);
		if (i > 0) { const int32 b = Green.V.Num() - 8; Green.Quad(b, b + 1, b + 4, b + 5); Green.Quad(b + 2, b + 3, b + 6, b + 7); }
	}

	// Murs : barrière de béton (soubassement sali, bandeau publicitaire de marques inventées, chaperon clair),
	// grillage de sécurité au-dessus avec ses poteaux tous les 6 m. Un panneau tous les 24 m.
	UMaterialInstanceDynamic* FenceM = bArt ? GPMArt::Make(TEXT("M_GPM_Fence"), this) : nullptr;
	FSection Fence;
	const float FenceH = 420.f;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 j = (i + 1) % N;
		const int32 PKi = Track->PitK(i), PKj = Track->PitK(j);
		const bool bPitOpen = (PKi >= Track->PitE0 && PKi <= Track->PitX1) || (PKj >= Track->PitE0 && PKj <= Track->PitX1);   // mur droit ouvert le long des stands
		for (int32 Sd = 0; Sd < 2; ++Sd)
		{
			if (Sd == 1 && bPitOpen) continue;
			const float Side = Sd ? 1.f : -1.f;
			const TArray<float>& WD = Sd ? Track->WallRight : Track->WallLeft;
			const FVector Ri = Right(i) * Side, Rj = Right(j) * Side;
			const FVector Wi = Track->Points[i] + Ri * WallDist(WD, i), Wj = Track->Points[j] + Rj * WallDist(WD, j);
			const FVector Nf = -(Ri + Rj).GetSafeNormal();   // face tournée vers la piste
			const int32 Brand = (i / 12 + Sd * 3) % GPMTrackLook::NumAds;
			const bool bLogo = (i % 12) >= 3 && (i % 12) <= 8;
			auto Band = [&](FSection& S, float Z0, float Z1, const FLinearColor& Col, float Out, float U0, float U1)
			{
				const FVector Oi = Wi + Nf * Out, Oj = Wj + Nf * Out;
				const int32 a = S.Add(Oi + Up * Z0, Nf, FVector2D(U0, Z0 / 100.f), Col);
				S.Add(Oi + Up * Z1, Nf, FVector2D(U0, Z1 / 100.f), Col);
				S.Add(Oj + Up * Z0, Nf, FVector2D(U1, Z0 / 100.f), Col);
				S.Add(Oj + Up * Z1, Nf, FVector2D(U1, Z1 / 100.f), Col);
				S.Quad(a, a + 1, a + 2, a + 3);
			};
			Band(Walls, 0.f, 16.f, GPMTrackLook::Dirty, 0.f, 0.f, 1.f);
			Band(Walls, 16.f, 38.f, GPMTrackLook::AdBg[Brand], 0.f, 0.f, 1.f);
			Band(Walls, 38.f, 66.f, bLogo ? GPMTrackLook::AdInk[Brand] : GPMTrackLook::AdBg[Brand], 0.f, 0.f, 1.f);
			Band(Walls, 66.f, 88.f, GPMTrackLook::AdBg[Brand], 0.f, 0.f, 1.f);
			Band(Walls, 88.f, WallH, GPMTrackLook::Cap, 0.f, 0.f, 1.f);
			// chaperon : dessus de la barrière (30 cm de large)
			{
				const FVector Ti = Wi + Up * WallH, Tj = Wj + Up * WallH;
				const int32 a = Walls.Add(Ti, Up, FVector2D(0, 0), GPMTrackLook::Cap);
				Walls.Add(Ti - Nf * 30.f, Up, FVector2D(1, 0), GPMTrackLook::Cap);
				Walls.Add(Tj, Up, FVector2D(0, 1), GPMTrackLook::Cap);
				Walls.Add(Tj - Nf * 30.f, Up, FVector2D(1, 1), GPMTrackLook::Cap);
				Walls.Quad(a, a + 1, a + 2, a + 3);
			}
			if (!FenceM) continue;
			// grillage (mailles calculées par le matériau, UV en mètres) et sa lisse du haut
			Band(Fence, WallH, FenceH, FLinearColor::White, -10.f, i * 2.f, (i + 1) * 2.f);
			Band(Walls, FenceH - 6.f, FenceH, GPMTrackLook::Post, -8.f, 0.f, 1.f);
			if (i % 3 == 0)
			{
				const FVector Tg = (Wj - Wi).GetSafeNormal() * 4.f, Base = Wi - Nf * 8.f + Up * WallH;
				const int32 a = Walls.Add(Base - Tg, Nf, FVector2D(0, 0), GPMTrackLook::Post);
				Walls.Add(Base - Tg + Up * (FenceH - WallH + 8.f), Nf, FVector2D(0, 1), GPMTrackLook::Post);
				Walls.Add(Base + Tg, Nf, FVector2D(1, 0), GPMTrackLook::Post);
				Walls.Add(Base + Tg + Up * (FenceH - WallH + 8.f), Nf, FVector2D(1, 1), GPMTrackLook::Post);
				Walls.Quad(a, a + 1, a + 2, a + 3);
			}
		}
	}

	// Vibreurs : rouge et blanc alternés tous les 2 m là où la courbure dépasse le seuil, avec un léger dévers
	// (1 cm au bord de la piste, 6 cm à l'extérieur) et un petit rebord, pour accrocher la lumière
	for (int32 i = 0; i < N; ++i)
	{
		if (!Kerb(i)) continue;
		const int32 j = (i + 1) % N;
		const FLinearColor C = i % 2 ? KerbRed : KerbWhite;
		for (float Side : { -1.f, 1.f })
		{
			const FVector Ri = Right(i) * Side, Rj = Right(j) * Side;
			const FVector Nrm = (Up - (Ri + Rj).GetSafeNormal() * 0.045f).GetSafeNormal();
			const FVector A0 = Track->Points[i] + Ri * Half + Up * 1.f, A1 = Track->Points[i] + Ri * (Half + KerbW) + Up * 6.f;
			const FVector B0 = Track->Points[j] + Rj * Half + Up * 1.f, B1 = Track->Points[j] + Rj * (Half + KerbW) + Up * 6.f;
			const int32 a = Kerbs.Add(A0, Nrm, FVector2D(0, 0), C);
			Kerbs.Add(A1, Nrm, FVector2D(1, 0), C);
			Kerbs.Add(B0, Nrm, FVector2D(0, 1), C);
			Kerbs.Add(B1, Nrm, FVector2D(1, 1), C);
			Kerbs.Quad(a, a + 1, a + 2, a + 3);
			const FVector Ro = (Ri + Rj).GetSafeNormal();
			const int32 e = Kerbs.Add(A1, Ro, FVector2D(0, 0), C);
			Kerbs.Add(A1 - Up * 10.f, Ro, FVector2D(1, 0), C);
			Kerbs.Add(B1, Ro, FVector2D(0, 1), C);
			Kerbs.Add(B1 - Up * 10.f, Ro, FVector2D(1, 1), C);
			Kerbs.Quad(e, e + 1, e + 2, e + 3);
		}
	}

	// Lignes blanches peintes au bord de la piste (limites de piste), sur tout le tour
	for (float Side : { -1.f, 1.f })
		for (int32 i = 0; i <= N; ++i)
		{
			const int32 j = i % N;
			const FVector P = Track->Points[j] + Up * 1.5f, R = Right(j) * Side;
			Start.Add(P + R * (Half - 24.f), Up, FVector2D(0, i), KerbWhite);
			Start.Add(P + R * (Half - 10.f), Up, FVector2D(1, i), KerbWhite);
			if (i > 0) { const int32 b = Start.V.Num() - 4; Start.Quad(b, b + 1, b + 2, b + 3); }
		}

	// Voie des stands : bande d'asphalte de 8 m qui s'écarte de la piste (côté droit), plus des marques blanches aux boxes
	for (int32 K = Track->PitE0; K <= Track->PitX1; ++K)
	{
		const int32 i = ((K % N) + N) % N;
		const FVector P = Track->Points[i] + Up * 2.f, R = Right(i);
		const float L = Track->PitLatAt(K) * 100.f;
		const bool bMark = false;   // emplacements : carrés des écuries ci-dessous
		Pit.Add(P + R * (L - 400.f), Up, FVector2D(0, K / 5.f), bArt ? FLinearColor::White : bMark ? KerbWhite : Asphalt);
		Pit.Add(P + R * (L + 400.f), Up, FVector2D(1, K / 5.f), bArt ? FLinearColor::White : bMark ? KerbWhite : Asphalt);
		if (bArt && bMark)
		{
			// marque blanche peinte au sol (avec la photo d'asphalte, une couleur de sommet ne suffit plus)
			const FVector Tg = FVector(R.Y, -R.X, 0.f) * 20.f, Lift = Up * 1.f;
			const int32 a = Start.Add(P + R * (L - 400.f) - Tg + Lift, Up, FVector2D(0, 0), KerbWhite);
			Start.Add(P + R * (L + 400.f) - Tg + Lift, Up, FVector2D(1, 0), KerbWhite);
			Start.Add(P + R * (L - 400.f) + Tg + Lift, Up, FVector2D(0, 1), KerbWhite);
			Start.Add(P + R * (L + 400.f) + Tg + Lift, Up, FVector2D(1, 1), KerbWhite);
			Start.Quad(a, a + 1, a + 2, a + 3);
		}
		if (K > Track->PitE0) { const int32 b = Pit.V.Num() - 4; Pit.Quad(b, b + 1, b + 2, b + 3); }
	}

	// Carrés des écuries dans la voie des stands (P.slot de track.js) : couleur de l'écurie, cadre blanc, jaune pour le joueur
	if (Data)
		for (int32 t = 0; t < Data->Teams.Num(); ++t)
		{
			const float Kc = FGPMTrackData::PitBox(t);
			FLinearColor TC = Data->Teams[t].Color; TC.A = 1.f;
			const FLinearColor Frame = t == 0 ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor::White;
			const float E = t == 0 ? 30.f : 15.f;
			auto Rect = [&](float KA, float KB, float LA, float LB, float Z, const FLinearColor& C)
			{
				for (float K = KA; K < KB - 0.01f; K += 1.f)
				{
					const float K2 = FMath::Min(K + 1.f, KB);
					auto At = [&](float Kf, float Lcm)
					{
						const float F = FMath::Fmod(FMath::Fmod(Kf, float(N)) + N, float(N));
						const int32 i = FMath::FloorToInt(F) % N, j = (i + 1) % N;
						const float Fr = F - FMath::FloorToFloat(F);
						const FVector P = FMath::Lerp(Track->Points[i], Track->Points[j], Fr);
						const FVector R = FMath::Lerp(Right(i), Right(j), Fr).GetSafeNormal();
						return P + R * Lcm + Up * Z;
					};
					const int32 q = Start.Add(At(K, LA), Up, FVector2D(0, 0), C);
					Start.Add(At(K, LB), Up, FVector2D(1, 0), C);
					Start.Add(At(K2, LA), Up, FVector2D(0, 1), C);
					Start.Add(At(K2, LB), Up, FVector2D(1, 1), C);
					Start.Quad(q, q + 1, q + 2, q + 3);
				}
			};
			const float L0 = Track->SlotL0() * 100.f, L1 = Track->SlotL1() * 100.f, Ek = E / 200.f;
			Rect(Kc - 2.f, Kc + 2.f, L0, L1, 2.6f, TC);
			Rect(Kc - 2.f, Kc + 2.f, L0 - E, L0, 2.9f, Frame);
			Rect(Kc - 2.f, Kc + 2.f, L1, L1 + E, 2.9f, Frame);
			Rect(Kc - 2.f, Kc - 2.f + Ek, L0 - E, L1 + E, 2.95f, Frame);
			Rect(Kc + 2.f - Ek, Kc + 2.f, L0 - E, L1 + E, 2.95f, Frame);
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
		const int32 a = Floor.Add(FVector(C.X - S, C.Y - S, Z), Up, FVector2D(0, 0), GroundC);
		Floor.Add(FVector(C.X + S, C.Y - S, Z), Up, FVector2D(1, 0), GroundC);
		Floor.Add(FVector(C.X - S, C.Y + S, Z), Up, FVector2D(0, 1), GroundC);
		Floor.Add(FVector(C.X + S, C.Y + S, Z), Up, FVector2D(1, 1), GroundC);
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
	for (FSection* S : { &Road, &Kerbs, &Green, &Walls, &Floor, &Start, &Pit, &Fence }) FixWinding(*S);

	const TArray<FProcMeshTangent> NoTangents;
	auto Make = [&](int32 Index, FSection& S, UMaterialInterface* M, bool bCollide)
	{
		Mesh->CreateMeshSection_LinearColor(Index, S.V, S.I, S.N, S.UV, S.Col, NoTangents, bCollide);
		if (M) Mesh->SetMaterial(Index, M);
	};
	Make(0, Road, bArt ? RoadM : AsphaltMaterial.Get(), true);
	Make(1, Kerbs, bArt ? KerbM : KerbMaterial.Get(), false);
	Make(2, Green, bArt ? GreenM : GrassMaterial.Get(), true);
	Make(3, Walls, bArt ? WallM : WallMaterial.Get(), true);
	Make(4, Floor, bArt ? FloorM : GrassMaterial.Get(), true);
	Make(5, Start, bArt ? KerbM : KerbMaterial.Get(), false);
	Make(6, Pit, bArt ? RoadM : AsphaltMaterial.Get(), false);
	if (FenceM && Fence.V.Num()) Make(7, Fence, FenceM, false);

	UE_LOG(LogGPM, Log, TEXT("Circuit %s construit : %d points, %d triangles"), *Track->Name, N,
		(Road.I.Num() + Kerbs.I.Num() + Green.I.Num() + Walls.I.Num() + 6) / 3);
	return true;
}

void AGPMTrack::SetWet(float Wet)
{
	const float K = FMath::Clamp(Wet, 0.f, 1.f);
	if (RoadMID) RoadMID->SetScalarParameterValue(TEXT("Wet"), K);
	// herbe un peu plus sombre, vibreurs brillants
	if (GreenMID) GreenMID->SetScalarParameterValue(TEXT("Wet"), K * 0.5f);
	if (KerbMID) KerbMID->SetScalarParameterValue(TEXT("Rough"), 0.5f - 0.38f * K);
}

FTransform AGPMTrack::GetStartTransform() const
{
	if (!Track || Track->Num() < 2) return GetActorTransform();
	const FVector P = Track->Points[0];
	const FVector Dir = (Track->Points[1] - P).GetSafeNormal();
	return FTransform(Dir.Rotation(), P);
}
