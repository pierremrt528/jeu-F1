// Décor sur mesure d'un circuit urbain (Content/Data/tracks/<id>_deco.json, généré par unreal/tools/principaute/deco.py) :
// port et mer (polygone d'eau, quais en pierre, promenade, digues et leurs feux, pontons, yachts), relief (coteau qui
// monte vers l'intérieur des terres, rocher en plateau), immeubles alignés sur les rues, monuments (casino, palais,
// grand hôtel), tribunes du port, piscine, jardin et fontaine, palmiers, lampadaires, tunnel couvert et éclairé.
#include "GPMScenery.h"
#include "GrandPrixManager.h"
#include "GPMTypes.h"
#include "GPMDataSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"

namespace GPMCityGeo
{
	FLinearColor Srgb(uint32 H) { return FLinearColor(FColor((H >> 16) & 255, (H >> 8) & 255, H & 255)); }
	uint32 Hash32(uint32 A) { A ^= A >> 16; A *= 0x7feb352d; A ^= A >> 15; A *= 0x846ca68b; A ^= A >> 16; return A; }

	/** Maillage en construction : chaque triangle est tourné du côté de la normale de ses sommets. */
	struct FBuf
	{
		TArray<FVector> V, N; TArray<FVector2D> UV; TArray<FLinearColor> C; TArray<int32> I;
		int32 Add(const FVector& P, const FVector& Nrm, const FVector2D& T, const FLinearColor& Col) { V.Add(P); N.Add(Nrm); UV.Add(T); C.Add(Col); return V.Num() - 1; }
		void Tri(int32 A, int32 B, int32 Cc)
		{
			// normale de face Unreal = −(B−A)×(C−A)
			const bool bFlip = FVector::DotProduct(FVector::CrossProduct(V[B] - V[A], V[Cc] - V[A]), N[A] + N[B] + N[Cc]) > 0.f;
			I.Add(A); I.Add(bFlip ? Cc : B); I.Add(bFlip ? B : Cc);
		}
		/** Quadrilatère A B C D (dans l'ordre du pourtour), UV en mètres. */
		void Quad(const FVector& A, const FVector& B, const FVector& Cc, const FVector& D, const FVector& Nrm, const FLinearColor& Col)
		{
			auto Uv = [](const FVector& P) { return FVector2D(P.X / 100.0 + P.Z / 100.0, P.Y / 100.0); };
			const int32 a = Add(A, Nrm, Uv(A), Col), b = Add(B, Nrm, Uv(B), Col), c = Add(Cc, Nrm, Uv(Cc), Col), d = Add(D, Nrm, Uv(D), Col);
			Tri(a, b, c); Tri(a, c, d);
		}
	};

	bool PointInPoly(const TArray<FVector2D>& P, double X, double Y)
	{
		bool bIn = false;
		for (int32 i = 0, j = P.Num() - 1; i < P.Num(); j = i++)
			if ((P[i].Y > Y) != (P[j].Y > Y) && X < (P[j].X - P[i].X) * (Y - P[i].Y) / (P[j].Y - P[i].Y) + P[i].X) bIn = !bIn;
		return bIn;
	}

	/** Triangulation d'un polygone simple (oreilles), indices dans P. */
	void EarClip(const TArray<FVector2D>& P, TArray<int32>& Out)
	{
		Out.Reset();
		const int32 Np = P.Num();
		if (Np < 3) return;
		double Area = 0.0;
		for (int32 i = 0; i < Np; ++i) Area += P[i].X * P[(i + 1) % Np].Y - P[(i + 1) % Np].X * P[i].Y;
		TArray<int32> Ring;
		for (int32 i = 0; i < Np; ++i) Ring.Add(Area > 0.0 ? i : Np - 1 - i);   // sens trigonométrique
		auto Turn = [](const FVector2D& A, const FVector2D& B, const FVector2D& Q) { return (B.X - A.X) * (Q.Y - A.Y) - (B.Y - A.Y) * (Q.X - A.X); };
		int32 Guard = 0;
		while (Ring.Num() > 3 && Guard++ < 40000)
		{
			bool bCut = false;
			const int32 Nr = Ring.Num();
			for (int32 k = 0; k < Nr && !bCut; ++k)
			{
				const int32 ia = Ring[(k + Nr - 1) % Nr], ib = Ring[k], ic = Ring[(k + 1) % Nr];
				const FVector2D& A = P[ia]; const FVector2D& B = P[ib]; const FVector2D& Cc = P[ic];
				if (Turn(A, B, Cc) <= 0.0) continue;   // sommet rentrant
				bool bInside = false;
				for (int32 m : Ring)
				{
					if (m == ia || m == ib || m == ic) continue;
					const FVector2D& Q = P[m];
					if (Turn(A, B, Q) >= 0.0 && Turn(B, Cc, Q) >= 0.0 && Turn(Cc, A, Q) >= 0.0) { bInside = true; break; }
				}
				if (bInside) continue;
				Out.Append({ ia, ib, ic });
				Ring.RemoveAt(k);
				bCut = true;
			}
			if (!bCut) break;
		}
		if (Ring.Num() == 3) Out.Append({ Ring[0], Ring[1], Ring[2] });
	}

	float Nv(const TSharedPtr<FJsonValue>& V, int32 k)
	{
		if (!V.IsValid() || V->Type != EJson::Array) return 0.f;
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		return A.IsValidIndex(k) && A[k].IsValid() ? float(A[k]->AsNumber()) : 0.f;
	}
}

