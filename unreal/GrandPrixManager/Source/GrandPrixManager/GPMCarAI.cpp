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

	// voitures autour : devant sur la même trajectoire, et à côté (pour ne pas se rabattre sur elles)
	AGPMCar* Ahead = nullptr;
	float Gap = 1e9f;
	float SideL = -1e9f, SideR = 1e9f;   // limites de trajectoire imposées par une voiture à gauche / à droite (m)
	for (AGPMCar* O : Field)
	{
		if (!O || O == this || O->PitPhase) continue;
		int32 Di = (O->Idx - Idx + N) % N;
		if (Di > N / 2) Di -= N;
		const float D = Di * SplineDS, DLat = O->Lat - Lat;   // D > 0 : devant
		if (D > 0 && D < 45 && FMath::Abs(DLat) < (bSCMode ? 99.f : 2.6f) && D < Gap) { Gap = D; Ahead = O; }
		// roues dans les roues : on garde 2,7 m d'écart latéral
		if (!bSCMode && FMath::Abs(D) < 6.5f && FMath::Abs(DLat) < 4.2f)
		{
			if (DLat < 0.f || (DLat == 0.f && D > 0.f)) SideL = FMath::Max(SideL, O->Lat + 2.7f);
			else SideR = FMath::Min(SideR, O->Lat - 2.7f);
		}
	}
	PassTimer -= Dt;
	const float Lim = T.Width / 2.f - 1.4f;
	float LaneTarget = T.RL[Idx] * 0.85f + LaneBias;
	float VCap = 1e9f;
	if (bSCMode)
	{
		// voiture de sécurité : file indienne, pas de dépassement
		if (Ahead) VCap = Ahead->V + (Gap - 16) * 0.6f;
	}
	else if (Ahead && (Ahead->V < V + 3 || Gap < 14))
	{
		if (PassTimer <= 0)
		{
			// on ne se décale que du côté libre : personne à côté ni juste devant sur la trajectoire de dépassement
			auto Clear = [this, Ahead, N, Lim](float Target)
			{
				if (FMath::Abs(Target) > Lim) return false;
				for (const AGPMCar* O : Field)
				{
					if (!O || O == this || O == Ahead || O->PitPhase) continue;
					int32 Dk = (O->Idx - Idx + N) % N;
					if (Dk > N / 2) Dk -= N;
					if (Dk * SplineDS > -9.f && Dk * SplineDS < 30.f && FMath::Abs(O->Lat - Target) < 2.8f) return false;
				}
				return true;
			};
			const float L = Ahead->Lat - 3.4f, R = Ahead->Lat + 3.4f;
			const bool bLeftOk = Clear(L), bRightOk = Clear(R);
			PassSide = bLeftOk && bRightOk ? (FMath::Abs(L) < FMath::Abs(R) ? -1.f : 1.f) : bLeftOk ? -1.f : bRightOk ? 1.f : 0.f;
			PassTimer = PassSide == 0.f ? 0.7f : 2.5f;
		}
		if (PassSide != 0.f) LaneTarget = Ahead->Lat + PassSide * 3.4f;
		// distance de sécurité : on suit sans percuter tant qu'on n'est pas décalé (plus d'écart à haute vitesse)
		const float Keep = 5.f + FMath::Max(V, 0.f) * 0.08f;
		if (Gap < Keep + 12.f) VCap = Ahead->V + (Gap - Keep) * 0.5f;
	}

	// consigne d'équipe : l'équipier n°2 s'écarte et lève le pied quand le joueur le suit de près
	bYielding = false;
	if (AGPMCar* P = YieldTo; P && Lap >= 2 && !bSCMode)
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
	// pas de rabattement sur une voiture à côté (prise en sandwich : on reste au milieu de l'espace)
	if (SideL > SideR) LaneTarget = 0.5f * (SideL + SideR);
	else LaneTarget = FMath::Clamp(LaneTarget, SideL, SideR);
	LaneTarget = FMath::Clamp(LaneTarget, -Lim, Lim);
	Lane += (LaneTarget - Lane) * FMath::Min(1.f, Dt * 1.5f);

	Steer = Pursuit(Lane);

	// vitesse cible
	float Vt = T.AISpeed.IsValidIndex(Idx) ? T.AISpeed[Idx] : 40.f;
	for (int32 k = 1; k <= 4; ++k) if (T.AISpeed.IsValidIndex((Idx + k) % N)) Vt = FMath::Min(Vt, T.AISpeed[(Idx + k) % N]);
	// adhérence du moment (gomme, usure, piste mouillée, poids de l'essence) et appui perdu (aileron abîmé), comme ai.js
	const float GripNow = FMath::Max(0.2f, GripMul / (1.f + (Mass - 1.f) * 0.25f) * (0.4f + 0.6f * DmgKd));
	Vt = FMath::Min(Vt * Skill * (bSCMode ? (Ahead ? 0.72f : 0.9f) : 1.f) * FMath::Sqrt(GripNow), VCap);
	if (bYielding) Vt *= 0.9f;
	if (bOffTrack) Vt = FMath::Min(Vt, 25.f);
	if (V < Vt - 0.8f) { Throttle = 1; Brake = 0; }
	else if (V > Vt + 1.2f) { Throttle = 0; Brake = FMath::Min(1.f, (V - Vt) / 6.f); }
	else { Throttle = 0.45f; Brake = 0; }
}
