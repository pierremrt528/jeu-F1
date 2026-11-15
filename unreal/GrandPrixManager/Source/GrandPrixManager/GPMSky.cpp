#include "GPMSky.h"
#include "GrandPrixManager.h"
#include "GPMTypes.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace GPMSkyGeo
{
	constexpr float DomeR = 3600000.f;            // 36 km : derrière tout le reste
	constexpr float CloudR = DomeR * 0.93f;
	constexpr float SunR = DomeR * 0.96f;
	constexpr float RidgeR = 2200000.f;           // 22 km : chaînes de montagnes, toujours au-delà de la coupure du brouillard
	const FLinearColor SunWarm(1.f, 0.88f, 0.68f);

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, float T) { return A + (B - A) * T; }
	FLinearColor Clamp1(const FLinearColor& C) { return FLinearColor(FMath::Min(C.R, 1.f), FMath::Min(C.G, 1.f), FMath::Min(C.B, 1.f), 1.f); }

	// mulberry32 de scenery.js (phases des crêtes)
	float Rand01(uint32& State)
	{
		State += 0x6D2B79F5u;
		uint32 T = State;
		T = (T ^ (T >> 15)) * (T | 1u);
		T ^= T + (T ^ (T >> 7)) * (T | 61u);
		return float((T ^ (T >> 14)) >> 8) / 16777216.f;
	}

	// faces tournées vers l'intérieur (normale de face Unreal = −(B−A)×(C−A))
	void FaceInward(const TArray<FVector>& V, TArray<int32>& I, const FVector& Center)
	{
		for (int32 k = 0; k + 2 < I.Num(); k += 3)
		{
			const FVector& A = V[I[k]];
			const FVector Front = -FVector::CrossProduct(V[I[k + 1]] - A, V[I[k + 2]] - A);
			const FVector Mid = (A + V[I[k + 1]] + V[I[k + 2]]) / 3.0;
			if (FVector::DotProduct(Front, Center - Mid) < 0.0) Swap(I[k + 1], I[k + 2]);
		}
	}
}