TArray<TSharedPtr<FJsonValue>> AGPMScenery::DecoArr(const TCHAR* Key) const
{
	UGameInstance* GI = GetGameInstance();
	UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	const TSharedPtr<FJsonValue> V = Data && !DecoFile.IsEmpty() ? Data->GetRaw(DecoFile, Key) : nullptr;
	return V.IsValid() && V->Type == EJson::Array ? V->AsArray() : TArray<TSharedPtr<FJsonValue>>();
}

void AGPMScenery::LoadDeco()
{
	bDeco = false;
	DecoFile.Reset(); WaterPoly.Reset(); Plateaus.Reset(); PlateauH.Reset(); DecoTrees.Reset();
	WaterBox = FBox2D(ForceInit);
	HillGrad = 0.f;
	if (!Track || !Track->bStreet) return;
	UGameInstance* GI = GetGameInstance();
	UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	if (!Data) return;
	DecoFile = FString::Printf(TEXT("tracks/%s_deco"), *Track->Id);
	for (const TSharedPtr<FJsonValue>& P : DecoArr(TEXT("water")))
	{
		const FVector2D Q(GPMCityGeo::Nv(P, 0) * 100.f, GPMCityGeo::Nv(P, 1) * 100.f);
		WaterPoly.Add(Q); WaterBox += Q;
	}
	if (WaterPoly.Num() < 3) { WaterPoly.Reset(); DecoFile.Reset(); return; }
	if (const TSharedPtr<FJsonValue> Wz = Data->GetRaw(DecoFile, TEXT("waterZ")); Wz.IsValid()) WaterZ = float(Wz->AsNumber()) * 100.f;
	if (const TSharedPtr<FJsonValue> Hv = Data->GetRaw(DecoFile, TEXT("hills")); Hv.IsValid() && Hv->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> O = Hv->AsObject();
		HillP0 = FVector2D(GPMCityGeo::Nv(O->TryGetField(TEXT("p0")), 0), GPMCityGeo::Nv(O->TryGetField(TEXT("p0")), 1)) * 100.f;
		HillN = FVector2D(GPMCityGeo::Nv(O->TryGetField(TEXT("n")), 0), GPMCityGeo::Nv(O->TryGetField(TEXT("n")), 1)).GetSafeNormal();
		double D = 0.0;
		if (O->TryGetNumberField(TEXT("start"), D)) HillStart = float(D);
		if (O->TryGetNumberField(TEXT("grad"), D)) HillGrad = float(D);
		if (O->TryGetNumberField(TEXT("max"), D)) HillMax = float(D);
	}
	for (const TSharedPtr<FJsonValue>& P : DecoArr(TEXT("plateaus")))
	{
		Plateaus.Add(FVector4f(GPMCityGeo::Nv(P, 0) * 100.f, GPMCityGeo::Nv(P, 1) * 100.f, FMath::Max(1.f, GPMCityGeo::Nv(P, 2)) * 100.f, FMath::Max(1.f, GPMCityGeo::Nv(P, 3)) * 100.f));
		PlateauH.Add(GPMCityGeo::Nv(P, 5) * 100.f);
	}
	for (const TSharedPtr<FJsonValue>& P : DecoArr(TEXT("trees")))
		DecoTrees.Add(FVector4f(GPMCityGeo::Nv(P, 0) * 100.f, GPMCityGeo::Nv(P, 1) * 100.f, GPMCityGeo::Nv(P, 2), FMath::Max(0.3f, GPMCityGeo::Nv(P, 3))));
	bDeco = true;
	UE_LOG(LogGPM, Log, TEXT("Décor sur mesure %s : eau %d points, %d plateaux, %d arbres"), *DecoFile, WaterPoly.Num(), Plateaus.Num(), DecoTrees.Num());
}

float AGPMScenery::DecoLift(float X, float Y) const
{
	float Rise = 0.f;
	if (HillGrad > 0.f)
	{
		const float In = float(((X - HillP0.X) * HillN.X + (Y - HillP0.Y) * HillN.Y) / 100.0);   // m vers l'intérieur des terres
		Rise = FMath::Clamp((In - HillStart) * HillGrad, 0.f, HillMax) * 100.f;
	}
	for (int32 k = 0; k < Plateaus.Num(); ++k)
	{
		const FVector4f& Pl = Plateaus[k];
		const float U = (X - Pl.X) / Pl.Z, W = (Y - Pl.Y) / Pl.W;
		const float Rr = FMath::Sqrt(U * U + W * W);
		const float Tt = FMath::Clamp((1.f - Rr) / 0.18f, 0.f, 1.f);   // falaises sur le pourtour
		Rise = FMath::Max(Rise, PlateauH[k] * Tt * Tt * (3.f - 2.f * Tt));
	}
	return Rise;
}

bool AGPMScenery::InWater(float X, float Y) const
{
	if (WaterPoly.Num() < 3 || !WaterBox.IsInside(FVector2D(X, Y))) return false;
	return GPMCityGeo::PointInPoly(WaterPoly, X, Y);
}

