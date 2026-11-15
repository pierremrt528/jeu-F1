// Décors de circuit modélisés dans Blender (portage de js/decor.js et de placeDecor() de js/scenery.js) : stands des écuries
// et motorhomes, panneaux publicitaires, postes de commissaires, murs de pneus, tours de télévision, mâts d'éclairage,
// passerelle et immeubles. Les modèles (Content/Data/decor.json, converti depuis assets/decor/*.bin) sont dans le repère
// du jeu web (x, y haut, z, en m) : chaque instance est placée comme dans le jeu web puis convertie en repère Unreal
// (x, z, y en cm, comme les points du circuit). Toutes les instances d'un modèle sont fusionnées en un seul maillage
// (un appel de rendu par modèle) avec la couleur par sommet : la teinte de l'instance ne colore que les faces prévues (K = 0).
#include "GPMScenery.h"
#include "GrandPrixManager.h"
#include "GPMTypes.h"
#include "GPMDataSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"

namespace
{
	// palette des décors (props.py : PPAL) ; 0 = teinte de l'instance, 11 = panneau publicitaire
	const uint32 DecorPal[16] = { 0xffffff, 0xf2f2ef, 0xb9bcc0, 0x3a3e44, 0x141619, 0x7f9db5, 0xd42a2a, 0xf2c21a, 0x1c1c1e, 0xe8742a, 0x2f8f4a, 0xffffff, 0x2a3a4a, 0xfff3c4, 0xc9b79c, 0x6d7278 };
	// panneaux publicitaires sans texture : 8 marques fictives aux couleurs franches
	const uint32 AdPal[8] = { 0xe23b2e, 0x1f6fd1, 0xf2c21a, 0x22a35a, 0xff7a1a, 0x7a3ad1, 0x10a7b8, 0xf2f2ef };
	FLinearColor Hex(uint32 H) { return FLinearColor(FColor((H >> 16) & 255, (H >> 8) & 255, H & 255)); }   // sRGB → linéaire
	// rugosité par couleur (alpha du sommet, matériau M_GPM_Lit) : vitres et métal brillants, le reste mat
	float Rough(int32 K) { return K == 5 || K == 12 ? 0.18f : K == 2 || K == 15 ? 0.4f : K == 11 ? 0.55f : 0.85f; }
	uint32 Mix(uint32 A) { A ^= A >> 16; A *= 0x7feb352d; A ^= A >> 15; A *= 0x846ca68b; A ^= A >> 16; return A; }
}

void AGPMScenery::LoadDecor()
{
	DecorItems.Reset();
	if (DecorModels.Num()) return;
	UGameInstance* GI = GetGameInstance();
	UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	static const TCHAR* Names[] = { TEXT("garage"), TEXT("motorhome"), TEXT("panneau"), TEXT("commissaires"), TEXT("pneus"), TEXT("camera"), TEXT("projecteur"),
		TEXT("pilier"), TEXT("tablier"), TEXT("imm-verre-8"), TEXT("imm-verre-16"), TEXT("imm-verre-32"), TEXT("imm-logements-5"), TEXT("imm-logements-10"),
		TEXT("imm-logements-18"), TEXT("imm-ancien-3"), TEXT("imm-ancien-5"), TEXT("imm-ancien-8") };
	for (const TCHAR* Name : Names)
	{
		const TSharedPtr<FJsonValue> V = Data ? Data->GetRaw(TEXT("decor"), Name) : nullptr;
		if (!V.IsValid() || V->Type != EJson::Object) continue;
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr, * N = nullptr, * K = nullptr, * I = nullptr;
		if (!O->TryGetArrayField(TEXT("P"), P) || !O->TryGetArrayField(TEXT("N"), N) || !O->TryGetArrayField(TEXT("K"), K) || !O->TryGetArrayField(TEXT("I"), I)) continue;
		FDecorModel M;
		const int32 NV = FMath::Min3(P->Num() / 3, N->Num() / 3, K->Num());
		for (int32 v = 0; v < NV; ++v)
		{
			M.P.Add(FVector3f((*P)[v * 3]->AsNumber(), (*P)[v * 3 + 1]->AsNumber(), (*P)[v * 3 + 2]->AsNumber()));
			M.N.Add(FVector3f((*N)[v * 3]->AsNumber(), (*N)[v * 3 + 1]->AsNumber(), (*N)[v * 3 + 2]->AsNumber()));
			M.K.Add(uint8((*K)[v]->AsNumber()));
		}
		for (const TSharedPtr<FJsonValue>& X : *I) { const int32 Ix = int32(X->AsNumber()); if (Ix >= 0 && Ix < NV) M.I.Add(Ix); }
		M.I.SetNum(M.I.Num() / 3 * 3);
		double Ad = -1;
		O->TryGetNumberField(TEXT("ad"), Ad);
		M.Ad = int32(Ad);
		if (M.I.Num()) DecorModels.Add(Name, MoveTemp(M));
	}
	UE_LOG(LogGPM, Log, TEXT("Décors Blender : %d modèles"), DecorModels.Num());
}