AGPMSky::AGPMSky()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	auto MakePart = [this](const TCHAR* PartName)
	{
		UProceduralMeshComponent* P = CreateDefaultSubobject<UProceduralMeshComponent>(PartName);
		P->SetupAttachment(RootComponent);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->CastShadow = false;
		P->bNeverDistanceCull = true;
		P->bAffectDistanceFieldLighting = false;
		return P;
	};
	Dome = MakePart(TEXT("Dome"));
	CloudLayer = MakePart(TEXT("Nuages"));
	Ridges = MakePart(TEXT("Montagnes"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VC(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VC.Succeeded()) VertexMat = VC.Object;
}

void AGPMSky::Build(const FGPMTrackData* InTrack)
{
	if (!InTrack || InTrack->Points.Num() < 4) return;
	const FGPMTheme& Th = InTrack->Theme;
	const FBox B(InTrack->Points);
	// centre du circuit, au niveau du point le plus bas (comme track.ground du jeu web)
	SetActorLocation(FVector(B.GetCenter().X, B.GetCenter().Y, B.Min.Z - 3000.0));
	if (!SkyMID)
	{
		SkyMID = GPMArt::Make(TEXT("M_GPM_Sky"), this);
		SunMID = GPMArt::Make(TEXT("M_GPM_Sky"), this);
		CloudMID = GPMArt::Make(TEXT("M_GPM_Clouds"), this);
	}
	BuildDome(Th, Th.Sun.GetSafeNormal());
	// orientation des nuages propre à chaque circuit (rotation.y = longueur % 628 / 100 dans le jeu web)
	BuildClouds(FMath::Fmod(InTrack->Length, 628.f) / 100.f);
	BuildRidges(InTrack, float(FMath::Max(B.GetSize().X, B.GetSize().Y)) / 100.f);
	SetGloom(0.f);
}

// Dôme : dégradé horizon → zénith (pow 0,55 comme skyGeo), halo chaud autour du soleil, dessous un peu plus sombre
void AGPMSky::BuildDome(const FGPMTheme& Th, const FVector& SunDir)
{
	const int32 NA = 96, NE = 48;
	TArray<FVector> V, N; TArray<FLinearColor> Col; TArray<int32> I;
	V.Reserve((NA + 1) * (NE + 1)); N.Reserve(V.Max()); Col.Reserve(V.Max());
	for (int32 e = 0; e <= NE; ++e)
	{
		const float El = -0.5f * PI + PI * e / NE;
		const float CE = FMath::Cos(El), SE = FMath::Sin(El);
		for (int32 a = 0; a <= NA; ++a)
		{
			const float Az = 2.f * PI * a / NA;
			const FVector D(CE * FMath::Cos(Az), CE * FMath::Sin(Az), SE);
			V.Add(D * GPMSkyGeo::DomeR);
			N.Add(-D);
			FLinearColor C = SE > 0.f ? GPMSkyGeo::Mix(Th.Horizon, Th.Zenith, FMath::Pow(SE, 0.55f)) : Th.Horizon * (1.f + 0.1f * SE);
			const float S = FMath::Max(0.f, float(FVector::DotProduct(D, SunDir)));
			C += GPMSkyGeo::SunWarm * (0.35f * FMath::Pow(S, 6.f) + 0.45f * FMath::Pow(S, 40.f));
			Col.Add(GPMSkyGeo::Clamp1(C));
		}
	}
	for (int32 e = 0; e < NE; ++e)
		for (int32 a = 0; a < NA; ++a)
		{
			const int32 P0 = e * (NA + 1) + a, P1 = P0 + 1, P2 = P0 + NA + 1, P3 = P2 + 1;
			if (e > 0) I.Append({ P0, P1, P2 });
			if (e < NE - 1) I.Append({ P1, P3, P2 });
		}
	GPMSkyGeo::FaceInward(V, I, FVector::ZeroVector);
	Dome->CreateMeshSection_LinearColor(0, V, I, N, TArray<FVector2D>(), Col, TArray<FProcMeshTangent>(), false);
	Dome->SetMaterial(0, SkyMID ? SkyMID.Get() : VertexMat.Get());

	// disque du soleil (très lumineux : le bloom fait le reste), seulement s'il est au-dessus de l'horizon
	if (SunDir.Z > 0.03)
	{
		TArray<FVector> SV, SN; TArray<FLinearColor> SC; TArray<int32> SI;
		const FVector Ctr = SunDir * GPMSkyGeo::SunR;
		FVector Ax, Ay;
		SunDir.FindBestAxisVectors(Ax, Ay);
		const float Rad = GPMSkyGeo::SunR * FMath::Tan(FMath::DegreesToRadians(0.9f));
		SV.Add(Ctr); SN.Add(-SunDir); SC.Add(FLinearColor(1.f, 0.98f, 0.92f));
		for (int32 k = 0; k <= 24; ++k)
		{
			const float A = 2.f * PI * k / 24;
			SV.Add(Ctr + (Ax * FMath::Cos(A) + Ay * FMath::Sin(A)) * Rad);
			SN.Add(-SunDir);
			SC.Add(FLinearColor(1.f, 0.9f, 0.72f));
			if (k < 24) SI.Append({ 0, k + 1, k + 2 });
		}
		GPMSkyGeo::FaceInward(SV, SI, FVector::ZeroVector);
		Dome->CreateMeshSection_LinearColor(1, SV, SI, SN, TArray<FVector2D>(), SC, TArray<FProcMeshTangent>(), false);
		Dome->SetMaterial(1, SunMID ? SunMID.Get() : VertexMat.Get());
	}
	else if (Dome->GetNumSections() > 1) Dome->ClearMeshSection(1);
}

// Nuages : demi-sphère texturée (photo d'un ciel HDRI : haut de l'image au zénith, bas à l'horizon), translucide
void AGPMSky::BuildClouds(float Yaw)
{
	CloudLayer->ClearAllMeshSections();
	if (!CloudMID) return;   // sans les textures importées par Tools/gpm_assets.py : dégradé seul
	const int32 NA = 48, NE = 12;
	TArray<FVector> V, N; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<int32> I;
	for (int32 e = 0; e <= NE; ++e)
	{
		const float El = 0.5f * PI * e / NE;
		const float CE = FMath::Cos(El), SE = FMath::Sin(El);
		for (int32 a = 0; a <= NA; ++a)
		{
			const float Az = Yaw + 2.f * PI * a / NA;
			const FVector D(CE * FMath::Cos(Az), CE * FMath::Sin(Az), SE);
			V.Add(D * GPMSkyGeo::CloudR);
			N.Add(-D);
			UV.Add(FVector2D(float(a) / NA, 1.f - float(e) / NE));
			Col.Add(FLinearColor::White);
		}
	}
	for (int32 e = 0; e < NE; ++e)
		for (int32 a = 0; a < NA; ++a)
		{
			const int32 P0 = e * (NA + 1) + a, P1 = P0 + 1, P2 = P0 + NA + 1, P3 = P2 + 1;
			I.Append({ P0, P1, P2 });
			if (e < NE - 1) I.Append({ P1, P3, P2 });
		}
	GPMSkyGeo::FaceInward(V, I, FVector::ZeroVector);
	CloudLayer->CreateMeshSection_LinearColor(0, V, I, N, UV, Col, TArray<FProcMeshTangent>(), false);
	CloudLayer->SetMaterial(0, CloudMID);
	CloudLayer->SetTranslucentSortPriority(-10);   // avant la pluie, les vitres et les fumées
}

// Montagnes : deux crêtes bruitées tout autour (ridge() de scenery.js), du pied (couleur de base) aux sommets
// (vers la couleur du haut, neige en montagne). Placées bien plus loin que dans le jeu web, mais agrandies d'autant :
// même taille à l'écran, et jamais dans le brouillard (leurs couleurs sont déjà celles du lointain).
void AGPMSky::BuildRidges(const FGPMTrackData* InTrack, float SizeM)
{
	const FGPMTheme& Th = InTrack->Theme;
	const float RR = FMath::Min(2350.f, (SizeM + 3000.f) / 2.f + 600.f);   // rayon du jeu web (m)
	const float Sc = GPMSkyGeo::RidgeR / RR;                             // cm du monde par m du jeu web
	TArray<FVector> V, N; TArray<FLinearColor> Col; TArray<int32> I;
	auto AddRidge = [&](float R, float H0, float H1, const FLinearColor& Foot, const FLinearColor& Peak, uint32 Seed)
	{
		if (H1 <= 1.f) return;
		uint32 State = Seed;
		float Ph[4];
		for (float& P : Ph) P = GPMSkyGeo::Rand01(State) * 6.f;
		const int32 Segs = 160, O0 = V.Num();
		for (int32 k = 0; k <= Segs; ++k)
		{
			const float A = 2.f * PI * k / Segs;
			const float Nz = 0.5f + 0.25f * FMath::Sin(A * 3 + Ph[0]) + 0.15f * FMath::Sin(A * 7 + Ph[1]) + 0.07f * FMath::Sin(A * 17 + Ph[2]) + 0.04f * FMath::Sin(A * 31 + Ph[3]);
			const float Hgt = H0 + (H1 - H0) * Nz;
			const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
			V.Add(Dir * (R * Sc) + FVector(0, 0, -30.f * Sc));
			V.Add(Dir * ((R - 300.f) * Sc) + FVector(0, 0, Hgt * Sc));
			N.Add(-Dir); N.Add(-Dir);
			Col.Add(Foot);
			Col.Add(GPMSkyGeo::Mix(Foot, Peak, FMath::Min(1.f, Nz * 1.1f)));
			if (k < Segs)
			{
				const int32 o = O0 + k * 2;
				I.Append({ o, o + 2, o + 1, o + 1, o + 2, o + 3 });
			}
		}
	};
	const uint32 Var = uint32(InTrack->WX.Num());   // forme des crêtes propre à chaque circuit
	AddRidge(RR, Th.FarH0, Th.FarH1, Th.MountFar, Th.MountFarTop, 21u + Var);
	AddRidge(RR - 450.f, Th.NearH0, Th.NearH1, Th.MountNear, Th.MountNearTop, 33u + Var * 7u);
	Ridges->ClearAllMeshSections();
	if (I.Num() == 0) return;
	GPMSkyGeo::FaceInward(V, I, FVector::ZeroVector);
	Ridges->CreateMeshSection_LinearColor(0, V, I, N, TArray<FVector2D>(), Col, TArray<FProcMeshTangent>(), false);
	Ridges->SetMaterial(0, SkyMID ? SkyMID.Get() : VertexMat.Get());
}

void AGPMSky::SetGloom(float G)
{
	// ciel et montagnes vers le gris du ciel couvert, nuages plus denses et gris, soleil qui s'efface
	if (SkyMID) SkyMID->SetScalarParameterValue(TEXT("Gloom"), 0.85f * G);
	if (SunMID)
	{
		SunMID->SetScalarParameterValue(TEXT("Gloom"), G);
		SunMID->SetScalarParameterValue(TEXT("Bright"), 1.f + 6.f * FMath::Square(1.f - G));
	}
	if (CloudMID) CloudMID->SetScalarParameterValue(TEXT("Gloom"), G);
	if (Dome && Dome->GetNumSections() > 1) Dome->SetMeshSectionVisible(1, G < 0.9f);
}
