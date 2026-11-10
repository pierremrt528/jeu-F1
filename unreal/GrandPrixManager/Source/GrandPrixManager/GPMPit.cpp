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
}

void AGPMCar::PitStep(float Dt)
{
	const FGPMTrackData& T = *Track;
	const int32 N = T.WX.Num();
	const float BoxK = FGPMTrackData::PitBox(TeamIdx);
	if (PitPhase == 2)
	{
		V = 0.f;
		PitStopT -= Dt;
		if (PitStopT <= 0.f)
		{
			// pneus neufs, ailerons réparés
			Tyre = NextTyre; Wear = 0.f; ++Stops; PitPhase = 3;
			Dmg = DmgR = 0.f; bFlat = false;
			RepairWings();
		}
	}
	else
	{
		if (PitKf < T.PitE1) V = FMath::Max(PitSpeed, V - 30.f * Dt);                       // freinage vers le limiteur
		else if (PitPhase == 3 && PitKf > T.PitX0) V = FMath::Min(55.f, V + 14.f * Dt);   // sortie : on relance
		else V = V > PitSpeed ? FMath::Max(PitSpeed, V - 30.f * Dt) : FMath::Min(PitSpeed, V + 10.f * Dt);
		PitKf += V * Dt / PitDS;
		if (PitPhase == 1 && PitKf >= BoxK) { PitKf = BoxK; PitPhase = 2; V = 0.f; }
		if (PitPhase == 3 && PitKf >= T.PitX1) { PitPhase = 0; Lane = Lat; }
	}
	// position : sur la spline, décalée vers la voie des stands
	const float F = FMath::Fmod(FMath::Fmod(PitKf, float(N)) + N, float(N));
	Idx = FMath::FloorToInt(F) % N;
	const float Fr = F - FMath::FloorToFloat(F);
	Lat += (T.PitLatAt(PitKf) - Lat) * FMath::Min(1.f, Dt * 4.f);
	const int32 i = Idx, j = (i + 1) % N;
	const float Px = T.WX[i] + (T.WX[j] - T.WX[i]) * Fr, Pz = T.WZ[i] + (T.WZ[j] - T.WZ[i]) * Fr;
	X = Px - T.TZ[i] * Lat; Z = Pz + T.TX[i] * Lat; Y = T.WY[i];
	Heading = T.H[i]; Pitch = Beta = SpinT = Ax = Ay = 0.f; Dive = Roll = YOff = 0.f;
	Throttle = Brake = 0.f; bOffTrack = false;
}