void AGPMScenery::PlaceDecor()
{
	if (DecorModels.Num() == 0 || !Track) return;
	const FGPMTrackData& T = *Track;
	const int32 N = T.WX.Num();
	if (N < 50) return;
	UGameInstance* GI = GetGameInstance();
	UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	const float W = T.Width;
	auto Wrap = [N](int32 i) { return ((i % N) + N) % N; };
	auto PtX = [&](int32 i, float Lat) { return T.WX[i] - T.TZ[i] * Lat; };
	auto PtZ = [&](int32 i, float Lat) { return T.WZ[i] + T.TX[i] * Lat; };
	auto WR = [&](int32 i) { return T.WallRight.IsValidIndex(i) ? T.WallRight[i] : W / 2.f + 9.f; };
	auto WL = [&](int32 i) { return T.WallLeft.IsValidIndex(i) ? T.WallLeft[i] : W / 2.f + 9.f; };
	auto Curv = [&](int32 i) { return T.Curvature.IsValidIndex(i) ? T.Curvature[i] : 0.f; };
	// courbure lissée sur ±15 points (Cw de scenery.js)
	TArray<float> Cw; Cw.SetNumZeroed(N);
	for (int32 i = 0; i < N; ++i) { float S = 0.f; for (int32 k = -15; k <= 15; ++k) S += Curv(Wrap(i + k)); Cw[i] = S / 31.f; }
	auto InPit = [&](int32 i) { const int32 K = T.PitK(i); return K >= T.PitE0 - 2 && K <= T.PitX1 + 2; };
	TArray<FVector> Occupied;   // x, z (m), rayon
	auto Free = [&](float X, float Z, float R) { for (const FVector& O : Occupied) if (FMath::Square(O.X - X) + FMath::Square(O.Y - Z) < FMath::Square(O.Z + R)) return false; return true; };
	// une autre portion de piste trop proche (croisement, épingle) : pas de décor ici
	auto OkAt = [&](int32 i, float S, float Off, float& OX, float& OZ)
	{
		if ((S > 0.f && InPit(i)) || FMath::Min(i, N - i) < 40) return false;
		OX = PtX(i, S * Off); OZ = PtZ(i, S * Off);
		int32 Near = -1;
		const float D = NearTrack(OX * 100.f, OZ * 100.f, nullptr, &Near);
		if (Near >= 0 && FMath::Min(FMath::Abs(Near - i), N - FMath::Abs(Near - i)) > 20 && D < W / 2.f + 10.f) return false;
		return true;
	};
	auto Add = [&](const TCHAR* Name, float X, float Y, float Z, float Ry, const FLinearColor& Col = FLinearColor::White, int32 AdI = 0)
	{
		if (!HasDecor(Name)) return;
		FDecorItem It; It.X = X; It.Y = Y; It.Z = Z; It.Ry = Ry; It.Col = Col; It.Ad = AdI;
		DecorItems.FindOrAdd(Name).Add(It);
	};
	auto Ang = [&](int32 i, float S) { return T.H[i] + S * PI / 2.f; };   // face avant (+z du modèle) tournée vers la piste
	uint32 Seed = uint32(T.Id.Len() * 977 + N);
	auto Rd = [&Seed]() { Seed = Mix(Seed + 0x9E3779B9u); return (Seed & 0xffffff) / 16777216.f; };

	// tribunes de la ligne droite (BuildStands) : place prise
	for (int32 S = 0; S < 4; ++S) { const int32 i = Wrap(-60 + S * 24); Occupied.Add(FVector(PtX(i, -(WL(i) + 8.f)), PtZ(i, -(WL(i) + 8.f)), 30.f)); }

	// stands des écuries le long de la voie des stands, motorhomes derrière (teinte : écurie du carré le plus proche)
	if (HasDecor(TEXT("garage")) && Data && Data->Teams.Num())
	{
		const float PitOff = T.PitLat + 3.6f + 1.5f + 8.f;
		auto TeamAt = [&](int32 i)
		{
			const int32 K = T.PitK(i);
			int32 B = 0;
			for (int32 n = 0; n < Data->Teams.Num(); ++n) if (FMath::Abs(FGPMTrackData::PitBox(n) - K) < FMath::Abs(FGPMTrackData::PitBox(B) - K)) B = n;
			return Data->Teams[B].Color;
		};
		for (int32 K = -4 * 31 + 8 - 15; K <= 1 * 31 + 8 + 15; K += 6)
		{
			const int32 i = Wrap(K);
			Add(TEXT("garage"), PtX(i, PitOff), T.WY[i], PtZ(i, PitOff), T.H[i] + PI / 2.f, TeamAt(i));
		}
		for (int32 n = 0; n < Data->Teams.Num(); ++n)
		{
			const int32 i = Wrap(-4 * 31 + 8 + n * 18);
			Add(TEXT("motorhome"), PtX(i, PitOff + 15.f), T.WY[i], PtZ(i, PitOff + 15.f), T.H[i] + PI / 2.f, Data->Teams[n].Color);
		}
		for (int32 K = -4 * 31 + 8 - 15; K <= 1 * 31 + 8 + 15; K += 10) { const int32 i = Wrap(K); Occupied.Add(FVector(PtX(i, PitOff), PtZ(i, PitOff), 30.f)); }
	}

	// panneaux publicitaires derrière le mur, alternés, plus nombreux à l'extérieur des virages
	{
		float Side = 1.f;
		for (int32 i = 0; i < N; i += FMath::Max(1, FMath::RoundToInt((150.f + Rd() * 90.f) / 2.f)))
		{
			const float S = FMath::Abs(Cw[i]) > 1.f / 300.f ? (Cw[i] > 0.f ? 1.f : -1.f) : (Side = -Side);
			const float Off = (S > 0.f ? WR(i) : WL(i)) + 6.f;
			float X, Z;
			if (!OkAt(i, S, Off, X, Z) || !Free(X, Z, 8.f)) continue;
			Add(TEXT("panneau"), X, T.WY[i] - 0.4f, Z, Ang(i, S), FLinearColor::White, int32(Rd() * 8.f));
			Occupied.Add(FVector(X, Z, 8.f));
		}
	}
	// postes de commissaires tous les ~300 m, côté extérieur du virage suivant
	for (int32 i = 60; i < N; i += 150)
	{
		float S = 1.f;
		for (int32 k = 0; k < 60; ++k) { const float C = Cw[Wrap(i + k)]; if (FMath::Abs(C) > 1.f / 250.f) { S = C > 0.f ? 1.f : -1.f; break; } }
		const float Off = (S > 0.f ? WR(i) : WL(i)) + 2.6f;
		float X, Z;
		if (!OkAt(i, S, Off, X, Z) || !Free(X, Z, 3.f)) continue;
		Add(TEXT("commissaires"), X, T.WY[i] - 0.2f, Z, Ang(i, S));
		Occupied.Add(FVector(X, Z, 3.f));
	}
	// murs de pneus devant le mur à l'extérieur des virages les plus serrés (bande aux couleurs des vibreurs : rouge)
	{
		int32 Count = 0;
		for (int32 i = 0; i < N && Count < 120; i += 3)
		{
			if (FMath::Abs(Cw[i]) <= 1.f / 95.f) continue;
			const float S = Cw[i] > 0.f ? 1.f : -1.f, Off = (S > 0.f ? WR(i) : WL(i)) - 0.7f;
			if (Off < W / 2.f + 3.f) continue;
			float X, Z;
			if (!OkAt(i, S, Off, X, Z)) continue;
			Add(TEXT("pneus"), X, T.WY[i], Z, Ang(i, S), Hex(0xd42a2a));
			++Count;
		}
	}
	// tours de télévision à l'extérieur des grands virages, 4 au plus, espacées
	{
		TArray<int32> Big;
		for (int32 i = 0; i < N; ++i) if (FMath::Abs(Cw[i]) > 1.f / 160.f) Big.Add(i);
		Big.Sort([&](int32 A, int32 B) { return FMath::Abs(Cw[A]) > FMath::Abs(Cw[B]); });
		TArray<int32> Cams;
		for (int32 i : Big)
		{
			if (Cams.Num() >= 4) break;
			bool bNear = false;
			for (int32 c : Cams) bNear |= FMath::Min(FMath::Abs(c - i), N - FMath::Abs(c - i)) < 120;
			if (bNear) continue;
			for (float S : { Cw[i] > 0.f ? 1.f : -1.f, Cw[i] > 0.f ? -1.f : 1.f })
			{
				const float Off = (S > 0.f ? WR(i) : WL(i)) + 10.f;
				float X, Z;
				if (OkAt(i, S, Off, X, Z) && Free(X, Z, 4.f)) { Add(TEXT("camera"), X, T.WY[i] - 0.3f, Z, Ang(i, S)); Cams.Add(i); Occupied.Add(FVector(X, Z, 4.f)); break; }
			}
		}
	}
	// mâts d'éclairage : course au crépuscule, sinon quelques-uns le long de la ligne droite des stands
	{
		const bool bNight = T.Theme.Sun.Z < 0.2f;
		const int32 Step = bNight ? 35 : 9999;
		float S = 1.f;
		for (int32 i = 0; i < N; i += Step, S = -S)
		{
			const float Off = (S > 0.f ? WR(i) : WL(i)) + 4.f;
			float X, Z;
			if (OkAt(i, S, Off, X, Z) && Free(X, Z, 2.f)) Add(TEXT("projecteur"), X, T.WY[i] - 0.3f, Z, Ang(i, S));
		}
	}
	// passerelle au-dessus de la plus longue ligne droite (loin du départ)
	if (HasDecor(TEXT("pilier")) && HasDecor(TEXT("tablier")))
	{
		int32 Best = -1, BestLen = 0, Run = 0;
		for (int32 i = 0; i < N; ++i)
		{
			Run = FMath::Abs(Curv(i)) < 1.f / 900.f ? Run + 1 : 0;
			const int32 M = Wrap(i - (Run >> 1));
			if (Run > BestLen && FMath::Min(M, N - M) > 120 && !InPit(M)) { BestLen = Run; Best = M; }
		}
		if (Best >= 0 && BestLen * 2 > 200)
		{
			const int32 i = Best;
			const float Wl = WL(i) + 2.2f, Wr = WR(i) + 2.2f;
			const float LX = PtX(i, -Wl), LZ = PtZ(i, -Wl), RX = PtX(i, Wr), RZ = PtZ(i, Wr);
			if (Free(LX, LZ, 3.f) && Free(RX, RZ, 3.f))
			{
				Add(TEXT("pilier"), LX, T.WY[i], LZ, T.H[i]);
				Add(TEXT("pilier"), RX, T.WY[i], RZ, T.H[i]);
				FDecorItem It;
				It.X = PtX(i, (Wr - Wl) / 2.f); It.Z = PtZ(i, (Wr - Wl) / 2.f); It.Y = T.WY[i] + 0.2f; It.Ry = T.H[i];
				It.Sx = (Wl + Wr) / 20.f; It.Ad = int32(Rd() * 8.f);
				DecorItems.FindOrAdd(TEXT("tablier")).Add(It);
			}
		}
	}
}

