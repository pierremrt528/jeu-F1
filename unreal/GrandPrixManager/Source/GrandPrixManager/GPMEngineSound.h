#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "GPMEngineSound.generated.h"

/** Un bip (feux, radio, clic des menus) joué par le fil audio. */
struct FGPMBlipVoice
{
	float Freq = 0.f, Ph = 0.f, Amp = 0.f, Decay = 0.f, Wait = 0.f;
	bool bSquare = false, bUi = false;
};

/**
 * Sons générés en temps réel, sans fichier audio (comme audio.js en Web Audio) : moteur V10 du joueur dont le régime suit
 * la vitesse et le rapport, voiture la plus proche (effet Doppler), crissement des pneus, vent, pluie, rumeur et acclamations
 * de la foule, chocs, bips des feux et de la radio, clics des menus. Le jeu met à jour les paramètres ; les sons ponctuels
 * passent par la file de commandes du composant ; le fil audio fabrique les échantillons.
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
	/** Voiture la plus proche : régime, volume 0..1, hauteur (effet Doppler, 1 = immobile l'une par rapport à l'autre). */
	void SetOther(float InRpm, float InVol, float InPitch) { OtherRpm.store(InRpm); OtherVol.store(InVol); OtherPitch.store(InPitch); }
	/** Ambiance : pluie 0..1, rumeur de la foule 0..1. */
	void SetAmbience(float InRain, float InCrowd) { RainAmt.store(InRain); CrowdAmt.store(InCrowd); }
	/** Volumes : sons de course (0 dans les menus) et clics des menus (0 si le son est coupé). */
	void SetMix(float InRace, float InUi) { RaceAmt.store(InRace); UiAmt.store(InUi); }

	/** Sons ponctuels (appelés par le jeu). */
	void Thump(float Vol);
	void Blip(float Freq, float Dur, float Vol, bool bSquareWave = false, float Delay = 0.f, bool bMenu = false);
	void RadioBeep();
	void Cheer(float Vol);
	void Click();

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<float> Rpm{ 4000.f }, Load{ 0.f }, Skid{ 0.f }, Speed{ 0.f };
	std::atomic<float> OtherRpm{ 4000.f }, OtherVol{ 0.f }, OtherPitch{ 1.f }, RainAmt{ 0.f }, CrowdAmt{ 0.f }, RaceAmt{ 0.f }, UiAmt{ 1.f };
	// état du fil audio
	float Rate = 48000.f, Phase = 0.f, SubPhase = 0.f, RpmS = 4000.f, LoadS = 0.f, SkidS = 0.f, SpeedS = 0.f;
	float Lp = 0.f, Lp2 = 0.f, SkidBp1 = 0.f, SkidBp2 = 0.f, WindLp = 0.f;
	float OPhase = 0.f, OSub = 0.f, ORpmS = 4000.f, OVolS = 0.f, OPitchS = 1.f, OLp = 0.f, OLp2 = 0.f;
	float RainS = 0.f, RainLp = 0.f, DropAmp = 0.f, DropLp = 0.f;
	float CrowdS = 0.f, CrowdLp1 = 0.f, CrowdLp2 = 0.f, CrowdMod = 0.7f, CrowdModT = 0.7f;
	int32 CrowdHold = 0;
	float CheerAmp = 0.f, CheerEnv = 0.f, CheerLp1 = 0.f, CheerLp2 = 0.f, CheerDecay = 0.9999f;
	float ThumpAmp = 0.f, ThumpLp = 0.f, ThumpPh = 0.f, ThumpDecay = 0.9997f;
	float StaticAmp = 0.f, StaticLp = 0.f, StaticGate = 1.f, StaticDecay = 0.9998f;
	int32 StaticHold = 0;
	float RaceS = 0.f, UiS = 1.f;
	FGPMBlipVoice Voices[8];
	uint32 Seed = 12345u;
	float Rand() { Seed = Seed * 1664525u + 1013904223u; return (Seed >> 8) * (2.f / 16777216.f) - 1.f; }
	void StartVoice(float Freq, float Dur, float Vol, bool bSquareWave, float Delay, bool bMenu);
};
