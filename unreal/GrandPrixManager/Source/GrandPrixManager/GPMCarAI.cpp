// Pilotes IA : portage de js/ai.js. Suivent la trajectoire (poursuite pure), respectent le profil de vitesse,
// et se décalent pour dépasser quand une voiture plus lente est devant.
#include "GPMCar.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float SplineDS = 2.f;   // pas de la spline (m)
}

void AGPMCar::MakeAI(float InSkill)
{
	bPlayerControlled = false;
	Skill = InSkill;
	Assist.bABS = Assist.bTC = Assist.bStab = true;   // les IA ont ABS + antipatinage
	ChaseCam->SetActive(false);
	CockpitCam->SetActive(false);
	Arm->SetComponentTickEnabled(false);
}

float AGPMCar::Pursuit(float InLane) const
{
	const FGPMTrackData& T = *Track;
	const int32 N = T.WX.Num();
	const float Vv = FMath::Max(V, 1.f);
	const float Ld = 9.f + Vv * 0.5f;
	const int32 j = (Idx + FMath::RoundToInt(Ld / SplineDS)) % N;
	const float Tx = T.WX[j] - T.TZ[j] * InLane, Tz = T.WZ[j] + T.TX[j] * InLane;
	const float Dx = Tx - X, Dz = Tz - Z;
	const float Fx = FMath::Sin(Heading), Fz = FMath::Cos(Heading);
	const float Lx = Dx * -Fz + Dz * Fx, Lz = Dx * Fx + Dz * Fz;   // repère voiture : x vers la droite
	const float Alpha = FMath::Atan2(Lx, Lz);
	const float KWant = 2.f * FMath::Sin(Alpha) / FMath::Max(0.1f, FMath::Sqrt(Dx * Dx + Dz * Dz));
	const float KMax = FMath::Tan(SteerMax / (1.f + Vv / 20.f)) / WB;
	return FMath::Clamp(KWant / KMax, -1.f, 1.f);
}

void AGPMCar::DriveAI(float Dt)
{
	if (!Track || Track->RL.Num() != Track->WX.Num()) return;
	const FGPMTrackData& T = *Track;
	const int32 N = T.WX.Num();

	// voiture devant sur la même portion de piste ?
	AGPMCar* Ahead = nullptr;
	float Gap = 1e9f;
	for (AGPMCar* O : Field)
	{
		if (!O || O == this || O->PitPhase) continue;
		const float D = ((O->Idx - Idx + N) % N) * SplineDS;
		if (D > 0 && D < 40 && FMath::Abs(O->Lat - Lat) < 2.6f && D < Gap) { Gap = D; Ahead = O; }
	}
	PassTimer -= Dt;
	const float Lim = T.Width / 2.f - 1.4f;
	float LaneTarget = T.RL[Idx] * 0.85f + LaneBias;
	float VCap = 1e9f;
	if (Ahead && (Ahead->V < V + 3 || Gap < 14))
	{
		if (PassTimer <= 0)
		{
			const float L = Ahead->Lat - 3.4f, R = Ahead->Lat + 3.4f;
			PassSide = FMath::Abs(L) < FMath::Abs(R) ? -1.f : 1.f;
			PassTimer = 2.5f;
		}
		LaneTarget = Ahead->Lat + PassSide * 3.4f;
		if (Gap < 12) VCap = Ahead->V - 0.5f + Gap * 0.15f;   // ne pas percuter
	}

	// consigne d'équipe : l'équipier n°2 s'écarte et lève le pied quand le joueur le suit de près
	bYielding = false;
	if (AGPMCar* P = YieldTo; P && Lap >= 2)
	{
		const float D = Progress() - P->Progress();
		YieldCd = FMath::Max(0.f, YieldCd - Dt);
		if (D > 0 && D < 16 && YieldCd <= 0)
		{
			bYielding = true;
			LaneTarget = (P->Lat >= Lat ? -1.f : 1.f) * Lim;
			YieldRun += Dt;
			if (YieldRun > 7) { YieldCd = 40; YieldRun = 0; }   // le joueur n'arrive pas à passer : il reprend son rythme
		}
		else if (D <= 0) YieldRun = 0;
	}
	LaneTarget = FMath::Clamp(LaneTarget, -Lim, Lim);
	Lane += (LaneTarget - Lane) * FMath::Min(1.f, Dt * 1.5f);

	Steer = Pursuit(Lane);

	// vitesse cible
	float Vt = T.AISpeed.IsValidIndex(Idx) ? T.AISpeed[Idx] : 40.f;
	for (int32 k = 1; k <= 4; ++k) if (T.AISpeed.IsValidIndex((Idx + k) % N)) Vt = FMath::Min(Vt, T.AISpeed[(Idx + k) % N]);
	Vt = FMath::Min(Vt * Skill, VCap);
	if (bYielding) Vt *= 0.9f;
	if (bOffTrack) Vt = FMath::Min(Vt, 25.f);
	if (V < Vt - 0.8f) { Throttle = 1; Brake = 0; }
	else if (V > Vt + 1.2f) { Throttle = 0; Brake = FMath::Min(1.f, (V - Vt) / 6.f); }
	else { Throttle = 0.45f; Brake = 0; }
}
