// Pneus et voie des stands : portage de lifeGrip / TYRES et de pitStep() (js/strategy.js).
#include "GPMCar.h"

namespace
{
	constexpr float PitSpeed = 22.f;   // limiteur (m/s ≈ 80 km/h)
	constexpr float PitDS = 2.f;
}

// intermédiaires (I) : entre les durs et les pneus pluie au sec, rois de la piste humide
float AGPMCar::TyreGrip(TCHAR T) { return T == 'S' ? 1.09f : T == 'H' ? 0.93f : T == 'W' ? 0.86f : T == 'I' ? 0.9f : 1.f; }
float AGPMCar::TyreWpl(TCHAR T) { return T == 'S' ? 0.16f : T == 'H' ? 0.055f : T == 'W' ? 0.06f : T == 'I' ? 0.07f : 0.085f; }

// adhérence selon la vie restante du pneu (R = 1 neuf … 0 usé) : 100-85 % maximal, 85-60 % bon, 60-40 % faible, dessous presque nul
float AGPMCar::LifeGrip(float R)
{
	if (R >= 0.85f) return 1.f;
	if (R >= 0.6f) return 1.f - 0.03f * (0.85f - R) / 0.25f;
	if (R >= 0.4f) return 0.97f - 0.1f * (0.6f - R) / 0.2f;
	return FMath::Max(0.6f, 0.87f - 0.27f * (0.4f - R) / 0.4f);
}

void AGPMCar::StartPit(float StopTime)
{
	PitPhase = 1;
	PitKf = Track->PitK(Idx);
	PitStopT = StopTime;
	bBox = false;
	bPitManual = false;
}

void AGPMCar::StartManualPit()
{
	PitPhase = 1;
	PitKf = Track->PitK(Idx);
	PitO = FMath::Clamp(Lat - Track->PitLatAt(PitKf), -2.5f, 2.5f);
	PitHd = 0.f;
	PitStopT = 0.f;
	bPitManual = true; bPitMissed = bPitWarned = false;
}

bool AGPMCar::InSlot() const
{
	if (!Track || !PitPhase) return false;
	return FMath::Abs(PitKf - FGPMTrackData::PitBox(TeamIdx)) < 1.1f && Lat > Track->SlotLc() - 0.9f;
}

// pneus neufs, ailerons réparés
void AGPMCar::PitService()
{
	Tyre = NextTyre; Wear = 0.f; ++Stops;
	Dmg = DmgR = 0.f; bFlat = false; bBox = false;
	RepairWings();
}

// position sur la spline, décalée vers la voie des stands
void AGPMCar::PlaceInLane(float Hd)
{
	const FGPMTrackData& T = *Track;
	const int32 N = T.WX.Num();
	const float F = FMath::Fmod(FMath::Fmod(PitKf, float(N)) + N, float(N));
	Idx = FMath::FloorToInt(F) % N;
	const float Fr = F - FMath::FloorToFloat(F);
	const int32 i = Idx, j = (i + 1) % N;
	const float Px = T.WX[i] + (T.WX[j] - T.WX[i]) * Fr, Pz = T.WZ[i] + (T.WZ[j] - T.WZ[i]) * Fr;
	X = Px - T.TZ[i] * Lat; Z = Pz + T.TX[i] * Lat; Y = T.WY[i];
	Heading = T.H[i] + Hd; Pitch = Beta = SpinT = Ax = Ay = 0.f; Dive = Roll = YOff = 0.f;
	bOffTrack = false;
}

