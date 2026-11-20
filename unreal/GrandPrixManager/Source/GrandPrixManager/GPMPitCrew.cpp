// Mécaniciens du carré de l'écurie du joueur : dix silhouettes (jambes, combinaison aux couleurs de l'écurie, casque)
// qui attendent devant le garage, se placent quand la voiture arrive (cric avant et arrière, pistolet et roue à chaque coin),
// s'accroupissent pendant l'arrêt puis s'écartent. Instances de cubes : 2 appels de rendu en tout.
#include "GPMScenery.h"
#include "GPMTypes.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace GPMCrewGeo
{
	// place autour de la voiture (m) : le long de la voiture, sur le côté ; cric avant, cric arrière, puis les quatre roues
	const float Work[10][2] = { { 3.6f, 0.f }, { -3.3f, 0.f }, { 1.7f, -1.35f }, { 1.7f, 1.35f }, { -1.6f, -1.35f }, { -1.6f, 1.35f },
		{ 2.3f, -1.95f }, { 2.3f, 1.95f }, { -2.2f, -1.95f }, { -2.2f, 1.95f } };
	constexpr float Walk = 650.f;   // cm/s : ils courent vers leur place
}

void AGPMScenery::BuildCrew()
{
	CrewDark = AddLayer(TEXT("Cube"), FLinearColor(0.03f, 0.03f, 0.035f), FLinearColor::Transparent, 0.6f);
	CrewSuit = AddLayer(TEXT("Cube"), FLinearColor(0.7f, 0.1f, 0.1f), FLinearColor::Transparent, 0.5f);
	CrewDark->SetCullDistances(0, 30000); CrewSuit->SetCullDistances(0, 30000);
	for (int32 k = 0; k < 10; ++k)
	{
		CrewPos[k] = FVector::ZeroVector; CrewDown[k] = 0.f;
		CrewDark->AddInstance(FTransform::Identity); CrewDark->AddInstance(FTransform::Identity);
		CrewSuit->AddInstance(FTransform::Identity);
	}
	CrewState = -1;
}

void AGPMScenery::SetCrew(int32 State, const FVector& CarPos, const FVector& CarFwd, const FLinearColor& Team, float Dt)
{
	using namespace GPMCrewGeo;
	if (!CrewDark || !CrewSuit || !Track || Track->Points.Num() < 4) return;
	if (!Team.Equals(CrewCol))
	{
		CrewCol = Team;
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(CrewSuit->GetMaterial(0)))
		{
			M->SetVectorParameterValue(TEXT("Color"), Team);
			M->SetVectorParameterValue(TEXT("Color2"), Team);
		}
	}
	// carré de l'écurie : axe de la voie des stands
	const int32 N = Track->Points.Num(), i = ((FMath::RoundToInt(FGPMTrackData::PitBox(0)) % N) + N) % N;
	const FVector Fw(Track->TX[i], Track->TZ[i], 0.f), Rt(-Track->TZ[i], Track->TX[i], 0.f);
	const FVector Box = Track->Points[i] + Rt * Track->SlotLc() * 100.f + FVector(0, 0, 2.f);
	const bool bOnCar = State > 0 && !CarPos.IsNearlyZero();
	const FVector C0 = bOnCar ? FVector(CarPos.X, CarPos.Y, Box.Z) : Box;
	const FVector F = bOnCar ? FVector(CarFwd.X, CarFwd.Y, 0.f).GetSafeNormal() : Fw, R(-F.Y, F.X, 0.f);
	const bool bSnap = CrewState < 0;
	bool bMoved = bSnap || State != CrewState;
	CrewState = State;
	for (int32 k = 0; k < 10; ++k)
	{
		FVector Want;
		if (State == 0) Want = Box + Rt * ((Track->SlotL1() - Track->SlotLc()) * 100.f + 180.f + (k / 5) * 110.f) + Fw * ((k % 5 - 2) * 120.f);
		else
		{
			const float Out = State == 1 ? 1.25f : 1.f;   // prêts : un pas en retrait, les crics loin devant et derrière
			Want = C0 + F * Work[k][0] * (State == 1 && k < 2 ? 1.5f : 1.f) * 100.f + R * Work[k][1] * Out * 100.f;
		}
		const FVector D = Want - CrewPos[k];
		if (bSnap) CrewPos[k] = Want;
		else if (D.SizeSquared() > 1.f) { CrewPos[k] += D.GetClampedToMaxSize(Walk * Dt); bMoved = true; }
		const float Down = State == 2 && k >= 2 && k < 6 && D.Size() < 30.f ? 1.f : 0.f;   // pistolets : accroupis
		if (!FMath::IsNearlyEqual(CrewDown[k], Down)) { CrewDown[k] = FMath::FInterpTo(CrewDown[k], Down, Dt, 8.f); bMoved = true; if (FMath::Abs(CrewDown[k] - Down) < 0.02f) CrewDown[k] = Down; }
	}
	if (!bMoved) return;
	for (int32 k = 0; k < 10; ++k)
	{
		// regard : vers la voiture au travail, vers la voie quand ils attendent
		const FVector Look = State > 0 ? (C0 - CrewPos[k]).GetSafeNormal2D() : -Rt;
		const FRotator Rot(0.f, Look.Rotation().Yaw, 0.f);
		const float Leg = FMath::Lerp(0.85f, 0.45f, CrewDown[k]);
		const FVector Foot = CrewPos[k];
		const bool bLast = k == 9;
		CrewDark->UpdateInstanceTransform(2 * k, FTransform(Rot, Foot + FVector(0, 0, Leg * 50.f), FVector(0.3f, 0.42f, Leg)), true, false, true);
		CrewSuit->UpdateInstanceTransform(k, FTransform(Rot, Foot + FVector(0, 0, Leg * 100.f + 32.f), FVector(0.34f, 0.5f, 0.64f)), true, bLast, true);
		CrewDark->UpdateInstanceTransform(2 * k + 1, FTransform(Rot, Foot + FVector(0, 0, Leg * 100.f + 78.f), FVector(0.27f, 0.25f, 0.27f)), true, bLast, true);
	}
}
