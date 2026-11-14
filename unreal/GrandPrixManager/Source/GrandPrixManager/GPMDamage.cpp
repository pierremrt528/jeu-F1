// Dégâts visibles de la monoplace (debris.js du jeu web) : aileron tordu d'un côté, aileron arraché (ses sommets
// disparaissent et un morceau part rebondir sur la piste, voir GPMDebris), voiture remise à neuf au stand.
#include "GPMCar.h"
#include "ProceduralMeshComponent.h"

// Repère du modèle : X vers l'avant, Y à droite (= −x du jeu web), Z vers le haut, en cm
void AGPMCar::ShapeWings()
{
	if (!CarModel || !Model) return;
	TArray<FVector> Pos = CarModel->P;
	for (int32 w = 0; w < 2; ++w)
	{
		const int32 First = CarModel->Wing[w][0], Cnt = CarModel->Wing[w][1];
		if (Cnt <= 0 || First + Cnt > Pos.Num()) continue;
		const bool bOff = w ? bRwOff : bFwOff;
		const float Side = WingBend[w];
		if (!bOff && Side == 0.f) continue;
		for (int32 k = First; k < First + Cnt; ++k)
		{
			if (bOff) { Pos[k] = FVector(0.f, 0.f, 30.f); continue; }   // sommets confondus : plus rien n'est dessiné
			// le côté touché plie vers le bas et vers l'arrière (avant) ou vers l'avant (arrière)
			const float F = FMath::Max(0.f, -Pos[k].Y * Side / 100.f);
			Pos[k].Z -= (0.13f * F * F - 0.02f * F) * 100.f;
			Pos[k].X -= (w ? -0.1f : 0.18f) * F * F * 100.f;
		}
	}
	Model->UpdateMeshSection_LinearColor(0, Pos, TArray<FVector>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>());
	bWingsShaped = true;
}

void AGPMCar::BendWing(bool bRear, float Side)
{
	WingBend[bRear ? 1 : 0] = Side < 0.f ? -1.f : 1.f;
	ShapeWings();
}

void AGPMCar::DetachWing(bool bRear)
{
	(bRear ? bRwOff : bFwOff) = true;
	ShapeWings();
}

void AGPMCar::RepairWings()
{
	bFwOff = bRwOff = false;
	WingBend[0] = WingBend[1] = 0.f;
	if (!bWingsShaped || !CarModel || !Model) return;
	Model->UpdateMeshSection_LinearColor(0, CarModel->P, TArray<FVector>(), TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>());
	bWingsShaped = false;
}

bool AGPMCar::WingPiece(bool bRear, TArray<FVector>& OutP, TArray<FVector>& OutN, TArray<FLinearColor>& OutC) const
{
	if (!CarModel) return false;
	const int32 First = CarModel->Wing[bRear ? 1 : 0][0], Cnt = CarModel->Wing[bRear ? 1 : 0][1];
	if (Cnt < 3 || First + Cnt > CarModel->P.Num() || First + Cnt > CarModel->K.Num()) return false;
	const bool bLit = Model && Model->GetMaterial(0) != VertexColorMat.Get();
	for (int32 k = First; k < First + Cnt; ++k)
	{
		OutP.Add(CarModel->P[k]);
		OutN.Add(CarModel->N.IsValidIndex(k) ? CarModel->N[k] : FVector::UpVector);
		const int32 Pal = CarModel->K[k];
		FLinearColor Col = Pal == 0 ? LiveryMain : Pal <= 2 ? LiveryAccent : CarModel->Fixed.IsValidIndex(Pal) ? CarModel->Fixed[Pal] : FLinearColor::Black;
		Col.A = bLit ? PaintRough(Pal) : 1.f;
		OutC.Add(Col);
	}
	return true;
}

UMaterialInterface* AGPMCar::BodyMaterial() const
{
	return Model ? Model->GetMaterial(0) : nullptr;
}
