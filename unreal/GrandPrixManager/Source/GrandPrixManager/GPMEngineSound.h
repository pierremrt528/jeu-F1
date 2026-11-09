#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "GPMEngineSound.generated.h"

/**
 * Sons générés en temps réel, sans fichier audio (comme audio.js en Web Audio) : moteur V10 dont le régime suit
 * la vitesse et le rapport, crissement des pneus qui glissent, souffle du vent. Le jeu met à jour les paramètres,
 * le fil audio fabrique les échantillons.
 */
UCLASS(ClassGroup = (GPM), meta = (BlueprintSpawnableComponent))
class GRANDPRIXMANAGER_API UGPMEngineSound : public USynthComponent
{
	GENERATED_BODY()

public:
	UGPMEngineSound(const FObjectInitializer& ObjectInitializer);

	/** Régime (tr/min), charge 0..1 (accélérateur), glisse 0..1, vitesse (m/s). */
	void SetState(float InRpm, float InLoad, float InSkid, float InSpeed)
	{
		Rpm.store(InRpm); Load.store(InLoad); Skid.store(InSkid); Speed.store(InSpeed);
	}

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<float> Rpm{ 4000.f }, Load{ 0.f }, Skid{ 0.f }, Speed{ 0.f };
	// état du fil audio
	float Rate = 48000.f, Phase = 0.f, SubPhase = 0.f, RpmS = 4000.f, LoadS = 0.f, SkidS = 0.f, SpeedS = 0.f;
	float Lp = 0.f, Lp2 = 0.f, SkidBp1 = 0.f, SkidBp2 = 0.f, WindLp = 0.f;
	uint32 Seed = 12345u;
	float Rand() { Seed = Seed * 1664525u + 1013904223u; return (Seed >> 8) * (2.f / 16777216.f) - 1.f; }
};