void AGPMScenery::BuildDecorMeshes()
{
	for (UProceduralMeshComponent* M : DecorMeshes) if (M) M->DestroyComponent();
	DecorMeshes.Reset();
	if (DecorItems.Num() == 0) return;
	UMaterialInstanceDynamic* Mat = GPMArt::Make(TEXT("M_GPM_Lit"), this);
	if (Mat)
	{
		Mat->SetScalarParameterValue(TEXT("Rough"), 1.f);
		Mat->SetScalarParameterValue(TEXT("Metal"), 0.f);
		Mat->SetScalarParameterValue(TEXT("Spec"), 0.5f);
	}
	int32 Total = 0;
	for (const TPair<FString, TArray<FDecorItem>>& E : DecorItems)
		for (const FDecorItem& It : E.Value)
			if (NearTrack(It.X * 100.f, It.Z * 100.f) < Track->Width / 2.f + 6.f)
				UE_LOG(LogGPM, Warning, TEXT("Décor sur la piste : %s (%.0f, %.0f)"), *E.Key, It.X, It.Z);
	for (const TPair<FString, TArray<FDecorItem>>& E : DecorItems)
	{
		const FDecorModel* M = DecorModels.Find(E.Key);
		if (!M || E.Value.Num() == 0) continue;
		TArray<FVector> V, Nrm; TArray<FLinearColor> Col; TArray<int32> Idx;
		V.Reserve(M->P.Num() * E.Value.Num()); Nrm.Reserve(V.Max()); Col.Reserve(V.Max()); Idx.Reserve(M->I.Num() * E.Value.Num());
		// sommets des faces publicitaires (triangles à partir de Ad) : couleur de la marque
		TArray<bool> IsAd; IsAd.SetNumZeroed(M->P.Num());
		if (M->Ad >= 0) for (int32 k = M->Ad; k < M->I.Num(); ++k) IsAd[M->I[k]] = true;
		for (const FDecorItem& It : E.Value)
		{
			const int32 Base = V.Num();
			const float C = FMath::Cos(It.Ry), S = FMath::Sin(It.Ry);
			for (int32 v = 0; v < M->P.Num(); ++v)
			{
				// repère web : échelle, rotation autour de y (three.js), translation ; puis Unreal (x, z, y) en cm
				const float Px = M->P[v].X * It.Sx, Py = M->P[v].Y * It.Sy, Pz = M->P[v].Z * It.Sz;
				const float Rx = Px * C + Pz * S, Rz = -Px * S + Pz * C;
				V.Add(FVector(It.X + Rx, It.Z + Rz, It.Y + Py) * 100.f);
				FVector3f Nl(M->N[v].X / It.Sx, M->N[v].Y / It.Sy, M->N[v].Z / It.Sz);
				Nl = Nl.GetSafeNormal();
				Nrm.Add(FVector(Nl.X * C + Nl.Z * S, -Nl.X * S + Nl.Z * C, Nl.Y));
				const int32 K = M->K[v];
				FLinearColor Cl = IsAd[v] || K == 11 ? Hex(AdPal[FMath::Abs(It.Ad + v / 64) % 8]) : K == 0 ? It.Col : Hex(DecorPal[K & 15]);
				Cl.A = Mat ? Rough(K) : 1.f;
				Col.Add(Cl);
			}
			for (int32 k = 0; k + 2 < M->I.Num(); k += 3)
			{
				const int32 A = Base + M->I[k], B = Base + M->I[k + 1], D = Base + M->I[k + 2];
				// face visible du côté des normales (normale de face Unreal = −(B−A)×(C−A))
				const bool bFlip = FVector::DotProduct(FVector::CrossProduct(V[B] - V[A], V[D] - V[A]), Nrm[A] + Nrm[B] + Nrm[D]) > 0.f;
				Idx.Add(A); Idx.Add(bFlip ? D : B); Idx.Add(bFlip ? B : D);
			}
		}
		UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this);
		PM->SetupAttachment(RootComponent);
		PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PM->CastShadow = E.Key == TEXT("garage") || E.Key == TEXT("motorhome") || E.Key == TEXT("tablier");
		PM->bAffectDistanceFieldLighting = false;
		PM->CreateMeshSection_LinearColor(0, V, Idx, Nrm, TArray<FVector2D>(), Col, TArray<FProcMeshTangent>(), false);
		PM->SetMaterial(0, Mat ? static_cast<UMaterialInterface*>(Mat) : VertexMat.Get());
		PM->SetCullDistance(E.Key.StartsWith(TEXT("imm-")) ? 0.f : 180000.f);
		PM->RegisterComponent();
		DecorMeshes.Add(PM);
		Total += E.Value.Num();
	}
	UE_LOG(LogGPM, Log, TEXT("Décors Blender : %d objets en %d maillages"), Total, DecorMeshes.Num());
}