void AGPMScenery::BuildDeco()
{
	if (DecoMesh) { DecoMesh->DestroyComponent(); DecoMesh = nullptr; }
	for (UPointLightComponent* L : TunnelLights) if (L) L->DestroyComponent();
	TunnelLights.Reset();
	if (!bDeco || !Track) return;
	using GPMCityGeo::Nv;
	using GPMCityGeo::Srgb;
	const FVector Up = FVector::UpVector;
	GPMCityGeo::FBuf Water, Stone, Walk;

	// --- eau : polygone du port et de la mer, plus vert près des quais, bleu profond au large
	{
		TArray<int32> Tris;
		GPMCityGeo::EarClip(WaterPoly, Tris);
		const FVector2D Mid = WaterBox.GetCenter();
		for (const FVector2D& Q : WaterPoly)
		{
			const float Far = FMath::Clamp(float(FVector2D::Distance(Q, FVector2D(Track->Points[0])) / 250000.0), 0.f, 1.f);
			const FLinearColor Col = FMath::Lerp(FLinearColor(0.01f, 0.075f, 0.1f, 0.07f), FLinearColor(0.006f, 0.045f, 0.11f, 0.09f), Far);
			Water.Add(FVector(Q.X, Q.Y, WaterZ), Up, FVector2D((Q - Mid) / 1000.0), Col);
		}
		for (int32 k = 0; k + 2 < Tris.Num(); k += 3) Water.Tri(Tris[k], Tris[k + 1], Tris[k + 2]);
		UE_LOG(LogGPM, Log, TEXT("Décor : eau %d triangles"), Tris.Num() / 3);
	}

	// --- quais : mur de pierre (algues près de l'eau) et promenade dallée de 7 m (l'eau est à gauche de chaque ligne)
	for (const TSharedPtr<FJsonValue>& Line : DecoArr(TEXT("quays")))
	{
		if (!Line.IsValid() || Line->Type != EJson::Array) continue;
		const TArray<TSharedPtr<FJsonValue>>& Pts = Line->AsArray();
		TArray<FVector> Q;
		for (const TSharedPtr<FJsonValue>& P : Pts) Q.Add(FVector(Nv(P, 0) * 100.f, Nv(P, 1) * 100.f, 0.f));
		for (int32 k = 0; k < Q.Num(); ++k)
		{
			const FVector Dir = (Q[FMath::Min(k + 1, Q.Num() - 1)] - Q[FMath::Max(k - 1, 0)]).GetSafeNormal2D();
			const FVector Land(-Dir.Y, Dir.X, 0.f);   // à droite : la terre
			float Alt = BaseZ;
			const float Dm = NearTrack(Q[k].X, Q[k].Y, &Alt);
			const FVector In = Q[k] + Land * 1000.f;
			Q[k].Z = FMath::Max(WaterZ + 150.f, Dm < 20.f ? Alt - 40.f : GroundZ(In.X, In.Y) + 5.f);   // Dm en m
		}
		for (int32 k = 0; k + 1 < Q.Num(); ++k)
		{
			const FVector A = Q[k], B = Q[k + 1];
			const FVector Dir = (B - A).GetSafeNormal2D();
			const FVector Sea(Dir.Y, -Dir.X, 0.f), Land = -Sea;
			const float Za = A.Z, Zb = B.Z, W0 = WaterZ - 250.f, W1 = WaterZ + 35.f;
			Stone.Quad(FVector(A.X, A.Y, W1), FVector(B.X, B.Y, W1), FVector(B.X, B.Y, Zb), FVector(A.X, A.Y, Za), Sea, FLinearColor(0.5f, 0.47f, 0.41f));
			Stone.Quad(FVector(A.X, A.Y, W0), FVector(B.X, B.Y, W0), FVector(B.X, B.Y, W1), FVector(A.X, A.Y, W1), Sea, FLinearColor(0.1f, 0.11f, 0.07f));
			// margelle claire (50 cm) puis dalles
			const FVector A1 = A + Land * 50.f, B1 = B + Land * 50.f, A2 = A + Land * 700.f, B2 = B + Land * 700.f;
			Stone.Quad(FVector(A.X, A.Y, Za + 6.f), FVector(B.X, B.Y, Zb + 6.f), FVector(B1.X, B1.Y, Zb + 6.f), FVector(A1.X, A1.Y, Za + 6.f), Up, FLinearColor(0.66f, 0.63f, 0.57f));
			Stone.Quad(FVector(A.X, A.Y, Za), FVector(B.X, B.Y, Zb), FVector(B.X, B.Y, Zb + 6.f), FVector(A.X, A.Y, Za + 6.f), Sea, FLinearColor(0.6f, 0.57f, 0.51f));
			Walk.Quad(FVector(A1.X, A1.Y, Za + 3.f), FVector(B1.X, B1.Y, Zb + 3.f), FVector(B2.X, B2.Y, Zb + 3.f), FVector(A2.X, A2.Y, Za + 3.f), Up, FLinearColor(1.05f, 1.02f, 0.96f));
		}
	}

	// --- digues (béton, parapet côté large, feu vert / rouge au bout), pontons en bois
	UHierarchicalInstancedStaticMeshComponent* Conc = AddLayer(TEXT("Cube"), FLinearColor(0.6f, 0.58f, 0.54f), FLinearColor(0.66f, 0.64f, 0.6f), 0.85f);
	UHierarchicalInstancedStaticMeshComponent* Wood = AddLayer(TEXT("Cube"), FLinearColor(0.34f, 0.25f, 0.16f), FLinearColor(0.42f, 0.31f, 0.2f), 0.8f);
	UHierarchicalInstancedStaticMeshComponent* Beacon = AddLayer(TEXT("Cylinder"), FLinearColor(0.9f, 0.9f, 0.88f), FLinearColor::Transparent, 0.5f);
	UHierarchicalInstancedStaticMeshComponent* BeaconG = AddLayer(TEXT("Cylinder"), FLinearColor(0.05f, 0.55f, 0.2f), FLinearColor::Transparent, 0.4f);
	UHierarchicalInstancedStaticMeshComponent* BeaconR = AddLayer(TEXT("Cylinder"), FLinearColor(0.7f, 0.05f, 0.04f), FLinearColor::Transparent, 0.4f);
	int32 Jn = 0;
	for (const TSharedPtr<FJsonValue>& J : DecoArr(TEXT("jetties")))
	{
		const FVector A(Nv(J, 0) * 100.f, Nv(J, 1) * 100.f, 0.f), B(Nv(J, 2) * 100.f, Nv(J, 3) * 100.f, 0.f);
		const float Wd = FMath::Max(4.f, Nv(J, 4)), Len = float(FVector::Dist2D(A, B)) / 100.f;
		const FVector Dir = (B - A).GetSafeNormal2D(), Side(-Dir.Y, Dir.X, 0.f);
		const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f);
		const FVector Mid = (A + B) / 2.f;
		Conc->AddInstance(FTransform(Rot, FVector(Mid.X, Mid.Y, WaterZ - 20.f), FVector(Len, Wd, 5.6f)));
		Conc->AddInstance(FTransform(Rot, FVector(Mid.X, Mid.Y, WaterZ) + Side * (Wd * 50.f - 40.f) + Up * 325.f, FVector(Len, 0.8f, 1.3f)));
		const FVector Tip = B + Dir * 150.f;
		Beacon->AddInstance(FTransform(Rot, FVector(Tip.X, Tip.Y, WaterZ + 260.f + 250.f), FVector(1.6f, 1.6f, 5.f)));
		(Jn % 2 == 0 ? BeaconG : BeaconR)->AddInstance(FTransform(Rot, FVector(Tip.X, Tip.Y, WaterZ + 260.f + 560.f), FVector(1.7f, 1.7f, 1.2f)));
		++Jn;
	}
	for (const TSharedPtr<FJsonValue>& Pn : DecoArr(TEXT("pontoons")))
	{
		const FVector A(Nv(Pn, 0) * 100.f, Nv(Pn, 1) * 100.f, 0.f), B(Nv(Pn, 2) * 100.f, Nv(Pn, 3) * 100.f, 0.f);
		const FVector Dir = (B - A).GetSafeNormal2D(), Mid = (A + B) / 2.f;
		Wood->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.f), FVector(Mid.X, Mid.Y, WaterZ + 20.f), FVector(FVector::Dist2D(A, B) / 100.f, 2.6f, 0.5f)));
	}

	// --- yachts et voiliers (coque : blanche le plus souvent, parfois marine, grise, ivoire ou noire)
	{
		static const uint32 Hulls[8] = { 0xf4f4f2, 0xf2f1ec, 0xf6f6f6, 0x1d2a44, 0xe9e2cf, 0x3a3f46, 0xf4f4f2, 0x15171a };
		int32 NY = 0;
		for (const TSharedPtr<FJsonValue>& Yv : DecoArr(TEXT("yachts")))
		{
			const bool bSail = Nv(Yv, 4) > 0.5f;
			const float Len = Nv(Yv, 3);
			FDecorItem It;
			It.X = Nv(Yv, 0); It.Z = Nv(Yv, 1); It.Y = WaterZ / 100.f - (bSail ? 0.15f : 0.3f); It.Ry = Nv(Yv, 2);
			It.Sx = It.Sy = It.Sz = Len / (bSail ? 12.f : 30.f);
			It.Col = Srgb(Hulls[int32(Nv(Yv, 5)) & 7]);
			DecorItems.FindOrAdd(bSail ? TEXT("voilier") : TEXT("yacht")).Add(It);
			++NY;
		}
		UE_LOG(LogGPM, Log, TEXT("Décor : %d bateaux"), NY);
	}

	// --- immeubles : modèles Blender (ancien, logements, verre) à l'étage le plus proche, pied sous le point le plus bas
	{
		static const uint32 Old[8] = { 0xe8c9a0, 0xdcae86, 0xe6b9a6, 0xefe2c8, 0xe9d79e, 0xd9a59a, 0xf1ece2, 0xc9a27e };
		static const uint32 Flat[6] = { 0xeeebe4, 0xe0d6c2, 0xd2cec6, 0xe2cfb0, 0xcfc4b0, 0xf2efe8 };
		static const uint32 Glass[3] = { 0xe8ecf0, 0xc9d2da, 0x9fb3c4 };
		static const TCHAR* const Kinds[3] = { TEXT("ancien"), TEXT("logements"), TEXT("verre") };
		static const int32 Floors[3][3] = { { 3, 5, 8 }, { 5, 10, 18 }, { 8, 16, 32 } };
		int32 NB = 0, Seed = 0;
		for (const TSharedPtr<FJsonValue>& Bv : DecoArr(TEXT("buildings")))
		{
			const int32 Kd = FMath::Clamp(int32(Nv(Bv, 6)), 0, 2);
			const float Hm = FMath::Max(4.f, Nv(Bv, 5)), Wb = Nv(Bv, 3), Db = Nv(Bv, 4), Yaw = Nv(Bv, 2);
			int32 F = Floors[Kd][0];
			for (int32 f : Floors[Kd]) if (FMath::Abs(FMath::Loge(f * 3.4f / Hm)) < FMath::Abs(FMath::Loge(F * 3.4f / Hm))) F = f;
			const FString Name = FString::Printf(TEXT("imm-%s-%d"), Kinds[Kd], F);
			if (!HasDecor(*Name)) continue;
			FDecorItem It;
			It.X = Nv(Bv, 0); It.Z = Nv(Bv, 1); It.Ry = Yaw;
			const float Base = Nv(Bv, 7);
			if (Base > -500.f) It.Y = Base;
			else
			{
				// pied du bâtiment au point le plus bas de son emprise (il s'enfonce dans la pente côté amont)
				const FVector2D Fw(FMath::Sin(Yaw), FMath::Cos(Yaw)), Rt(Fw.Y, -Fw.X);
				float Low = GroundZ(It.X * 100.f, It.Z * 100.f);
				for (int32 c = 0; c < 4; ++c)
				{
					const FVector2D Q = FVector2D(It.X, It.Z) + Rt * (Wb * 0.5f * (c & 1 ? 1.f : -1.f)) + Fw * (Db * 0.5f * (c & 2 ? 1.f : -1.f));
					Low = FMath::Min(Low, GroundZ(Q.X * 100.f, Q.Y * 100.f));
				}
				It.Y = Low / 100.f - 0.6f;
			}
			It.Sx = Wb / 20.f; It.Sz = Db / 20.f; It.Sy = Hm / (F * 3.4f);
			const uint32 H = GPMCityGeo::Hash32(uint32(++Seed) * 2654435761u);
			It.Col = Srgb(Kd == 0 ? Old[H % 8] : Kd == 1 ? Flat[H % 6] : Glass[H % 3]);
			DecorItems.FindOrAdd(Name).Add(It);
			++NB;
		}
		// monuments : casino (place du casino), palais (sur le rocher), grand hôtel (extérieur de Massenet)
		for (const TSharedPtr<FJsonValue>& Lv : DecoArr(TEXT("landmarks")))
		{
			if (!Lv.IsValid() || Lv->Type != EJson::Array || Lv->AsArray().Num() < 5) continue;
			const FString Kind = Lv->AsArray()[0]->AsString();
			FDecorItem It;
			It.X = Nv(Lv, 1); It.Z = Nv(Lv, 2); It.Ry = Nv(Lv, 3);
			It.Y = GroundZ(It.X * 100.f, It.Z * 100.f) / 100.f - 0.4f;
			if (Kind == TEXT("hotel") && HasDecor(TEXT("imm-ancien-8")))
			{
				It.Sx = 70.f / 20.f; It.Sz = 32.f / 20.f; It.Sy = 28.f / 27.2f; It.Col = Srgb(0xefe4cc);
				DecorItems.FindOrAdd(TEXT("imm-ancien-8")).Add(It);
			}
			else if (HasDecor(*Kind))
			{
				It.Col = Srgb(Kind == TEXT("palais") ? 0xe3b48d : 0xf2ead8);
				DecorItems.FindOrAdd(Kind).Add(It);
			}
		}
		UE_LOG(LogGPM, Log, TEXT("Décor : %d immeubles le long des rues et sur le coteau"), NB);
	}

	// --- tribunes du port : 3 gradins, sièges de couleur, spectateurs, toit sur poteaux
	{
		UHierarchicalInstancedStaticMeshComponent* SConc = AddLayer(TEXT("Cube"), FLinearColor(0.55f, 0.55f, 0.56f), FLinearColor(0.62f, 0.62f, 0.63f), 0.9f);
		UHierarchicalInstancedStaticMeshComponent* SSeat = AddLayer(TEXT("Cube"), FLinearColor(0.75f, 0.06f, 0.05f), FLinearColor(0.85f, 0.85f, 0.86f), 0.6f);
		UHierarchicalInstancedStaticMeshComponent* SRoof = AddLayer(TEXT("Cube"), FLinearColor(0.9f, 0.9f, 0.92f), FLinearColor(0.8f, 0.82f, 0.85f), 0.4f);
		UHierarchicalInstancedStaticMeshComponent* FanA = AddLayer(TEXT("Cube"), FLinearColor(0.75f, 0.08f, 0.06f), FLinearColor(0.95f, 0.95f, 0.95f), 0.8f);
		UHierarchicalInstancedStaticMeshComponent* FanB = AddLayer(TEXT("Cube"), FLinearColor(0.1f, 0.2f, 0.6f), FLinearColor(0.9f, 0.75f, 0.15f), 0.8f);
		FanA->SetCullDistances(0, 60000); FanB->SetCullDistances(0, 60000);
		int32 Sn = 0, StandsSkipped = 0;
		for (const TSharedPtr<FJsonValue>& Sv : DecoArr(TEXT("stands")))
		{
			const FVector C(Nv(Sv, 0) * 100.f, Nv(Sv, 1) * 100.f, 0.f);
			const float Len = FMath::Max(10.f, Nv(Sv, 3));
			int32 Ni = -1; float Alt = BaseZ;
			NearTrack(C.X, C.Y, &Alt, &Ni);
			if (Ni < 0) continue;
			const FVector Along(FMath::Sin(Nv(Sv, 2)), FMath::Cos(Nv(Sv, 2)), 0.f);
			FVector Away = C - Track->Points[Ni]; Away.Z = 0.f; Away = Away.GetSafeNormal();
			// garde-fou : toute l'emprise (rangs de -6 m à +8 m du centre, sur toute la longueur) reste à plus de 0,5 m des murs,
			// même dans un virage ou une épingle (une tribune ne doit jamais mordre sur la piste)
			bool bStandClose = false;
			for (int32 Qa = -5; Qa <= 5 && !bStandClose; ++Qa)
				for (int32 Qb = 0; Qb < 3 && !bStandClose; ++Qb)
				{
					const FVector Qp = C + Along * (Qa * Len * 10.f) + Away * ((Qb == 0 ? -6.f : Qb == 1 ? 0.f : 8.f) * 100.f);
					int32 Qi = -1;
					const float Qd = NearTrack(Qp.X, Qp.Y, nullptr, &Qi);
					if (Qi < 0) continue;
					const float Qwl = Track->WallLeft.IsValidIndex(Qi) ? Track->WallLeft[Qi] : Track->Width * 0.5f;
					const float Qwr = Track->WallRight.IsValidIndex(Qi) ? Track->WallRight[Qi] : Track->Width * 0.5f;
					if (Qd < FMath::Min(Qwl, Qwr) + 0.5f) bStandClose = true;
				}
			if (bStandClose) { ++StandsSkipped; continue; }
			const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f);
			const float Z0 = Alt - 30.f;
			for (int32 Tread = 0; Tread < 3; ++Tread)
			{
				const FVector Ps = C + Away * ((Tread - 1) * 400.f);
				const float Hm = 1.6f + Tread * 2.3f;
				SConc->AddInstance(FTransform(Rot, FVector(Ps.X, Ps.Y, Z0 + Hm * 50.f), FVector(Len, 4.f, Hm)));
				SSeat->AddInstance(FTransform(Rot, FVector(Ps.X, Ps.Y, Z0 + Hm * 100.f + 20.f), FVector(Len, 3.6f, 0.4f)));
				const int32 Seats = int32(Len / 0.68f);
				for (int32 Seat = 0; Seat < Seats; ++Seat)
				{
					const float Pick = FMath::Frac(FMath::Sin(Sn * 37.1f + Tread * 11.3f + Seat * 0.71f) * 43758.547f);
					if (Pick < 0.1f) continue;
					const FVector At = FVector(Ps.X, Ps.Y, Z0 + Hm * 100.f + 70.f) + Along * ((Seat - Seats * 0.5f) * 68.f);
					(Pick < 0.55f ? FanA : FanB)->AddInstance(FTransform(Rot, At, FVector(0.42f, 0.36f, 0.6f + 0.3f * Pick)));
				}
			}
			const FVector Pr = C + Away * 100.f;
			SRoof->AddInstance(FTransform(Rot, FVector(Pr.X, Pr.Y, Z0 + 1150.f), FVector(Len + 2.f, 14.f, 0.35f)));
			for (int32 Post = 0; Post < 4; ++Post)
			{
				const FVector Q = C + Away * 700.f + Along * ((Post - 1.5f) * Len * 30.f);
				SConc->AddInstance(FTransform(Rot, FVector(Q.X, Q.Y, Z0 + 575.f), FVector(0.4f, 0.4f, 11.5f)));
			}
			++Sn;
		}
		UE_LOG(LogGPM, Log, TEXT("Décor : %d tribunes (%d écartées : trop près de la piste)"), Sn, StandsSkipped);
	}

	// --- piscine (bassin turquoise, lignes d'eau, plage en dalles), jardin du casino avec fontaine
	{
		UHierarchicalInstancedStaticMeshComponent* Deck = AddLayer(TEXT("Cube"), FLinearColor(0.82f, 0.81f, 0.78f), FLinearColor(0.86f, 0.85f, 0.82f), 0.7f);
		UHierarchicalInstancedStaticMeshComponent* Pool = AddLayer(TEXT("Cube"), FLinearColor(0.02f, 0.32f, 0.42f), FLinearColor(0.03f, 0.36f, 0.45f), 0.06f);
		UHierarchicalInstancedStaticMeshComponent* Lane = AddLayer(TEXT("Cube"), FLinearColor(0.85f, 0.1f, 0.08f), FLinearColor(0.95f, 0.95f, 0.95f), 0.5f);
		UGameInstance* GI = GetGameInstance();
		UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
		const TSharedPtr<FJsonValue> Pv = Data && !DecoFile.IsEmpty() ? Data->GetRaw(DecoFile, TEXT("pool")) : nullptr;
		if (Pv.IsValid() && Pv->Type == EJson::Array)
		{
			const FVector C(Nv(Pv, 0) * 100.f, Nv(Pv, 1) * 100.f, 0.f);
			const float Yw = Nv(Pv, 2), Wp = Nv(Pv, 3), Dp = Nv(Pv, 4);
			const FVector Along(FMath::Sin(Yw), FMath::Cos(Yw), 0.f), Across(Along.Y, -Along.X, 0.f);
			const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f);
			float Alt = BaseZ;
			NearTrack(C.X, C.Y, &Alt);
			const float Z0 = Alt - 20.f;
			Deck->AddInstance(FTransform(Rot, FVector(C.X, C.Y, Z0 - 10.f), FVector(Dp + 10.f, Wp + 10.f, 0.6f)));
			Pool->AddInstance(FTransform(Rot, FVector(C.X, C.Y, Z0 + 22.f), FVector(Dp, Wp, 0.06f)));
			for (int32 k = 1; k < 8; ++k)
			{
				const FVector Lp = C + Across * ((k / 8.f - 0.5f) * Wp * 100.f);
				Lane->AddInstance(FTransform(Rot, FVector(Lp.X, Lp.Y, Z0 + 26.f), FVector(Dp - 1.f, 0.12f, 0.04f)));
			}
		}
		UHierarchicalInstancedStaticMeshComponent* Lawn = AddLayer(TEXT("Cylinder"), FLinearColor(0.07f, 0.19f, 0.04f), FLinearColor(0.09f, 0.23f, 0.05f), 0.95f);
		UHierarchicalInstancedStaticMeshComponent* Basin = AddLayer(TEXT("Cylinder"), FLinearColor(0.78f, 0.76f, 0.7f), FLinearColor::Transparent, 0.7f);
		for (const TSharedPtr<FJsonValue>& Gv : DecoArr(TEXT("gardens")))
		{
			const float X = Nv(Gv, 0) * 100.f, Y = Nv(Gv, 1) * 100.f, Rg = Nv(Gv, 2);
			const float Z0 = GroundZ(X, Y);
			Lawn->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, Z0 + 15.f), FVector(Rg * 2.f, Rg * 2.f, 0.4f)));
			Basin->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, Z0 + 50.f), FVector(9.f, 9.f, 0.9f)));
			Pool->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, Z0 + 92.f), FVector(8.2f, 8.2f, 0.06f)));
			Basin->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X, Y, Z0 + 250.f), FVector(0.5f, 0.5f, 3.6f)));
		}
	}

	// --- lampadaires le long des rues
	for (const TSharedPtr<FJsonValue>& Lv : DecoArr(TEXT("lamps")))
	{
		FDecorItem It;
		It.X = Nv(Lv, 0); It.Z = Nv(Lv, 1); It.Ry = Nv(Lv, 2);
		float Alt = BaseZ;
		int32 Lmi = -1;
		const float Dm = NearTrack(It.X * 100.f, It.Z * 100.f, &Alt, &Lmi);
		// garde-fou : jamais de lampadaire sur l'asphalte ni entre l'axe et les murs (épingle : l'autre branche passe tout près)
		if (Lmi >= 0)
		{
			const float Lwl = Track->WallLeft.IsValidIndex(Lmi) ? Track->WallLeft[Lmi] : Track->Width * 0.5f;
			const float Lwr = Track->WallRight.IsValidIndex(Lmi) ? Track->WallRight[Lmi] : Track->Width * 0.5f;
			if (Dm < FMath::Min(Lwl, Lwr) + 0.3f) continue;
		}
		It.Y = (Dm < 15.f ? Alt : GroundZ(It.X * 100.f, It.Z * 100.f)) / 100.f - 0.05f;
		DecorItems.FindOrAdd(TEXT("lampadaire")).Add(It);
	}

	// --- tunnel couvert
	BuildTunnel();

	// --- maillage : eau (section 0), pierre (1), promenade dallée (2)
	DecoMesh = NewObject<UProceduralMeshComponent>(this);
	DecoMesh->SetupAttachment(RootComponent);
	DecoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecoMesh->bAffectDistanceFieldLighting = false;
	DecoMesh->CastShadow = true;   // le toit du tunnel fait de l'ombre sur la piste
	auto Section = [&](int32 Sec, const GPMCityGeo::FBuf& B, UMaterialInterface* M)
	{
		if (B.I.Num() == 0) return;
		DecoMesh->CreateMeshSection_LinearColor(Sec, B.V, B.I, B.N, B.UV, B.C, TArray<FProcMeshTangent>(), false);
		DecoMesh->SetMaterial(Sec, M ? M : VertexMat.Get());
	};
	UMaterialInstanceDynamic* SeaM = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	if (SeaM)
	{
		SeaM->SetScalarParameterValue(TEXT("Rough"), 1.f);
		SeaM->SetScalarParameterValue(TEXT("Metal"), 0.25f);
		SeaM->SetScalarParameterValue(TEXT("Spec"), 0.6f);
	}
	UMaterialInstanceDynamic* StoneM = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	if (StoneM)
	{
		StoneM->SetScalarParameterValue(TEXT("Rough"), 0.9f);
		StoneM->SetScalarParameterValue(TEXT("Spec"), 0.35f);
	}
	Section(0, Water, SeaM);
	GPMCityGeo::FBuf Shell;
	Shell.V = MoveTemp(TunnelV); Shell.N = MoveTemp(TunnelN); Shell.UV.Init(FVector2D::ZeroVector, Shell.V.Num()); Shell.C = MoveTemp(TunnelC); Shell.I = MoveTemp(TunnelI);
	// la pierre des quais et la coque du tunnel partagent le même matériau
	{
		const int32 Off = Stone.V.Num();
		Stone.V.Append(Shell.V); Stone.N.Append(Shell.N); Stone.UV.Append(Shell.UV); Stone.C.Append(Shell.C);
		for (int32 Ix : Shell.I) Stone.I.Add(Ix + Off);
	}
	Section(1, Stone, StoneM);
	Section(2, Walk, GPMArt::Surface(TEXT("concrete"), 4.f, this));
	DecoMesh->RegisterComponent();
}

