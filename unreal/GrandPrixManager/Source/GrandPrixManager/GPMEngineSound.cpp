#include "GPMEngineSound.h"

namespace GPMSoundDsp
{
	/** Saturation douce (approximation de tanh) : les sons qui s'additionnent ne craquent pas. */
	float SoftClip(float X)
	{
		const float C = FMath::Clamp(X, -3.f, 3.f);
		return C * (27.f + C * C) / (27.f + 9.f * C * C);
	}
	/** Coefficient de décroissance par échantillon pour tomber à -60 dB en Secs secondes. */
	float DecayFor(float Secs, float SampleRate) { return FMath::Exp(FMath::Loge(0.001f) / FMath::Max(1.f, Secs * SampleRate)); }
}

UGPMEngineSound::UGPMEngineSound(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = false;
}

bool UGPMEngineSound::Init(int32& SampleRate)
{
	NumChannels = 1;
	Rate = float(SampleRate);
	ThumpDecay = GPMSoundDsp::DecayFor(0.35f, Rate);
	StaticDecay = GPMSoundDsp::DecayFor(0.45f, Rate);
	CheerDecay = GPMSoundDsp::DecayFor(4.5f, Rate);
	return true;
}

// ---------- sons ponctuels : exécutés par le fil audio avant le prochain bloc d'échantillons ----------

void UGPMEngineSound::Thump(float Vol)
{
	SynthCommand([this, Vol]() { ThumpAmp = FMath::Max(ThumpAmp, Vol); });
}

void UGPMEngineSound::Blip(float Freq, float Dur, float Vol, bool bSquareWave, float Delay, bool bMenu)
{
	SynthCommand([this, Freq, Dur, Vol, bSquareWave, Delay, bMenu]() { StartVoice(Freq, Dur, Vol, bSquareWave, Delay, bMenu); });
}

void UGPMEngineSound::RadioBeep()
{
	// bip-bip du muret (audio.js) puis grésillement de la radio
	Blip(1200.f, 0.07f, 0.12f);
	Blip(900.f, 0.09f, 0.12f, false, 0.09f);
	SynthCommand([this]() { StaticAmp = 0.05f; });
}

void UGPMEngineSound::Cheer(float Vol)
{
	SynthCommand([this, Vol]() { CheerAmp = FMath::Max(CheerAmp, Vol); });
}

void UGPMEngineSound::Click() { Blip(1650.f, 0.035f, 0.05f, false, 0.f, true); }

void UGPMEngineSound::StartVoice(float Freq, float Dur, float Vol, bool bSquareWave, float Delay, bool bMenu)
{
	FGPMBlipVoice* Slot = &Voices[0];
	for (FGPMBlipVoice& Vc : Voices)
		if (Vc.Amp <= 0.0005f && Vc.Wait <= 0.f) { Slot = &Vc; break; }
	Slot->Freq = Freq;
	Slot->Ph = 0.f;
	Slot->Amp = Vol;
	Slot->Decay = GPMSoundDsp::DecayFor(Dur, Rate);
	Slot->Wait = Delay * Rate;
	Slot->bSquare = bSquareWave;
	Slot->bUi = bMenu;
}

