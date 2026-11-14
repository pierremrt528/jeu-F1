// Atelier 3D (showroom.js) : la voiture de l'écurie sur un plateau tournant, sol sombre et brillant, mur circulaire avec
// rampes lumineuses et panneau au nom de l'écurie, plafond à panneaux de lumière, deux personnages pour le rendez-vous
// et la signature. Tout est en géométrie générée (couleurs par sommet) : M_GPM_Lit éclairé, M_GPM_Sky pour ce qui brille.
#include "GPMShowroom.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

namespace GPMRoomGeo
{
	constexpr float WallR = 1700.f, WallH = 950.f, PlateR = 330.f, PlateH = 14.f;
	constexpr float BoardX = 1460.f;        // panneau du nom, devant le mur côté -X (face à la caméra des écrans)
	constexpr float SceneYaw = -12.f;       // cap de la voiture pendant les scènes : trois quarts avant pour la photo
	constexpr int32 FloorRings = 6;

	struct FRoomMesh
	{
		TArray<FVector> V, N;
		TArray<FLinearColor> C;
		TArray<int32> I;

		int32 Vert(const FVector& P, const FVector& Nm, const FLinearColor& Col) { V.Add(P); N.Add(Nm); C.Add(Col); return V.Num() - 1; }
		// triangles ajoutés depuis First : face visible du côté des normales (normale de face Unreal = −(B−A)×(C−A))
		void Orient(int32 First)
		{
			for (int32 k = First; k + 2 < I.Num(); k += 3)
			{
				const FVector& A = V[I[k]];
				const FVector Front = -FVector::CrossProduct(V[I[k + 1]] - A, V[I[k + 2]] - A);
				if (FVector::DotProduct(Front, N[I[k]] + N[I[k + 1]] + N[I[k + 2]]) < 0.0) Swap(I[k + 1], I[k + 2]);
			}
		}
		/** Quadrilatère plat (sommets dans l'ordre du pourtour), une couleur par sommet. */
		void Quad(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& Nm,
			const FLinearColor& C0, const FLinearColor& C1, const FLinearColor& C2, const FLinearColor& C3)
		{
			const int32 First = I.Num(), O = V.Num();
			Vert(P0, Nm, C0); Vert(P1, Nm, C1); Vert(P2, Nm, C2); Vert(P3, Nm, C3);
			I.Append({ O, O + 1, O + 2, O, O + 2, O + 3 });
			Orient(First);
		}
		void Box(const FVector& Ctr, const FVector& Half, const FLinearColor& Col)
		{
			const FVector X(Half.X, 0, 0), Y(0, Half.Y, 0), Z(0, 0, Half.Z);
			Quad(Ctr + X - Y - Z, Ctr + X + Y - Z, Ctr + X + Y + Z, Ctr + X - Y + Z, FVector(1, 0, 0), Col, Col, Col, Col);
			Quad(Ctr - X - Y - Z, Ctr - X + Y - Z, Ctr - X + Y + Z, Ctr - X - Y + Z, FVector(-1, 0, 0), Col, Col, Col, Col);
			Quad(Ctr - X + Y - Z, Ctr + X + Y - Z, Ctr + X + Y + Z, Ctr - X + Y + Z, FVector(0, 1, 0), Col, Col, Col, Col);
			Quad(Ctr - X - Y - Z, Ctr + X - Y - Z, Ctr + X - Y + Z, Ctr - X - Y + Z, FVector(0, -1, 0), Col, Col, Col, Col);
			Quad(Ctr - X - Y + Z, Ctr + X - Y + Z, Ctr + X + Y + Z, Ctr - X + Y + Z, FVector(0, 0, 1), Col, Col, Col, Col);
			Quad(Ctr - X - Y - Z, Ctr + X - Y - Z, Ctr + X + Y - Z, Ctr - X + Y - Z, FVector(0, 0, -1), Col, Col, Col, Col);
		}
		/** Cylindre de A à B (bouts fermés). */
		void Cyl(const FVector& A, const FVector& B, float R, int32 Seg, const FLinearColor& Col)
		{
			const FVector Ax = (B - A).GetSafeNormal();
			const FVector U = FVector::CrossProduct(Ax, FMath::Abs(Ax.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
			const FVector W = FVector::CrossProduct(Ax, U);
			const int32 First = I.Num(), O = V.Num();
			for (int32 k = 0; k < Seg; ++k)
			{
				const float An = 2.f * PI * k / Seg;
				const FVector Rd = U * FMath::Cos(An) + W * FMath::Sin(An);
				Vert(A + Rd * R, Rd, Col);
				Vert(B + Rd * R, Rd, Col);
			}
			for (int32 k = 0; k < Seg; ++k)
			{
				const int32 a = O + 2 * k, b = O + 2 * ((k + 1) % Seg);
				I.Append({ a, b, a + 1, b, b + 1, a + 1 });
			}
			const int32 Ca = Vert(A, -Ax, Col), Cb = Vert(B, Ax, Col), O2 = V.Num();
			for (int32 k = 0; k < Seg; ++k)
			{
				const float An = 2.f * PI * k / Seg;
				const FVector Rd = U * FMath::Cos(An) + W * FMath::Sin(An);
				Vert(A + Rd * R, -Ax, Col);
				Vert(B + Rd * R, Ax, Col);
			}
			for (int32 k = 0; k < Seg; ++k)
			{
				const int32 a = O2 + 2 * k, b = O2 + 2 * ((k + 1) % Seg);
				I.Append({ Ca, a, b, Cb, b + 1, a + 1 });
			}
			Orient(First);
		}
		/** Sphère (anneaux × pans), éventuellement aplatie (Squash sur Z). */
		void Ball(const FVector& Ctr, float R, const FLinearColor& Col, float Squash = 1.f, int32 Rings = 7, int32 Segs = 12)
		{
			const int32 First = I.Num(), O = V.Num();
			for (int32 r = 0; r <= Rings; ++r)
			{
				const float Th = PI * r / Rings;
				for (int32 s = 0; s <= Segs; ++s)
				{
					const float Ph = 2.f * PI * s / Segs;
					const FVector Nm(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
					Vert(Ctr + FVector(Nm.X * R, Nm.Y * R, Nm.Z * R * Squash), Nm, Col);
				}
			}
			for (int32 r = 0; r < Rings; ++r)
				for (int32 s = 0; s < Segs; ++s)
				{
					const int32 a = O + r * (Segs + 1) + s, b = a + Segs + 1;
					I.Append({ a, b, a + 1, a + 1, b, b + 1 });
				}
			Orient(First);
		}
		void Into(UProceduralMeshComponent* P, int32 Section) const
		{
			if (P) P->CreateMeshSection_LinearColor(Section, V, I, N, TArray<FVector2D>(), C, TArray<FProcMeshTangent>(), false);
		}
	};

	/** Couleur avec la rugosité dans l'alpha (M_GPM_Lit). */
	FLinearColor WithRough(const FLinearColor& Col, float R) { return FLinearColor(Col.R, Col.G, Col.B, R); }
	FLinearColor Blend(const FLinearColor& A, const FLinearColor& B, float T) { return A + (B - A) * T; }

	/** Couleur du sol à la distance Rad du centre (dégradé clair sous la voiture vers le noir du mur), comme le maillage du sol. */
	FLinearColor FloorRing(int32 r)
	{
		const FLinearColor Mid(0.2f, 0.22f, 0.26f), Edge(0.022f, 0.026f, 0.034f);
		return WithRough(Blend(Mid, Edge, FMath::Pow(float(r) / FloorRings, 0.6f)), 0.24f);
	}
	FLinearColor FloorAt(float Rad)
	{
		const float F = FMath::Clamp(Rad / WallR * FloorRings, 0.f, float(FloorRings));
		const int32 r = FMath::Min(FMath::FloorToInt(F), FloorRings - 1);
		return Blend(FloorRing(r), FloorRing(r + 1), F - r);
	}

	/** Nom de l'écurie pour le mur : majuscules sans accents (la police 3D n'est sûre que pour l'ASCII). */
	FString Plain(const FString& S)
	{
		static const TCHAR* const From = TEXT("àâäáãåçéèêëíìîïñóòôöõúùûüýÿÀÂÄÁÃÅÇÉÈÊËÍÌÎÏÑÓÒÔÖÕÚÙÛÜÝœŒæÆ");
		static const TCHAR* const To = TEXT("aaaaaaceeeeiiiinooooouuuuyyAAAAAACEEEEIIIINOOOOOUUUUYoOaA");
		FString Out;
		for (const TCHAR Ch : S)
		{
			const TCHAR* Hit = FCString::Strchr(From, Ch);
			Out.AppendChar(Hit ? To[Hit - From] : (Ch < 128 ? Ch : TCHAR('-')));
		}
		return Out.ToUpper();
	}
}

AGPMShowroom::AGPMShowroom()
{
	using namespace GPMRoomGeo;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	Room = MakePart(TEXT("Piece"), false);
	Glow = MakePart(TEXT("Lumieres"), false);
	Plate = MakePart(TEXT("Plateau"), true);
	FigA = MakePart(TEXT("PersoA"), true);
	FigB = MakePart(TEXT("PersoB"), true);
	ArmA = MakePart(TEXT("BrasA"), true);
	ArmB = MakePart(TEXT("BrasB"), true);
	ArmA->SetupAttachment(FigA);
	ArmB->SetupAttachment(FigB);
	ArmA->SetRelativeLocation(FVector(0.f, 23.f, 148.f));
	ArmB->SetRelativeLocation(FVector(0.f, 23.f, 148.f));

	// nom de l'écurie sur le panneau du fond (le texte regarde vers +X, vers la caméra des écrans)
	NameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NomEcurie"));
	NameText->SetupAttachment(RootComponent);
	NameText->SetHorizontalAlignment(EHTA_Center);
	NameText->SetVerticalAlignment(EVRTA_TextCenter);
	NameText->SetWorldSize(150.f);
	NameText->SetTextRenderColor(FColor(242, 245, 255));
	NameText->CastShadow = false;
	NameText->SetRelativeLocation(FVector(-(BoardX - 14.f), 0.f, 560.f));

	// projecteur au-dessus du plateau (ombre de la voiture), deux contre-jours colorés, un peu de remplissage côté caméra
	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Projecteur"));
	KeyLight->SetupAttachment(RootComponent);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeLocation(FVector(120.f, -60.f, WallH - 60.f));
	KeyLight->SetRelativeRotation(FRotator(-82.f, 180.f, 0.f));
	KeyLight->IntensityUnits = ELightUnits::Candelas;
	KeyLight->SetIntensity(850.f);
	KeyLight->SetAttenuationRadius(2600.f);
	KeyLight->SetInnerConeAngle(24.f);
	KeyLight->SetOuterConeAngle(48.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.88f));
	KeyLight->SetCastShadows(true);

	auto MakeLamp = [this](const TCHAR* LampName, const FVector& At, float Cd, float Radius)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(LampName);
		L->SetupAttachment(RootComponent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetRelativeLocation(At);
		L->IntensityUnits = ELightUnits::Candelas;
		L->SetIntensity(Cd);
		L->SetAttenuationRadius(Radius);
		L->SetCastShadows(false);
		return L;
	};
	RimA = MakeLamp(TEXT("ContreJourA"), FVector(-720.f, 640.f, 280.f), 460.f, 2000.f);
	RimB = MakeLamp(TEXT("ContreJourB"), FVector(-640.f, -720.f, 320.f), 420.f, 2000.f);
	FillLight = MakeLamp(TEXT("Remplissage"), FVector(900.f, -380.f, 780.f), 130.f, 2400.f);   // haut : pas de reflet éblouissant sur le sol
	RimB->SetLightColor(FLinearColor(0.62f, 0.76f, 1.f));
	FillLight->SetLightColor(FLinearColor(1.f, 0.97f, 0.92f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VC(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VC.Succeeded()) VertexMat = VC.Object;
}

UProceduralMeshComponent* AGPMShowroom::MakePart(const TCHAR* PartName, bool bShadow)
{
	UProceduralMeshComponent* P = CreateDefaultSubobject<UProceduralMeshComponent>(PartName);
	P->SetupAttachment(RootComponent);
	P->SetMobility(EComponentMobility::Movable);
	P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	P->CastShadow = bShadow;
	P->bAffectDistanceFieldLighting = false;
	return P;
}

UMaterialInterface* AGPMShowroom::Mat(UMaterialInstanceDynamic* M) const
{
	return M ? static_cast<UMaterialInterface*>(M) : VertexMat.Get();
}

void AGPMShowroom::Build()
{
	if (bBuilt) return;
	bBuilt = true;
	LitMID = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	if (LitMID) { LitMID->SetScalarParameterValue(TEXT("Rough"), 1.f); LitMID->SetScalarParameterValue(TEXT("Metal"), 0.f); }
	GlowMID = GPMArt::Make(TEXT("M_GPM_Sky"), this);
	if (GlowMID) GlowMID->SetScalarParameterValue(TEXT("Bright"), 2.4f);
	AccentMID = GPMArt::Make(TEXT("M_GPM_Sky"), this);
	if (AccentMID) AccentMID->SetScalarParameterValue(TEXT("Bright"), 3.f);
	BuildRoom();
	BuildGlow();
	BuildPlate();
	SetCast(FGPMOutfit(), Outfit(FLinearColor(0.08f, 0.09f, 0.11f), FLinearColor(0.5f, 0.07f, 0.09f), TEXT("agent"), true));
	FigA->SetVisibility(false, true);
	FigB->SetVisibility(false, true);
	// voiture d'exposition : une vraie monoplace du jeu, sans circuit (elle ne roule pas)
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Display = GetWorld()->SpawnActor<AGPMCar>(Spot() + FVector(0, 0, GPMRoomGeo::PlateH), FRotator::ZeroRotator, SP);
	if (Display) Display->SetActorTickEnabled(false);
	SetTeam(TeamMain, TeamAccent, TeamLabel);
}

void AGPMShowroom::BuildRoom()
{
	using namespace GPMRoomGeo;
	FRoomMesh M;
	// sol : résine sombre, plus claire sous la voiture, très brillante (alpha = rugosité)
	const int32 Segs = 48;
	for (int32 r = 0; r < FloorRings; ++r)
	{
		const float Ra = WallR * r / FloorRings, Rb = WallR * (r + 1) / FloorRings;
		const FLinearColor C0 = FloorRing(r), C1 = FloorRing(r + 1);
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A0 = 2.f * PI * s / Segs, A1 = 2.f * PI * (s + 1) / Segs;
			const FVector P00(FMath::Cos(A0) * Ra, FMath::Sin(A0) * Ra, 0), P01(FMath::Cos(A1) * Ra, FMath::Sin(A1) * Ra, 0);
			const FVector P10(FMath::Cos(A0) * Rb, FMath::Sin(A0) * Rb, 0), P11(FMath::Cos(A1) * Rb, FMath::Sin(A1) * Rb, 0);
			M.Quad(P00, P01, P11, P10, FVector::UpVector, C0, C0, C1, C1);
		}
	}
	// joints des dalles : fines lignes un peu plus claires (pas sous le plateau ni sous son halo)
	const FLinearColor Joint = WithRough(FLinearColor(0.06f, 0.068f, 0.085f), 0.3f);
	for (int32 k = -8; k <= 8; ++k)
	{
		const float Pos = k * 200.f, Len = FMath::Sqrt(FMath::Max(0.f, WallR * WallR - Pos * Pos)) - 20.f;
		if (Len <= 0.f || FMath::Abs(Pos) < PlateR + 160.f) continue;
		M.Quad(FVector(Pos - 1.5f, -Len, 0.4f), FVector(Pos + 1.5f, -Len, 0.4f), FVector(Pos + 1.5f, Len, 0.4f), FVector(Pos - 1.5f, Len, 0.4f), FVector::UpVector, Joint, Joint, Joint, Joint);
		M.Quad(FVector(-Len, Pos - 1.5f, 0.4f), FVector(-Len, Pos + 1.5f, 0.4f), FVector(Len, Pos + 1.5f, 0.4f), FVector(Len, Pos - 1.5f, 0.4f), FVector::UpVector, Joint, Joint, Joint, Joint);
	}
	// mur circulaire : dégradé sombre, face tournée vers le centre
	const FLinearColor WLow(0.05f, 0.058f, 0.075f), WMid(0.032f, 0.037f, 0.05f), WTop(0.012f, 0.014f, 0.02f);
	for (int32 s = 0; s < Segs; ++s)
	{
		const float A0 = 2.f * PI * s / Segs, A1 = 2.f * PI * (s + 1) / Segs;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
		const FVector In = -(D0 + D1).GetSafeNormal();
		M.Quad(D0 * WallR, D1 * WallR, D1 * WallR + FVector(0, 0, 280.f), D0 * WallR + FVector(0, 0, 280.f), In, WithRough(WLow, 0.7f), WithRough(WLow, 0.7f), WithRough(WMid, 0.7f), WithRough(WMid, 0.7f));
		M.Quad(D0 * WallR + FVector(0, 0, 280.f), D1 * WallR + FVector(0, 0, 280.f), D1 * WallR + FVector(0, 0, WallH), D0 * WallR + FVector(0, 0, WallH), In, WithRough(WMid, 0.7f), WithRough(WMid, 0.7f), WithRough(WTop, 0.7f), WithRough(WTop, 0.7f));
	}
	// plafond
	const FLinearColor Ceil = WithRough(FLinearColor(0.015f, 0.017f, 0.022f), 0.8f);
	for (int32 s = 0; s < Segs; ++s)
	{
		const float A0 = 2.f * PI * s / Segs, A1 = 2.f * PI * (s + 1) / Segs;
		M.Quad(FVector(0, 0, WallH), FVector(FMath::Cos(A0) * WallR, FMath::Sin(A0) * WallR, WallH), FVector(FMath::Cos(A1) * WallR, FMath::Sin(A1) * WallR, WallH), FVector(0, 0, WallH),
			-FVector::UpVector, Ceil, Ceil, Ceil, Ceil);
	}
	// panneau du nom, d'un bord à l'autre du mur du fond
	M.Box(FVector(-BoardX - 8.f, 0.f, 560.f), FVector(8.f, 840.f, 130.f), WithRough(FLinearColor(0.018f, 0.02f, 0.028f), 0.45f));
	M.Into(Room, 0);
	Room->SetMaterial(0, Mat(LitMID));
}

void AGPMShowroom::BuildGlow()
{
	using namespace GPMRoomGeo;
	FRoomMesh M;
	const FLinearColor Ramp(0.55f, 0.68f, 0.95f), Panel(0.85f, 0.9f, 1.f);
	// rampes verticales sur le mur, sauf derrière le panneau du nom (côté -X)
	for (int32 k = 0; k < 14; ++k)
	{
		const float An = 2.f * PI * (k + 0.5f) / 14.f;
		const FVector D(FMath::Cos(An), FMath::Sin(An), 0);
		if (D.X < -0.86f) continue;
		const FVector Side = FVector::CrossProduct(FVector::UpVector, D) * 9.f, At = D * (WallR - 4.f);
		M.Quad(At - Side + FVector(0, 0, 90.f), At + Side + FVector(0, 0, 90.f), At + Side + FVector(0, 0, 660.f), At - Side + FVector(0, 0, 660.f), -D, Ramp, Ramp, Ramp * 0.3f, Ramp * 0.3f);
	}
	// panneaux de lumière au plafond, au-dessus de la voiture
	for (int32 k = -1; k <= 1; ++k)
	{
		const float Y = k * 170.f;
		M.Quad(FVector(-420.f, Y - 30.f, WallH - 6.f), FVector(420.f, Y - 30.f, WallH - 6.f), FVector(420.f, Y + 30.f, WallH - 6.f), FVector(-420.f, Y + 30.f, WallH - 6.f), -FVector::UpVector, Panel, Panel, Panel, Panel);
	}
	M.Into(Glow, 0);
	Glow->SetMaterial(0, Mat(GlowMID));
}

void AGPMShowroom::BuildPlate()
{
	using namespace GPMRoomGeo;
	FRoomMesh M;
	// plateau : disque de métal brossé, anneaux concentriques, tranche sombre
	const int32 Segs = 64;
	const FLinearColor Top0(0.3f, 0.32f, 0.36f), Top1(0.17f, 0.18f, 0.21f), Rim(0.08f, 0.085f, 0.1f);
	for (int32 r = 0; r < 8; ++r)
	{
		const float Ra = PlateR * r / 8.f, Rb = PlateR * (r + 1) / 8.f;
		const FLinearColor C0 = WithRough(Blend(Top0, Top1, r / 8.f) * (r % 2 ? 1.08f : 1.f), 0.32f), C1 = WithRough(Blend(Top0, Top1, (r + 1) / 8.f), 0.32f);
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A0 = 2.f * PI * s / Segs, A1 = 2.f * PI * (s + 1) / Segs;
			M.Quad(FVector(FMath::Cos(A0) * Ra, FMath::Sin(A0) * Ra, PlateH), FVector(FMath::Cos(A1) * Ra, FMath::Sin(A1) * Ra, PlateH),
				FVector(FMath::Cos(A1) * Rb, FMath::Sin(A1) * Rb, PlateH), FVector(FMath::Cos(A0) * Rb, FMath::Sin(A0) * Rb, PlateH), FVector::UpVector, C0, C0, C1, C1);
		}
	}
	const FLinearColor RimC = WithRough(Rim, 0.5f), Mark = WithRough(FLinearColor(0.55f, 0.57f, 0.6f), 0.3f);
	for (int32 s = 0; s < Segs; ++s)
	{
		const float A0 = 2.f * PI * s / Segs, A1 = 2.f * PI * (s + 1) / Segs;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
		M.Quad(D0 * PlateR, D1 * PlateR, D1 * PlateR + FVector(0, 0, PlateH), D0 * PlateR + FVector(0, 0, PlateH), (D0 + D1).GetSafeNormal(), RimC, RimC, RimC, RimC);
		// repères sur le bord du plateau (tous les 11,25°)
		if (s % 2 == 0)
		{
			const FVector T0 = D0 * (PlateR - 22.f), T1 = D0 * (PlateR - 12.f), Lat = FVector::CrossProduct(FVector::UpVector, D0) * 1.6f;
			M.Quad(T0 - Lat + FVector(0, 0, PlateH + 0.4f), T1 - Lat + FVector(0, 0, PlateH + 0.4f), T1 + Lat + FVector(0, 0, PlateH + 0.4f), T0 + Lat + FVector(0, 0, PlateH + 0.4f), FVector::UpVector, Mark, Mark, Mark, Mark);
		}
	}
	// ombre douce sous la voiture
	const FLinearColor Sh = WithRough(FLinearColor(0.03f, 0.03f, 0.035f), 0.6f), ShEdge = WithRough(Top0 * 0.9f, 0.32f);
	for (int32 s = 0; s < 32; ++s)
	{
		const float A0 = 2.f * PI * s / 32.f, A1 = 2.f * PI * (s + 1) / 32.f;
		const FVector E0(FMath::Cos(A0) * 290.f, FMath::Sin(A0) * 120.f, PlateH + 0.3f), E1(FMath::Cos(A1) * 290.f, FMath::Sin(A1) * 120.f, PlateH + 0.3f);
		const FVector In0 = E0 * FVector(0.6f, 0.6f, 1.f), In1 = E1 * FVector(0.6f, 0.6f, 1.f);
		M.Quad(FVector(0, 0, PlateH + 0.3f), In0, In1, FVector(0, 0, PlateH + 0.3f), FVector::UpVector, Sh, Sh, Sh, Sh);
		M.Quad(In0, In1, E1, E0, FVector::UpVector, Sh, Sh, ShEdge, ShEdge);
	}
	M.Into(Plate, 0);
	UMaterialInstanceDynamic* PlateMID = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	if (PlateMID) { PlateMID->SetScalarParameterValue(TEXT("Rough"), 1.f); PlateMID->SetScalarParameterValue(TEXT("Metal"), 1.f); }
	Plate->SetMaterial(0, Mat(PlateMID));
}

// pièces à la couleur de l'écurie, reconstruites quand elle change : couleur dans les sommets (pas de teinte du matériau,
// qui effacerait les dégradés)
void AGPMShowroom::BuildAccent(const FLinearColor& Neon)
{
	using namespace GPMRoomGeo;
	const int32 Segs = 48;
	const FLinearColor Lit = WithRough(Neon, 1.f);
	// plinthe lumineuse au pied du mur, filet sous le nom
	FRoomMesh G;
	for (int32 s = 0; s < Segs; ++s)
	{
		const float A0 = 2.f * PI * s / Segs, A1 = 2.f * PI * (s + 1) / Segs;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
		const FVector In = -(D0 + D1).GetSafeNormal();
		const float Rw = WallR - 3.f;
		G.Quad(D0 * Rw + FVector(0, 0, 18.f), D1 * Rw + FVector(0, 0, 18.f), D1 * Rw + FVector(0, 0, 30.f), D0 * Rw + FVector(0, 0, 30.f), In, Lit, Lit, Lit, Lit);
	}
	G.Quad(FVector(-BoardX + 1.f, -820.f, 436.f), FVector(-BoardX + 1.f, 820.f, 436.f), FVector(-BoardX + 1.f, 820.f, 442.f), FVector(-BoardX + 1.f, -820.f, 442.f), FVector(1, 0, 0), Lit, Lit, Lit, Lit);
	G.Into(Glow, 1);
	Glow->SetMaterial(1, Mat(AccentMID));
	// anneau lumineux du plateau
	FRoomMesh Ring;
	for (int32 s = 0; s < 64; ++s)
	{
		const float A0 = 2.f * PI * s / 64.f, A1 = 2.f * PI * (s + 1) / 64.f;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
		const FVector Up(0, 0, PlateH + 0.5f);
		Ring.Quad(D0 * (PlateR - 9.f) + Up, D1 * (PlateR - 9.f) + Up, D1 * (PlateR - 3.f) + Up, D0 * (PlateR - 3.f) + Up, FVector::UpVector, Lit, Lit, Lit, Lit);
	}
	Ring.Into(Plate, 1);
	Plate->SetMaterial(1, Mat(AccentMID));
	// halo au sol autour du plateau : le sol éclairé se teinte de la couleur de l'écurie et retrouve sa couleur au bord
	FRoomMesh Halo;
	const float R0 = PlateR + 2.f, Span = 170.f;
	for (int32 r = 0; r < 4; ++r)
	{
		const float Ra = R0 + Span * r / 4.f, Rb = R0 + Span * (r + 1) / 4.f;
		const float Ka = 0.6f * FMath::Square(1.f - r / 4.f), Kb = 0.6f * FMath::Square(1.f - (r + 1) / 4.f);
		FLinearColor Ca = Blend(FloorAt(Ra), Neon, Ka), Cb = Blend(FloorAt(Rb), Neon, Kb);
		Ca.A = Cb.A = 0.24f;
		for (int32 s = 0; s < Segs; ++s)
		{
			const float A0 = 2.f * PI * s / Segs, A1 = 2.f * PI * (s + 1) / Segs;
			const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
			const FVector Up(0, 0, 0.6f);
			Halo.Quad(D0 * Ra + Up, D1 * Ra + Up, D1 * Rb + Up, D0 * Rb + Up, FVector::UpVector, Ca, Ca, Cb, Cb);
		}
	}
	Halo.Into(Room, 1);
	Room->SetMaterial(1, Mat(LitMID));
}

// ---------- personnages : combinaison du pilote ou costume de l'agent, 1,80 m ----------

FGPMOutfit AGPMShowroom::Outfit(const FLinearColor& Main, const FLinearColor& Accent, const FString& Who, bool bBusiness)
{
	static const FLinearColor Skins[] = { FLinearColor(0.75f, 0.52f, 0.4f), FLinearColor(0.55f, 0.36f, 0.25f), FLinearColor(0.38f, 0.24f, 0.15f), FLinearColor(0.2f, 0.12f, 0.08f) };
	static const FLinearColor Hairs[] = { FLinearColor(0.02f, 0.015f, 0.01f), FLinearColor(0.08f, 0.04f, 0.02f), FLinearColor(0.3f, 0.18f, 0.07f), FLinearColor(0.05f, 0.05f, 0.05f) };
	const uint32 H = GetTypeHash(Who);
	FGPMOutfit O;
	O.Skin = Skins[H % 4];
	O.Hair = Hairs[(H / 4) % 4];
	O.bSuit = bBusiness;
	O.Top = Main;
	O.Legs = bBusiness ? Main : Main * 0.75f;
	O.Stripe = Accent;
	return O;
}

void AGPMShowroom::BuildFigure(UProceduralMeshComponent* Body, UProceduralMeshComponent* Arm, const FGPMOutfit& O)
{
	using namespace GPMRoomGeo;
	if (!Body || !Arm) return;
	FRoomMesh M;
	const FLinearColor Top = WithRough(O.Top, O.bSuit ? 0.75f : 0.55f), Legs = WithRough(O.Legs, 0.7f), Skin = WithRough(O.Skin, 0.55f);
	const FLinearColor Shoe = WithRough(FLinearColor(0.02f, 0.02f, 0.025f), 0.35f), HairC = WithRough(O.Hair, 0.8f), Stripe = WithRough(O.Stripe, 0.5f);
	for (int32 s = -1; s <= 1; s += 2)
	{
		M.Box(FVector(5.f, s * 10.f, 5.f), FVector(14.f, 6.5f, 5.f), Shoe);
		M.Cyl(FVector(0.f, s * 10.f, 8.f), FVector(0.f, s * 10.5f, 94.f), 7.5f, 10, Legs);
	}
	M.Box(FVector(0.f, 0.f, 98.f), FVector(12.f, 20.f, 9.f), Legs);
	M.Box(FVector(0.f, 0.f, 128.f), FVector(12.f, 21.f, 26.f), Top);
	M.Ball(FVector(0.f, 0.f, 152.f), 21.5f, Top, 0.35f);
	for (int32 s = -1; s <= 1; s += 2) M.Ball(FVector(0.f, s * 22.f, 148.f), 8.5f, Top);
	// bras gauche le long du corps
	M.Cyl(FVector(0.f, -24.f, 148.f), FVector(1.f, -27.f, 96.f), 5.8f, 10, Top);
	M.Ball(FVector(1.f, -27.f, 91.f), 5.5f, Skin);
	if (O.bSuit)
	{
		// chemise blanche, cravate, dossier sous le bras
		M.Box(FVector(12.4f, 0.f, 141.f), FVector(0.8f, 6.5f, 12.f), WithRough(FLinearColor(0.85f, 0.86f, 0.88f), 0.6f));
		M.Box(FVector(13.4f, 0.f, 137.f), FVector(0.8f, 2.2f, 10.f), Stripe);
		M.Box(FVector(4.f, -33.f, 100.f), FVector(13.f, 1.6f, 17.f), WithRough(FLinearColor(0.16f, 0.1f, 0.06f), 0.5f));
	}
	else
	{
		// combinaison : bande d'accent devant et dans le dos, casque tenu à la main gauche
		M.Box(FVector(12.3f, 0.f, 128.f), FVector(0.6f, 4.5f, 26.f), Stripe);
		M.Box(FVector(-12.3f, 0.f, 128.f), FVector(0.6f, 4.5f, 26.f), Stripe);
		M.Ball(FVector(6.f, -31.f, 84.f), 13.f, WithRough(O.Top, 0.25f), 1.05f);
		M.Box(FVector(16.f, -31.f, 86.f), FVector(3.5f, 8.f, 4.f), WithRough(FLinearColor(0.03f, 0.03f, 0.04f), 0.1f));
	}
	M.Cyl(FVector(0.f, 0.f, 155.f), FVector(0.5f, 0.f, 164.f), 5.5f, 10, Skin);
	M.Ball(FVector(1.f, 0.f, 172.f), 11.f, Skin, 1.1f);
	M.Ball(FVector(-1.5f, 0.f, 175.f), 11.6f, HairC, 1.f, 6, 12);
	for (int32 s = -1; s <= 1; s += 2) M.Ball(FVector(10.5f, s * 4.f, 173.f), 1.5f, WithRough(FLinearColor(0.02f, 0.02f, 0.02f), 0.2f), 1.f, 4, 6);
	M.Into(Body, 0);
	// bras droit : pend depuis l'épaule (origine du composant), levé pour la poignée de main
	FRoomMesh A;
	A.Cyl(FVector(0.f, 1.f, 0.f), FVector(1.f, 4.f, -52.f), 5.8f, 10, Top);
	A.Ball(FVector(1.f, 4.f, -57.f), 5.5f, Skin);
	A.Into(Arm, 0);
	Body->SetMaterial(0, Mat(LitMID));
	Arm->SetMaterial(0, Mat(LitMID));
}

void AGPMShowroom::SetCast(const FGPMOutfit& A, const FGPMOutfit& B)
{
	auto Hash = [](const FGPMOutfit& O)
	{
		uint32 S = HashCombine(GetTypeHash(O.Top.ToFColor(true)), GetTypeHash(O.Legs.ToFColor(true)));
		S = HashCombine(S, HashCombine(GetTypeHash(O.Stripe.ToFColor(true)), GetTypeHash(O.Skin.ToFColor(true))));
		return HashCombine(S, HashCombine(GetTypeHash(O.Hair.ToFColor(true)), uint32(O.bSuit)));
	};
	const uint32 Sig = HashCombine(Hash(A), Hash(B)) | 1u;
	if (Sig == CastSig) return;
	CastSig = Sig;
	BuildFigure(FigA, ArmA, A);
	BuildFigure(FigB, ArmB, B);
}

// ---------- état, couleurs, scènes, caméra ----------

void AGPMShowroom::SetShown(bool bShow)
{
	if (bShow == bShown) return;
	bShown = bShow;
	SetActorHiddenInGame(!bShow);
	if (Display) Display->SetActorHiddenInGame(!bShow);
	KeyLight->SetVisibility(bShow);
	RimA->SetVisibility(bShow);
	RimB->SetVisibility(bShow);
	FillLight->SetVisibility(bShow);
}

void AGPMShowroom::SetTeam(const FLinearColor& Main, const FLinearColor& Accent, const FString& Name)
{
	const bool bColors = !Main.Equals(TeamMain, 0.002f) || !Accent.Equals(TeamAccent, 0.002f) || !Glow->GetProcMeshSection(1);
	TeamMain = Main;
	TeamAccent = Accent;
	if (!bBuilt) return;
	if (bColors)
	{
		if (Display) Display->SetLivery(Main, Accent);
		// plinthe, anneau et halo dans la couleur principale (mêlée à l'accent si elle est trop sombre pour briller)
		FLinearColor Neon = Main.GetLuminance() < 0.08f ? Main * 0.35f + Accent * 0.65f : Main;
		Neon.A = 1.f;
		BuildAccent(Neon);
		RimA->SetLightColor(Neon * 0.7f + FLinearColor::White * 0.3f);
	}
	if (Name != TeamLabel || bColors)
	{
		TeamLabel = Name;
		const FString Shown = GPMRoomGeo::Plain(Name);
		NameText->SetText(FText::FromString(Shown));
		// le nom tient sur le panneau (≈ 15 m) : lettres plus petites pour un nom long
		NameText->SetWorldSize(FMath::Clamp(1500.f / (0.62f * FMath::Max(1, Shown.Len())), 70.f, 160.f));
	}
}

FVector AGPMShowroom::CastFocus() const
{
	return (FigA->GetComponentLocation() + FigB->GetComponentLocation()) * 0.5f + FVector(0, 0, 125.f);
}

void AGPMShowroom::PlayScene(int32 Kind, float T)
{
	const bool bOn = Kind != 0;
	if (FigA->IsVisible() != bOn)
	{
		FigA->SetVisibility(bOn, true);
		FigB->SetVisibility(bOn, true);
	}
	if (!bOn) return;
	const FVector Cam(700.f, -1020.f, 0.f);   // côté de la caméra des scènes (repère de l'atelier)
	auto Face = [](const FVector& From, const FVector& To) { return FRotator(0.f, float((To - From).Rotation().Yaw), 0.f); };
	if (Kind == 1)
	{
		// rendez-vous : le pilote et son agent traversent l'atelier côte à côte et s'arrêtent devant la voiture
		const FVector EndA(230.f, -430.f, 0.f), EndB(110.f, -540.f, 0.f), StartA(-300.f, -760.f, 0.f), StartB(-410.f, -880.f, 0.f);
		const float U = FMath::Clamp(T / 3.4f, 0.f, 1.f), E = U * (2.f - U) * 0.35f + U * 0.65f;   // presque constant, ralentit à la fin
		const float Walk = 1.f - FMath::Clamp((T - 3.1f) / 0.4f, 0.f, 1.f);
		const float Bob = FMath::Abs(FMath::Sin(T * 2.f * PI)) * 3.f * Walk;
		const float Turn = FMath::Clamp((T - 3.2f) / 0.6f, 0.f, 1.f);
		FigA->SetRelativeLocationAndRotation(FMath::Lerp(StartA, EndA, E) + FVector(0, 0, Bob), FMath::Lerp(Face(StartA, EndA), Face(EndA, Cam), Turn));
		FigB->SetRelativeLocationAndRotation(FMath::Lerp(StartB, EndB, E) + FVector(0, 0, Bob), FMath::Lerp(Face(StartB, EndB), Face(EndB, Cam), Turn));
		const float Swing = FMath::Sin(T * PI * 2.f * 0.5f * 2.f) * 16.f * Walk;
		ArmA->SetRelativeRotation(FRotator(Swing, 0.f, 0.f));
		ArmB->SetRelativeRotation(FRotator(-Swing, 0.f, 0.f));
	}
	else
	{
		// signature : face à face, poignée de main, puis les deux se tournent vers l'objectif pour la photo
		const FVector Mid(200.f, -470.f, 0.f), Axis(0.744f, 0.668f, 0.f);
		const FVector PA = Mid + Axis * 44.f, PB = Mid - Axis * 44.f;
		const float Raise = FMath::Clamp((T - 0.3f) / 0.5f, 0.f, 1.f) * (1.f - FMath::Clamp((T - 1.8f) / 0.4f, 0.f, 1.f));
		const float Turn = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((T - 2.f) / 0.5f, 0.f, 1.f));
		const float Shake = T > 0.8f && T < 1.8f ? FMath::Sin((T - 0.8f) * 2.f * PI * 3.f) * 7.f : 0.f;
		FigA->SetRelativeLocationAndRotation(PA, FMath::Lerp(Face(PA, PB), Face(PA, Cam), Turn));
		FigB->SetRelativeLocationAndRotation(PB, FMath::Lerp(Face(PB, PA), Face(PB, Cam), Turn));
		ArmA->SetRelativeRotation(FRotator(Raise * 58.f + Shake, -Raise * 24.f, 0.f));
		ArmB->SetRelativeRotation(FRotator(Raise * 58.f - Shake, -Raise * 24.f, 0.f));
	}
}

void AGPMShowroom::Camera(int32 Shot, float T, FVector& Eye, FVector& Look, float& Fov) const
{
	using namespace GPMRoomGeo;
	const FVector O = Spot(), CarC = O + FVector(0, 0, PlateH + 60.f);
	switch (Shot)
	{
	case 0:
	{
		// création de l'écurie, réglages : la voiture dans le tiers droit (le panneau occupe la gauche), léger balancement
		// presque dans l'axe du panneau : le nom de l'écurie reste lisible au-dessus de la voiture
		const float A = -0.05f + 0.05f * FMath::Sin(T * 0.07f);
		Eye = O + FVector(FMath::Cos(A) * 1320.f, FMath::Sin(A) * 1320.f, 255.f);
		const FVector F = (CarC - Eye).GetSafeNormal2D(), Right(-F.Y, F.X, 0.f);
		Look = CarC - Right * 560.f;
		Fov = 70.f;
		break;
	}
	case 1:
	{
		// QG, mercato, négociations : la voiture au centre, derrière les panneaux
		const float A = 0.45f * FMath::Sin(T * 0.05f);
		Eye = O + FVector(FMath::Cos(A) * 1150.f, FMath::Sin(A) * 1150.f, 330.f);
		Look = CarC - FVector(0, 0, 10.f);
		Fov = 66.f;
		break;
	}
	case 2:
	{
		// bienvenue au QG : tour lent autour de la voiture
		const float A = T * 0.4f + 0.6f;
		Eye = O + FVector(FMath::Cos(A) * 720.f, FMath::Sin(A) * 720.f, 175.f);
		Look = CarC;
		Fov = 62.f;
		break;
	}
	case 3:
	{
		// rendez-vous : la caméra suit le pilote et son agent qui arrivent, en s'approchant
		const float U = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T / 3.8f, 0.f, 1.f));
		Eye = O + FMath::Lerp(FVector(830.f, -1180.f, 185.f), FVector(640.f, -950.f, 165.f), U);
		Look = CastFocus() - FVector(0, 0, 5.f);
		Fov = 55.f;
		break;
	}
	case 5:
	{
		// accueil : plan bas de trois quarts, la voiture dans la moitié droite (le menu occupe la gauche), lent balancement
		const float A = 0.35f + 0.22f * FMath::Sin(T * 0.06f);
		Eye = O + FVector(FMath::Cos(A) * 1080.f, FMath::Sin(A) * 1080.f, 150.f);
		const FVector F = (CarC - Eye).GetSafeNormal2D(), Right(-F.Y, F.X, 0.f);
		Look = CarC - Right * 280.f - FVector(0, 0, 25.f);
		Fov = 56.f;
		break;
	}
	default:
	{
		// signature : profil serré sur la poignée de main, puis recul pour la photo devant la voiture
		const float U = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((T - 1.8f) / 0.9f, 0.f, 1.f));
		Eye = O + FMath::Lerp(FVector(560.f, -840.f, 150.f), FVector(700.f, -1020.f, 175.f), U);
		Look = CastFocus() - FVector(0, 0, 15.f * U);
		Fov = 52.f;
		break;
	}
	}
}

void AGPMShowroom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bShown) return;
	// le plateau tourne doucement, sauf pendant les scènes (la voiture se place de trois quarts pour la photo)
	if (FigA->IsVisible()) Yaw = FMath::FInterpTo(Yaw, FMath::RoundToFloat((Yaw - GPMRoomGeo::SceneYaw) / 360.f) * 360.f + GPMRoomGeo::SceneYaw, DeltaSeconds, 1.6f);
	else Yaw = FMath::Fmod(Yaw + DeltaSeconds * 9.f, 360.f);
	Plate->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	if (Display) Display->SetActorLocationAndRotation(Spot() + FVector(0, 0, GPMRoomGeo::PlateH), FRotator(0.f, Yaw, 0.f));
}
