// Pilotes IA : portage de js/ai.js, enrichi. Suivent la trajectoire (poursuite pure), freinent d'après l'adhérence du
// moment (pneus, pluie, essence, air sale), se décalent pour dépasser (de préférence à la corde du virage suivant),
// défendent la corde, s'écartent pour les voitures qui leur prennent un tour, et font parfois des erreurs.
#include "GPMCar.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float SplineDS = 2.f;   // pas de la spline (m)
	float Sgn(float X) { return X > 0.f ? 1.f : X < 0.f ? -1.f : 0.f; }
}

void AGPMCar::MakeAI(float InSkill)
{
	static bool bOptLoaded = false;
	if (!bOptLoaded) { LoadAIOptions(); bOptLoaded = true; }
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
	auto Wr = [N](int32 i) { return ((i % N) + N) % N; };
	const bool bProf = T.AISpeed.Num() == N;
	auto Prof = [&T, bProf, &Wr](int32 i) { return bProf ? T.AISpeed[Wr(i)] : 40.f; };
	const float Aggro = float(AIAggro);   // 0 prudent, 1 normal, 2 agressif
	const float Lim = T.Width / 2.f - 1.4f;

	// prochain virage (dans les 160 m) : distance et côté de la corde (la trajectoire idéale passe à la corde)
	int32 ApexK = -1;
	float ApexV = 1e9f;
	for (int32 k = 4; k <= 80; k += 2)
		if (Prof(Idx + k) < ApexV) { ApexV = Prof(Idx + k); ApexK = k; }
	const bool bCorner = ApexK > 0 && ApexV < Prof(Idx) * 0.82f;
	const float ApexDist = bCorner ? ApexK * SplineDS : 1e9f;
	const float Inside = bCorner && FMath::Abs(T.RL[Wr(Idx + ApexK)]) > 1.f ? Sgn(T.RL[Wr(Idx + ApexK)]) : 0.f;

	// voitures autour : devant sur la même trajectoire, juste derrière, à côté (pour ne pas se rabattre sur elles),
	// et celle qui nous prend un tour
	AGPMCar* Ahead = nullptr;
	AGPMCar* Behind = nullptr;
	AGPMCar* Beside = nullptr;
	AGPMCar* Lapper = nullptr;
	float Gap = 1e9f, GapB = 1e9f;
	float SideL = -1e9f, SideR = 1e9f;   // limites de trajectoire imposées par une voiture à gauche / à droite (m)
	for (AGPMCar* O : Field)
	{
		if (!O || O == this || O->PitPhase) continue;
		int32 Di = (O->Idx - Idx + N) % N;
		if (Di > N / 2) Di -= N;
		const float D = Di * SplineDS, DLat = O->Lat - Lat;   // D > 0 : devant
		if (D > 0 && D < 45 && FMath::Abs(DLat) < (bSCMode ? 99.f : 2.6f) && D < Gap) { Gap = D; Ahead = O; }
		if (D < 0 && D > -30 && -D < GapB) { GapB = -D; Behind = O; }
		if (D < 0 && D > -50 && O->Progress() - Progress() > N / 2) Lapper = O;
		// roues dans les roues : on garde 2,7 m d'écart latéral
		if (!bSCMode && FMath::Abs(D) < 6.5f && FMath::Abs(DLat) < 4.2f)
		{
			if (DLat < 0.f || (DLat == 0.f && D > 0.f)) SideL = FMath::Max(SideL, O->Lat + 2.7f);
			else SideR = FMath::Min(SideR, O->Lat - 2.7f);
			if (FMath::Abs(D) < 5.5f) Beside = O;
		}
	}
	PassTimer -= Dt;
	DefendCd = FMath::Max(0.f, DefendCd - Dt);
	DefendT = FMath::Max(0.f, DefendT - Dt);
	MistakeT = FMath::Max(0.f, MistakeT - Dt);
	float LaneTarget = T.RL[Idx] * 0.85f + LaneBias;
	float VCap = 1e9f, DecK = 1.f, VMul = 1.f;
	bool bPassing = false;
	if (bSCMode)
	{
		// voiture de sécurité : file indienne, pas de dépassement
		if (Ahead) VCap = Ahead->V + (Gap - 16) * 0.6f;
	}
	else if (Ahead && (Ahead->V < V + 2.f + 1.2f * Aggro || Gap < 14))
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
			// les deux côtés libres : la corde du virage qui arrive, sinon le côté le plus proche du milieu
			const float Pref = Inside != 0.f && ApexDist < 160.f ? Inside : (FMath::Abs(L) < FMath::Abs(R) ? -1.f : 1.f);
			PassSide = bLeftOk && bRightOk ? Pref : bLeftOk ? -1.f : bRightOk ? 1.f : 0.f;
			PassTimer = PassSide == 0.f ? 0.7f : 2.5f;
		}
		if (PassSide != 0.f) { LaneTarget = Ahead->Lat + PassSide * 3.4f; bPassing = true; }
		// distance de sécurité : on suit sans percuter tant qu'on n'est pas décalé (plus d'écart à haute vitesse)
		const float Keep = 5.f + FMath::Max(V, 0.f) * (0.08f - 0.01f * Aggro);
		if (Gap < Keep + 12.f) VCap = Ahead->V + (Gap - Keep) * 0.5f;
	}

	if (!bSCMode)
	{
		// défense : une voiture colle derrière avant un freinage, on se place une fois à la corde (pas de zigzag)
		if (!bPassing && !Beside && Behind && GapB < 20.f && Behind->V > V - 1.f && Inside != 0.f && ApexDist < 140.f && ApexDist > 30.f && DefendCd <= 0.f)
		{
			DefendCd = 3.f;
			if (FMath::FRand() < 0.35f + 0.25f * Aggro)
			{
				DefendLane = Inside * FMath::Min(Lim, T.Width * 0.3f);
				DefendT = ApexDist / FMath::Max(V, 10.f) + 0.6f;
				DefendCd = DefendT + 8.f;
			}
		}
		if (DefendT > 0.f && !bPassing) LaneTarget = DefendLane;

		// côte à côte à l'entrée du virage : à la corde on freine un peu plus tard, à l'extérieur on cède la place
		if (Beside && Inside != 0.f && ApexDist < 110.f)
		{
			int32 Db = (Beside->Idx - Idx + N) % N;
			if (Db > N / 2) Db -= N;
			if (Sgn(Lat - Beside->Lat) == Inside) DecK *= 1.03f + 0.02f * Aggro;
			else if (Db * SplineDS > -2.5f) VMul *= 0.965f + 0.012f * Aggro;
		}

		// drapeau bleu : on laisse passer la voiture qui nous prend un tour
		bBlue = Lapper != nullptr && Lapper != YieldTo;
		if (bBlue)
		{
			LaneTarget = (Lapper->Lat >= Lat ? -1.f : 1.f) * Lim * 0.85f;
			VMul *= 0.97f;
		}

		// erreurs de pilotage : freinage trop tardif, la voiture élargit la trajectoire (plus souvent pour les pilotes
		// moins forts, sur pneus usés ou piste mouillée) ; sur le mouillé, l'arrière peut aussi décrocher
		if (bAIErrors && MistakeT <= 0.f && bCorner && ApexDist < 90.f && V > 35.f && Lap >= 1)
		{
			const float Rate = 0.002f * (1.f + 25.f * FMath::Max(0.f, 0.985f - Skill)) * (1.f + 3.f * FMath::Max(0.f, 0.95f - GripMul)) * (1.f + 0.4f * Aggro);
			if (FMath::FRand() < Rate * Dt)
			{
				MistakeT = 1.6f;
				MistakeSide = Inside != 0.f ? -Inside : (FMath::FRand() < 0.5f ? -1.f : 1.f);
				if (GripMul < 0.8f && FMath::FRand() < 0.25f) Beta += MistakeSide * 0.3f;
			}
		}
		bMistake = MistakeT > 0.f;
		if (bMistake)
		{
			if (MistakeT > 0.9f) DecK *= 1.12f;
			LaneTarget = MistakeSide * (Lim + 1.f);
		}
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
	const float LimNow = Lim + (bMistake ? 1.f : 0.f);
	LaneTarget = FMath::Clamp(LaneTarget, -LimNow, LimNow);
	Lane += (LaneTarget - Lane) * FMath::Min(1.f, Dt * 1.5f);

	Steer = Pursuit(Lane);

	// vitesse cible : profil de référence × niveau, adhérence du moment (gomme, usure, piste mouillée, poids de
	// l'essence), appui perdu (aileron abîmé, air sale), et petites variations de rythme selon la régularité du pilote
	const float GripNow = FMath::Max(0.2f, GripMul / (1.f + (Mass - 1.f) * 0.25f) * (0.4f + 0.6f * DmgKd));
	const float Noise = 1.f + 0.06f * FMath::Max(0.f, 1.02f - Skill) * FMath::PerlinNoise1D(RaceTime * 0.07f + LaneBias * 37.f);
	const float Sc = Skill * CornerK * Noise * (bSCMode ? (Ahead ? 0.72f : 0.9f) : 1.f) * FMath::Sqrt(GripNow) * (1.f - 0.04f * Wake) * VMul * SlowMul;
	float Vt = Prof(Idx) * Sc;
	// freinage anticipé : on regarde assez loin pour s'arrêter avec les freins et l'adhérence qu'on a vraiment
	const float GripDec = FMath::Min(1.f, GripNow) * DecK;
	const int32 KMax = FMath::Clamp(FMath::CeilToInt(V * V / (2.f * 12.f * FMath::Max(0.3f, GripDec)) / SplineDS) + 4, 4, 150);
	for (int32 k = 1; k <= KMax; ++k)
	{
		const float Vk = Prof(Idx + k) * Sc;
		const float Dec = 0.85f * (Brake0 + BrakeK * Vk * Vk) * GripDec;
		Vt = FMath::Min(Vt, FMath::Sqrt(Vk * Vk + 2.f * Dec * FMath::Max(0.f, k * SplineDS - 3.f)));
	}
	Vt = FMath::Min(Vt, VCap);
	if (bYielding) Vt *= 0.9f;
	if (bOffTrack) Vt = FMath::Min(Vt, 25.f);

	// batterie : déployée pour attaquer dans l'aspiration de la voiture de devant
	bBoost = Ers > 0.25f && Slipstream > 0.35f && !bYielding && !bSCMode;
	if (bBoost) Vt *= 1.02f;
	// pédales dosées (plus de coups de gaz tout-ou-rien) : un peu d'avance au gaz, frein proportionnel à l'écart
	const float Err = Vt - V;
	if (Err > -0.6f) { Throttle = FMath::Clamp(0.6f + Err * 0.6f, 0.f, 1.f); Brake = 0.f; }
	else { Throttle = 0.f; Brake = FMath::Clamp((-Err - 0.6f) * 0.45f, 0.f, 1.f); }

	// départ : temps de réaction de chaque pilote à l'extinction des feux
	if (bHold) { bHoldPrev = true; LaunchT = 0.f; }
	else if (bHoldPrev)
	{
		bHoldPrev = false;
		LaunchT = 0.06f + FMath::FRand() * (0.12f + 3.f * FMath::Max(0.f, 1.f - Skill));
	}
	if (LaunchT > 0.f) { LaunchT -= Dt; Throttle = 0.f; Brake = 0.f; }
}