int32 UGPMEngineSound::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float TRpm = Rpm.load(), TLoad = Load.load(), TSkid = Skid.load(), TSpeed = Speed.load();
	const float TORpm = OtherRpm.load(), TOVol = OtherVol.load(), TOPitch = OtherPitch.load();
	const float TRain = RainAmt.load(), TCrowd = CrowdAmt.load(), TRace = RaceAmt.load(), TUi = UiAmt.load();
	const float K = 1.f / Rate;
	for (int32 n = 0; n < NumSamples; ++n)
	{
		// paramètres lissés (pas de clics quand ils changent)
		RpmS += (TRpm - RpmS) * 0.0015f;
		LoadS += (TLoad - LoadS) * 0.002f;
		SkidS += (TSkid - SkidS) * 0.003f;
		SpeedS += (TSpeed - SpeedS) * 0.001f;
		ORpmS += (TORpm - ORpmS) * 0.002f;
		OVolS += (TOVol - OVolS) * 0.0008f;
		OPitchS += (TOPitch - OPitchS) * 0.0005f;
		RainS += (TRain - RainS) * 0.00005f;
		CrowdS += (TCrowd - CrowdS) * 0.00008f;
		RaceS += (TRace - RaceS) * 0.0004f;
		UiS += (TUi - UiS) * 0.002f;

		// moteur : V10 = 5 explosions par tour ; dent de scie + harmonique + grondement sous-harmonique
		const float F = RpmS / 60.f * 5.f;
		Phase += F * K; if (Phase >= 1.f) Phase -= 1.f;
		SubPhase += F * 0.5f * K; if (SubPhase >= 1.f) SubPhase -= 1.f;
		const float Saw = 2.f * Phase - 1.f;
		const float Saw2 = 2.f * FMath::Frac(Phase * 2.f) - 1.f;
		const float Sub = FMath::Sin(SubPhase * 2.f * PI);
		const float X = 0.55f * Saw + 0.25f * Saw2 + 0.35f * Sub + 0.05f * Rand();
		// filtre passe-bas : plus ouvert en pleine charge (son plus rageur)
		const float A = FMath::Clamp((900.f + 5200.f * LoadS + RpmS * 0.25f) * 2.f * PI * K, 0.f, 1.f);
		Lp += A * (X - Lp); Lp2 += A * (Lp - Lp2);
		float Race = Lp2 * (0.22f + 0.2f * LoadS);

		// voiture la plus proche : même moteur, plus sourd, hauteur décalée par l'effet Doppler
		if (OVolS > 0.001f)
		{
			const float OF = ORpmS / 60.f * 5.f * OPitchS;
			OPhase += OF * K; if (OPhase >= 1.f) OPhase -= 1.f;
			OSub += OF * 0.5f * K; if (OSub >= 1.f) OSub -= 1.f;
			const float OX = 0.6f * (2.f * OPhase - 1.f) + 0.2f * (2.f * FMath::Frac(OPhase * 2.f) - 1.f) + 0.3f * FMath::Sin(OSub * 2.f * PI);
			const float OA = FMath::Clamp((700.f + 2600.f * OVolS + ORpmS * 0.15f) * 2.f * PI * K, 0.f, 1.f);
			OLp += OA * (OX - OLp); OLp2 += OA * (OLp - OLp2);
			Race += OLp2 * OVolS * 0.3f;
		}

		// crissement : bruit filtré autour de 1,2 kHz
		if (SkidS > 0.01f)
		{
			const float Nz = Rand();
			SkidBp1 += 0.25f * (Nz - SkidBp1);
			SkidBp2 += 0.08f * (SkidBp1 - SkidBp2);
			Race += (SkidBp1 - SkidBp2) * SkidS * 0.9f;
		}
		// vent : bruit grave qui monte avec le carré de la vitesse
		WindLp += 0.02f * (Rand() - WindLp);
		Race += WindLp * FMath::Square(FMath::Min(1.f, SpeedS / 90.f)) * 0.5f;

		// pluie : souffle aigu et gouttes qui claquent sur la carrosserie
		if (RainS > 0.001f)
		{
			const float Nz = Rand();
			RainLp += 0.25f * (Nz - RainLp);
			if (Rand() > 0.9985f) DropAmp = 0.4f + 0.6f * FMath::Abs(Rand());
			DropLp += 0.35f * (Rand() * DropAmp - DropLp);
			DropAmp *= 0.996f;
			Race += ((Nz - RainLp) * 0.45f + DropLp) * RainS * 0.12f;
		}

		// foule : rumeur dans le médium qui ondule lentement, et vagues d'acclamations plus claires
		if (CrowdS > 0.001f || CheerAmp > 0.001f || CheerEnv > 0.001f)
		{
			const float Nz = Rand();
			CrowdLp1 += 0.09f * (Nz - CrowdLp1);
			CrowdLp2 += 0.012f * (CrowdLp1 - CrowdLp2);
			if (--CrowdHold <= 0)
			{
				CrowdHold = int32(Rate * (0.3f + 0.6f * FMath::Abs(Rand())));
				CrowdModT = 0.55f + 0.45f * FMath::Abs(Rand());
			}
			CrowdMod += (CrowdModT - CrowdMod) * 0.00015f;
			Race += (CrowdLp1 - CrowdLp2) * CrowdMod * CrowdS * 0.55f;
			CheerEnv += (CheerAmp - CheerEnv) * 0.0002f;
			CheerAmp *= CheerDecay;
			CheerLp1 += 0.35f * (Nz - CheerLp1);
			CheerLp2 += 0.06f * (CheerLp1 - CheerLp2);
			Race += (CheerLp1 - CheerLp2) * CheerEnv * (0.75f + 0.25f * CrowdMod) * 0.5f;
		}

		// choc : bruit sourd et grondement grave qui s'éteignent en un tiers de seconde
		if (ThumpAmp > 0.0005f)
		{
			ThumpLp += 0.04f * (Rand() - ThumpLp);
			ThumpPh += 62.f * K; if (ThumpPh >= 1.f) ThumpPh -= 1.f;
			Race += (ThumpLp * 2.4f + 0.55f * FMath::Sin(ThumpPh * 2.f * PI)) * ThumpAmp;
			ThumpAmp *= ThumpDecay;
		}

		// grésillement de la radio : bruit aigu haché
		if (StaticAmp > 0.0005f)
		{
			const float Nz = Rand();
			StaticLp += 0.5f * (Nz - StaticLp);
			if (--StaticHold <= 0) { StaticHold = int32(Rate * (0.004f + 0.012f * FMath::Abs(Rand()))); StaticGate = Rand() > -0.4f ? 1.f : 0.2f; }
			Race += (Nz - StaticLp) * StaticGate * StaticAmp;
			StaticAmp *= StaticDecay;
		}

		// bips : feux, radio (sons de course) et clics des menus
		float Menu = 0.f;
		for (FGPMBlipVoice& Vc : Voices)
		{
			if (Vc.Wait > 0.f) { Vc.Wait -= 1.f; continue; }
			if (Vc.Amp <= 0.0005f) continue;
			Vc.Ph += Vc.Freq * K; if (Vc.Ph >= 1.f) Vc.Ph -= 1.f;
			const float S = Vc.bSquare ? (Vc.Ph < 0.5f ? 0.6f : -0.6f) : FMath::Sin(Vc.Ph * 2.f * PI);
			if (Vc.bUi) Menu += S * Vc.Amp; else Race += S * Vc.Amp;
			Vc.Amp *= Vc.Decay;
		}

		OutAudio[n] = GPMSoundDsp::SoftClip(Race * RaceS + Menu * UiS);
	}
	return NumSamples;
}
