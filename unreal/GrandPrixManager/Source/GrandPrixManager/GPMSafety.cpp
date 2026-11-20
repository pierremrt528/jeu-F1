// Voiture de sécurité : un coupé de sport argent (bande verte, vitres teintées, jantes) avec une rampe de gyrophares orange
// qui clignotent en alternance, et ses phares allumés. Géométrie générée ici (cm, X vers l'avant, Z vers le haut).
#include "GPMCar.h"
#include "GrandPrixManager.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace GPMSafetyGeo
{
	struct FMeshBuf
	{
		TArray<FVector> P, N;
		TArray<FLinearColor> C;
		TArray<int32> T;
		// triangle tourné vers « Out » (normale de face d'Unreal : −(B−A)×(C−A))
		void Tri(const FVector& A, FVector B, FVector Cc, const FVector& Out, const FLinearColor& Col)
		{
			FVector Nf = FVector::CrossProduct(B - A, Cc - A);
			if (Nf.SizeSquared() < 1e-6f) return;
			if (FVector::DotProduct(Nf, Out) > 0.f) { Swap(B, Cc); Nf = -Nf; }
			const FVector Nn = (-Nf).GetSafeNormal();
			for (const FVector& V : { A, B, Cc }) { T.Add(P.Num()); P.Add(V); N.Add(Nn); C.Add(Col); }
		}
		void Quad(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FVector& Ref, const FLinearColor& Col)
		{
			const FVector Out = (A + B + Cc + D) * 0.25f - Ref;
			Tri(A, B, Cc, Out, Col); Tri(A, Cc, D, Out, Col);
		}
		void Box(const FVector& Lo, const FVector& Hi, const FLinearColor& Col)
		{
			const FVector M = (Lo + Hi) * 0.5f;
			auto V = [&](int32 i) { return FVector(i & 1 ? Hi.X : Lo.X, i & 2 ? Hi.Y : Lo.Y, i & 4 ? Hi.Z : Lo.Z); };
			static const int32 F[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };
			for (const auto& Fc : F) Quad(V(Fc[0]), V(Fc[1]), V(Fc[2]), V(Fc[3]), M, Col);
		}
		// roue : bande de roulement noire, flanc et jante grise côté extérieur
		void Wheel(const FVector& Ctr, float R, float Wd, float Side, const FLinearColor& Tyre, const FLinearColor& Rim)
		{
			constexpr int32 Seg = 14;
			const float Yo = Ctr.Y + Side * Wd * 0.5f, Yi = Ctr.Y - Side * Wd * 0.5f;
			for (int32 k = 0; k < Seg; ++k)
			{
				const float A0 = 2.f * PI * k / Seg, A1 = 2.f * PI * (k + 1) / Seg;
				const FVector D0(FMath::Cos(A0) * R, 0, FMath::Sin(A0) * R), D1(FMath::Cos(A1) * R, 0, FMath::Sin(A1) * R);
				const FVector O0 = Ctr + D0, O1 = Ctr + D1;
				Quad(FVector(O0.X, Yi, O0.Z), FVector(O1.X, Yi, O1.Z), FVector(O1.X, Yo, O1.Z), FVector(O0.X, Yo, O0.Z), FVector(Ctr.X, (Yi + Yo) * 0.5f, Ctr.Z), Tyre);
				const FVector Co(Ctr.X, Yo, Ctr.Z);
				Tri(Co, FVector(O0.X, Yo, O0.Z) - D0 * 0.3f, FVector(O1.X, Yo, O1.Z) - D1 * 0.3f, FVector(0, Side, 0), Rim);
				Quad(FVector(O0.X, Yo, O0.Z), FVector(O1.X, Yo, O1.Z), FVector(O1.X, Yo, O1.Z) - D1 * 0.3f, FVector(O0.X, Yo, O0.Z) - D0 * 0.3f, Co - FVector(0, Side, 0), Tyre);
				Tri(FVector(Ctr.X, Yi, Ctr.Z), FVector(O0.X, Yi, O0.Z), FVector(O1.X, Yi, O1.Z), FVector(0, -Side, 0), Tyre);
			}
		}
	};

	// sections de la carrosserie : x, demi-largeur, bas, ligne de caisse, demi-largeur du toit, haut
	struct FStation { float X, W, Z0, Z1, W2, Z2; };
	const FStation Body[9] = {
		{ 235, 78, 24, 52, 70, 56 }, { 205, 90, 18, 66, 82, 70 }, { 120, 94, 16, 76, 86, 80 }, { 60, 95, 16, 80, 80, 84 },
		{ -10, 95, 16, 82, 64, 124 }, { -90, 95, 16, 82, 62, 121 }, { -170, 95, 18, 84, 78, 96 }, { -215, 92, 22, 84, 84, 88 }, { -238, 84, 30, 78, 78, 82 } };
}