// le joueur conduit dans la voie : limiteur, s'arrête dans son carré (ou recule, frein à l'arrêt), puis repart
void AGPMCar::PitManual(float Dt)
{
	const FGPMTrackData& T = *Track;
	const float K1 = FGPMTrackData::PitBox(TeamIdx) + 2.f;
	if (PitPhase == 2)
	{
		V = 0.f;
		PitHd *= 0.9f;
		PitStopT -= Dt;
		if (PitStopT <= 0.f) { PitService(); PitPhase = 3; PitMsg = 2; }
	}
	else
	{
		float Vn = V;
		if (Throttle > 0.1f) Vn += (Vn < 0.f ? 12.f : 9.f) * Dt;
		else Vn -= FMath::Sign(Vn) * FMath::Min(FMath::Abs(Vn), 2.f * Dt);
		if (Brake > 0.1f) Vn = Vn > 0.05f ? FMath::Max(0.f, Vn - 28.f * Dt) : FMath::Max(-4.f, Vn - 5.f * Dt);   // à l'arrêt, le frein fait reculer
		if (PitKf < T.PitE1) Vn = FMath::Min(Vn, FMath::Max(PitSpeed, V - 30.f * Dt));   // entrée : freinage jusqu'au limiteur
		else if (PitKf <= T.PitX0) Vn = FMath::Min(Vn, PitSpeed);                         // limiteur
		else Vn = FMath::Min(Vn, 55.f);
		if (PitKf <= T.PitE1 + 1 && Vn < 0.f) Vn = 0.f;
		V = Vn;
		PitKf += Vn * Dt / PitDS;
		// écart au centre de la voie (±2,5 m) : le volant le fait varier
		PitO = FMath::Clamp(PitO + Steer * 4.5f * FMath::Min(1.f, FMath::Abs(Vn) / 5.f) * Dt, -2.5f, 2.5f);
		const float L0 = Lat;
		Lat = T.PitLatAt(PitKf) + PitO;
		const float Lv = PitPhase == 3 && PitKf > T.PitX0 ? 0.f : (Lat - L0) / Dt;
		const float Want = FMath::Clamp(-FMath::Atan(Lv / ((Vn < 0.f ? -1.f : 1.f) * FMath::Max(3.f, FMath::Abs(Vn)))), -0.35f, 0.35f);
		PitHd += (Want - PitHd) * FMath::Min(1.f, Dt * 6.f);
		if (PitPhase == 1 && !bPitMissed && FMath::Abs(Vn) < 0.6f && InSlot()) { V = 0.f; PitPhase = 2; PitStopT = 1e6f; PitMsg = 1; }   // durée fixée par le muret
		if (PitPhase == 1 && !bPitWarned && PitKf > K1 + 3.f) { bPitWarned = true; PitMsg = 3; }
		if (PitPhase == 1 && PitKf > T.PitX0) { PitPhase = 3; bPitMissed = true; bBox = false; PitMsg = 4; }
		if (PitKf >= T.PitX1) { PitPhase = 0; bPitManual = false; Lane = Lat; PlaceInLane(0.f); return; }
	}
	PlaceInLane(PitHd);
}

void AGPMCar::PitStep(float Dt)
{
	if (bPitManual) { PitManual(Dt); return; }
	const FGPMTrackData& T = *Track;
	const float BoxK = FGPMTrackData::PitBox(TeamIdx);
	if (PitPhase == 2)
	{
		V = 0.f;
		PitStopT -= Dt;
		if (PitStopT <= 0.f) { PitService(); PitPhase = 3; }
	}
	else
	{
		if (PitKf < T.PitE1) V = FMath::Max(PitSpeed, V - 30.f * Dt);                       // freinage vers le limiteur
		else if (PitPhase == 3 && PitKf > T.PitX0) V = FMath::Min(55.f, V + 14.f * Dt);   // sortie : on relance
		else if (PitPhase == 1 && BoxK - PitKf < 6.f) V = FMath::Max(3.f, FMath::Min(V, (BoxK - PitKf) * 4.f));   // on se gare dans le carré
		else V = V > PitSpeed ? FMath::Max(PitSpeed, V - 30.f * Dt) : FMath::Min(PitSpeed, V + 10.f * Dt);
		PitKf += V * Dt / PitDS;
		if (PitPhase == 1 && PitKf >= BoxK) { PitKf = BoxK; PitPhase = 2; V = 0.f; }
		if (PitPhase == 3 && PitKf >= T.PitX1) { PitPhase = 0; Lane = Lat; }
	}
	// vers le carré de l'écurie (côté garages), puis retour au centre de la voie
	const float Near = FMath::Max(0.f, 1.f - FMath::Abs(PitKf - BoxK) / 14.f), Sm = Near * Near * (3.f - 2.f * Near);
	Lat += (T.PitLatAt(PitKf) + (T.SlotLc() - T.PitLat) * Sm - Lat) * FMath::Min(1.f, Dt * 4.f);
	PlaceInLane(0.f);
	Throttle = Brake = 0.f;
}

// tests : rester à droite jusqu'à l'entrée des stands, puis se garer dans le carré (en reculant si besoin), puis repartir
void AGPMCar::TestDrive()
{
	if (!Track) return;
	if (!PitPhase)
	{
		Steer = FMath::Clamp((Track->PitHalf + 3.f - Lat) * 0.25f, -1.f, 1.f);
		Throttle = V < 25.f ? 1.f : 0.f; Brake = V > 32.f ? 1.f : 0.f;
		return;
	}
	if (!bPitManual || PitPhase == 3) { Steer = 0.f; Throttle = 1.f; Brake = 0.f; return; }
	if (PitPhase == 2) { Steer = Throttle = Brake = 0.f; return; }
	Steer = FMath::Clamp(((Track->SlotLc() - Track->PitLat) - PitO) * 0.8f, -1.f, 1.f);
	const float D = FGPMTrackData::PitBox(TeamIdx) - PitKf;
	Throttle = D > 7.f ? 1.f : (D > 0.4f && V < 2.f ? 0.5f : 0.f);
	Brake = (D <= 7.f && V > 3.f) || (D <= 0.4f && D > -0.4f) || D < -0.4f ? 1.f : 0.f;   // dépassé : le frein à l'arrêt fait reculer
}