// Tunnel : murs, plafond sombre, toit en béton (le grand hôtel est posé dessus), façades des deux bouches,
// deux rangées de néons au plafond et une lampe tous les 28 m qui éclaire vraiment les voitures.
void AGPMScenery::BuildTunnel()
{
	TunnelV.Reset(); TunnelN.Reset(); TunnelC.Reset(); TunnelI.Reset();
	if (!Track || Track->TunK0 < 0 || Track->TunK1 <= Track->TunK0) return;
	const int32 N = Track->Points.Num();
	const int32 K0 = FMath::Clamp(Track->TunK0, 0, N - 1), K1 = FMath::Clamp(Track->TunK1, 0, N - 1);
	GPMCityGeo::FBuf B;
	const FVector Up = FVector::UpVector;
	const float Half = Track->Width * 50.f, Hc = 560.f, Slab = 90.f;
	auto Tan = [&](int32 i) { return (Track->Points[(i + 1) % N] - Track->Points[(i - 1 + N) % N]).GetSafeNormal2D(); };
	auto Rt = [&](int32 i) { const FVector Tg = Tan(i); return FVector(-Tg.Y, Tg.X, 0.f); };   // droite du pilote
	auto Wd = [&](const TArray<float>& W, int32 i) { return FMath::Max((W.IsValidIndex(i) ? W[i] : Track->Width * 0.5f + 2.f) * 100.f, Half + 170.f) + 25.f; };
	const FLinearColor WallLo(0.07f, 0.07f, 0.075f), WallHi(0.2f, 0.2f, 0.21f), Ceil(0.045f, 0.045f, 0.05f), Roof(0.4f, 0.39f, 0.37f), Facade(0.52f, 0.48f, 0.42f);
	for (int32 i = K0; i < K1; ++i)
	{
		const int32 j = i + 1;
		const FVector Pi = Track->Points[i], Pj = Track->Points[j];
		const FVector Ri = Rt(i), Rj = Rt(j);
		const float Li = Wd(Track->WallLeft, i), Lj = Wd(Track->WallLeft, j), Ri0 = Wd(Track->WallRight, i), Rj0 = Wd(Track->WallRight, j);
		const FVector LiP = Pi - Ri * Li, LjP = Pj - Rj * Lj, RiP = Pi + Ri * Ri0, RjP = Pj + Rj * Rj0;
		// murs intérieurs (bas sombre jusqu'à 1,2 m, puis carrelage gris)
		B.Quad(LiP - Up * 20.f, LjP - Up * 20.f, LjP + Up * 120.f, LiP + Up * 120.f, Ri, WallLo);
		B.Quad(LiP + Up * 120.f, LjP + Up * 120.f, LjP + Up * Hc, LiP + Up * Hc, Ri, WallHi);
		B.Quad(RiP - Up * 20.f, RjP - Up * 20.f, RjP + Up * 120.f, RiP + Up * 120.f, -Ri, WallLo);
		B.Quad(RiP + Up * 120.f, RjP + Up * 120.f, RjP + Up * Hc, RiP + Up * Hc, -Ri, WallHi);
		// plafond et toit
		B.Quad(LiP + Up * Hc, LjP + Up * Hc, RjP + Up * Hc, RiP + Up * Hc, -Up, Ceil);
		const FVector LoI = Pi - Ri * (Li + 120.f), LoJ = Pj - Rj * (Lj + 120.f), RoI = Pi + Ri * (Ri0 + 120.f), RoJ = Pj + Rj * (Rj0 + 120.f);
		B.Quad(LoI + Up * (Hc + Slab), LoJ + Up * (Hc + Slab), RoJ + Up * (Hc + Slab), RoI + Up * (Hc + Slab), Up, Roof);
		// faces extérieures : côté mer (façade en pierre), côté terre
		B.Quad(LoI - Up * 300.f, LoJ - Up * 300.f, LoJ + Up * (Hc + Slab), LoI + Up * (Hc + Slab), -Ri, Facade);
		B.Quad(RoI - Up * 300.f, RoJ - Up * 300.f, RoJ + Up * (Hc + Slab), RoI + Up * (Hc + Slab), Ri, Facade * 0.9f);
		// épaisseur des murs (dessus des murs entre l'intérieur et l'extérieur, sous le toit : invisible ; bas : sol)
	}
	// façades des bouches : tablier vu de face et bandeau sombre au-dessus
	for (int32 End = 0; End < 2; ++End)
	{
		const int32 i = End ? K1 : K0;
		const FVector P = Track->Points[i], R = Rt(i);
		const FVector Fw = Tan(i) * (End ? 1.f : -1.f);
		const float L = Wd(Track->WallLeft, i) + 120.f, Rr = Wd(Track->WallRight, i) + 120.f;
		const FVector A = P - R * L, Bq = P + R * Rr;
		B.Quad(A + Up * Hc, Bq + Up * Hc, Bq + Up * (Hc + Slab), A + Up * (Hc + Slab), Fw, Facade);
		B.Quad(A + Up * (Hc + Slab), Bq + Up * (Hc + Slab), Bq + Up * (Hc + Slab + 160.f), A + Up * (Hc + Slab + 160.f), Fw, FLinearColor(0.12f, 0.12f, 0.13f));
		B.Quad(A + Up * (Hc + Slab + 160.f), Bq + Up * (Hc + Slab + 160.f), Bq + Up * (Hc + Slab + 160.f) - Fw * 120.f, A + Up * (Hc + Slab + 160.f) - Fw * 120.f, Up, Roof);
	}
	TunnelV = MoveTemp(B.V); TunnelN = MoveTemp(B.N); TunnelC = MoveTemp(B.C); TunnelI = MoveTemp(B.I);

	// néons (émissifs) : deux rangées tous les 4 m
	UHierarchicalInstancedStaticMeshComponent* Neon = AddLayer(TEXT("Cube"), FLinearColor(1.f, 0.9f, 0.7f), FLinearColor::Transparent, 0.3f);
	if (UMaterialInstanceDynamic* NM = Cast<UMaterialInstanceDynamic>(Neon->GetMaterial(0))) NM->SetVectorParameterValue(TEXT("Glow"), FLinearColor(9.f, 7.2f, 4.6f));
	for (int32 i = K0 + 1; i < K1; i += 2)
	{
		const FVector P = Track->Points[i], R = Rt(i);
		const FVector Tg = Tan(i);
		const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(Tg.Y, Tg.X)), 0.f);
		for (const float Lat : { -190.f, 190.f })
			Neon->AddInstance(FTransform(Rot, P + R * Lat + Up * (Hc - 6.f), FVector(1.4f, 0.22f, 0.07f)));
	}
	// vraies lampes (sans ombre) : les voitures passent dans des flaques de lumière orangée
	for (int32 i = K0 + 6; i < K1 - 3; i += 14)
	{
		UPointLightComponent* PL = NewObject<UPointLightComponent>(this);
		PL->SetupAttachment(RootComponent);
		PL->SetMobility(EComponentMobility::Movable);
		PL->IntensityUnits = ELightUnits::Candelas;
		PL->SetIntensity(70.f);
		PL->SetAttenuationRadius(1700.f);
		PL->SetSourceRadius(60.f);
		PL->SetLightColor(FLinearColor(1.f, 0.78f, 0.52f));
		PL->SetCastShadows(false);
		PL->SetRelativeLocation(Track->Points[i] + Up * (Hc - 40.f));
		PL->RegisterComponent();
		TunnelLights.Add(PL);
	}
	UE_LOG(LogGPM, Log, TEXT("Décor : tunnel de %d m, %d lampes"), (K1 - K0) * 2, TunnelLights.Num());
}