void AGPMCar::MakeSafetyBody()
{
	using namespace GPMSafetyGeo;
	bSafetyBody = true;
	CarModel = nullptr;
	HelmetOf = DriverName;
	for (UStaticMeshComponent* P : MainParts) P->SetVisibility(false);
	for (UStaticMeshComponent* P : AccentParts) P->SetVisibility(false);
	for (UStaticMeshComponent* Wh : Wheels) Wh->SetVisibility(false);
	auto Rgh = [](FLinearColor Col, float R) { Col.A = R; return Col; };   // rugosité dans l'alpha (M_GPM_Lit)
	const FLinearColor Silver = Rgh(FLinearColor(0.5f, 0.52f, 0.55f), 0.22f), Green = Rgh(FLinearColor(0.02f, 0.32f, 0.08f), 0.3f);
	const FLinearColor Glass = Rgh(FLinearColor(0.008f, 0.01f, 0.012f), 0.04f), Black = Rgh(FLinearColor(0.02f, 0.02f, 0.02f), 0.7f);
	const FLinearColor Gum = Rgh(FLinearColor(0.015f, 0.015f, 0.015f), 0.9f), Rim = Rgh(FLinearColor(0.3f, 0.3f, 0.32f), 0.3f);
	const FLinearColor Red = Rgh(FLinearColor(0.45f, 0.01f, 0.01f), 0.1f);

	FMeshBuf B;
	auto Ring = [](const FStation& S, FVector Out[8])
	{
		const float Zm = S.Z0 + 0.4f * (S.Z1 - S.Z0);
		const float Yz[8][2] = { { -S.W, S.Z0 }, { S.W, S.Z0 }, { S.W, Zm }, { S.W, S.Z1 }, { S.W2, S.Z2 }, { -S.W2, S.Z2 }, { -S.W, S.Z1 }, { -S.W, Zm } };
		for (int32 k = 0; k < 8; ++k) Out[k] = FVector(S.X, Yz[k][0], Yz[k][1]);
	};
	for (int32 s = 0; s + 1 < 9; ++s)
	{
		FVector A[8], Bb[8];
		Ring(Body[s], A); Ring(Body[s + 1], Bb);
		const FVector Ref((Body[s].X + Body[s + 1].X) * 0.5f, 0, 55.f);
		const bool bGlassTop = s == 3 || s == 5, bGlassSide = s >= 3 && s <= 5;
		for (int32 k = 0; k < 8; ++k)
		{
			const int32 k1 = (k + 1) % 8;
			const FLinearColor Col = k == 0 ? Black : (k == 1 || k == 7) ? Green : k == 4 ? (bGlassTop ? Glass : Silver) : (k == 3 || k == 5) ? (bGlassSide ? Glass : Silver) : Silver;
			B.Quad(A[k], A[k1], Bb[k1], Bb[k], Ref, Col);
		}
	}
	// faces avant et arrière
	for (int32 e = 0; e < 2; ++e)
	{
		FVector R[8];
		Ring(Body[e ? 8 : 0], R);
		const FVector Ctr(Body[e ? 8 : 0].X, 0, 45.f), Out(e ? -1.f : 1.f, 0, 0);
		for (int32 k = 0; k < 8; ++k) B.Tri(Ctr, R[k], R[(k + 1) % 8], Out, k == 0 ? Black : Silver);
	}
	// calandre, feux arrière, becquet, rétroviseurs, rampe de gyrophares (socle)
	B.Box(FVector(232, -50, 26), FVector(238, 50, 40), Black);
	for (float Sd : { -1.f, 1.f })
	{
		B.Box(FVector(-241, Sd > 0 ? 44 : -76, 58), FVector(-236, Sd > 0 ? 76 : -44, 68), Red);
		B.Box(FVector(55, Sd > 0 ? 95 : -107, 84), FVector(70, Sd > 0 ? 107 : -95, 94), Silver);
		B.Wheel(FVector(140, Sd * 82.f, 33), 33.f, 24.f, Sd, Gum, Rim);
		B.Wheel(FVector(-145, Sd * 82.f, 33), 33.f, 26.f, Sd, Gum, Rim);
	}
	B.Box(FVector(-232, -80, 86), FVector(-212, 80, 90), Black);
	B.Box(FVector(-48, -56, 120), FVector(-16, 56, 127), Black);
	Model->ClearAllMeshSections();
	Model->CreateMeshSection_LinearColor(0, B.P, B.T, B.N, TArray<FVector2D>(), B.C, TArray<FProcMeshTangent>(), false);
	if (UMaterialInstanceDynamic* M = GPMArt::Make(TEXT("M_GPM_Lit"), this))
	{
		M->SetScalarParameterValue(TEXT("Rough"), 1.f);
		M->SetScalarParameterValue(TEXT("Metal"), 0.6f);   // peinture métallisée
		M->SetScalarParameterValue(TEXT("Spec"), 0.7f);
		Model->SetMaterial(0, M);
	}
	else if (VertexColorMat) Model->SetMaterial(0, VertexColorMat);

	// gyrophares gauche (1) et droit (2), phares (3) : sections lumineuses
	auto Glow = [this](int32 Sec, const FMeshBuf& G, const FLinearColor& Col, const FLinearColor& Gl)
	{
		Model->CreateMeshSection_LinearColor(Sec, G.P, G.T, G.N, TArray<FVector2D>(), G.C, TArray<FProcMeshTangent>(), false);
		if (UMaterialInstanceDynamic* M = GPMArt::Make(TEXT("M_GPM_Inst"), this))
		{
			M->SetVectorParameterValue(TEXT("Color"), Col);
			M->SetVectorParameterValue(TEXT("Color2"), Col);
			M->SetVectorParameterValue(TEXT("Glow"), Gl);
			Model->SetMaterial(Sec, M);
		}
	};
	const FLinearColor Amber(0.9f, 0.35f, 0.02f), White(0.9f, 0.9f, 0.85f);
	FMeshBuf L, R, H;
	L.Box(FVector(-44, -54, 127), FVector(-20, -6, 135), Amber);
	R.Box(FVector(-44, 6, 127), FVector(-20, 54, 135), Amber);
	H.Box(FVector(231, -74, 42), FVector(237, -46, 50), White);
	H.Box(FVector(231, 46, 42), FVector(237, 74, 50), White);
	Glow(1, L, Amber, FLinearColor(40.f, 12.f, 0.5f));
	Glow(2, R, Amber, FLinearColor(40.f, 12.f, 0.5f));
	Glow(3, H, White, FLinearColor(12.f, 12.f, 10.f));
	Model->SetMeshSectionVisible(1, false);
	Model->SetMeshSectionVisible(2, false);
}

// gyrophares : gauche et droite en alternance, deux éclats chacun
void AGPMCar::TickBeacons(float Dt)
{
	BeaconT += Dt;
	const float Ph = FMath::Fmod(BeaconT, 0.6f);
	const bool bFlash = FMath::Fmod(Ph, 0.3f) < 0.09f || (FMath::Fmod(Ph, 0.3f) > 0.14f && FMath::Fmod(Ph, 0.3f) < 0.21f);
	Model->SetMeshSectionVisible(1, bBeacons && bFlash && Ph < 0.3f);
	Model->SetMeshSectionVisible(2, bBeacons && bFlash && Ph >= 0.3f);
}
