#include "GPMEngineSound.h"

UGPMEngineSound::UGPMEngineSound(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = false;
}

bool UGPMEngineSound::Init(int32& SampleRate)
{
	NumChannels = 1;
	Rate = float(SampleRate);
	return true;
}

int32 UGPMEngineSound::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float TRpm = Rpm.load(), TLoad = Load.load(), TSkid = Skid.load(), TSpeed = Speed.load();
	const float K = 1.f / Rate;
	for (int32 n = 0; n < NumSamples; ++n)
	{
		// paramètres lissés (pas de clics quand ils changent)
		RpmS += (TRpm - RpmS) * 0.0015f;
		LoadS += (TLoad - LoadS) * 0.002f;
		SkidS += (TSkid - SkidS) * 0.003f;
		SpeedS += (TSpeed - SpeedS) * 0.001f;

		// moteur : V10 = 5 explosions par tour ; dent de scie + harmonique + grondement sous-harmonique
		const float F = RpmS / 60.f * 5.f;
		Phase += F * K; if (Phase >= 1.f) Phase -= 1.f;
		SubPhase += F * 0.5f * K; if (SubPhase >= 1.f) SubPhase -= 1.f;
		const float Saw = 2.f * Phase - 1.f;
		const float Saw2 = 2.f * FMath::Frac(Phase * 2.f) - 1.f;
		const float Sub = FMath::Sin(SubPhase * 2.f * PI);
		float X = 0.55f * Saw + 0.25f * Saw2 + 0.35f * Sub + 0.05f * Rand();
		// filtre passe-bas : plus ouvert en pleine charge (son plus rageur)
		const float A = FMath::Clamp((900.f + 5200.f * LoadS + RpmS * 0.25f) * 2.f * PI * K, 0.f, 1.f);
		Lp += A * (X - Lp); Lp2 += A * (Lp - Lp2);
		float Out = Lp2 * (0.22f + 0.2f * LoadS);

		// crissement : bruit filtré autour de 1,2 kHz
		if (SkidS > 0.01f)
		{
			const float Nz = Rand();
			SkidBp1 += 0.25f * (Nz - SkidBp1);
			SkidBp2 += 0.08f * (SkidBp1 - SkidBp2);
			Out += (SkidBp1 - SkidBp2) * SkidS * 0.9f;
		}
		// vent : bruit grave qui monte avec le carré de la vitesse
		WindLp += 0.02f * (Rand() - WindLp);
		Out += WindLp * FMath::Square(FMath::Min(1.f, SpeedS / 90.f)) * 0.5f;

		OutAudio[n] = FMath::Clamp(Out, -1.f, 1.f);
	}
	return NumSamples;
}
