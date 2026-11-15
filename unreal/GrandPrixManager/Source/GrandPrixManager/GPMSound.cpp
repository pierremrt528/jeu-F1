// Sons de la course (audio.js) : moteur du joueur, voiture la plus proche avec effet Doppler, crissements et mur frotté,
// vent, pluie, rumeur des tribunes près de la ligne et acclamations, chocs, bips des feux et de la radio du muret.
// Tout est fabriqué en temps réel par UGPMEngineSound (aucun fichier audio).
#include "GPMGameMode.h"
#include "GPMCar.h"
#include "GPMEngineSound.h"
#include "GPMTypes.h"

void AGPMGameMode::UiClick()
{
	if (EngineSound && bSetSound) EngineSound->Click();
}

void AGPMGameMode::UpdateAudio(float Dt)
{
	if (!EngineSound || !Car) return;
	// les sons de course se taisent derrière les menus ; plus discrets sous les écrans de résultats
	const bool bRacing = Phase == EPhase::Quali || Phase == EPhase::Lights || Phase == EPhase::Race || Phase == EPhase::Intro || Phase == EPhase::Outro;
	const float RaceK = bRacing ? 1.f : (Phase == EPhase::Done || Phase == EPhase::QualiDone) ? 0.35f : 0.f;
	EngineSound->SetMix(bSetSound ? RaceK : 0.f, bSetSound ? 1.f : 0.f);

	// moteur du joueur ; crissements : glisse, roues bloquées, carrosserie qui frotte le mur
	float Skid = FMath::Clamp((FMath::Abs(Car->Beta) - 0.04f) * 6.f + (Car->bLock ? 0.6f : 0.f), 0.f, 1.f) * FMath::Min(1.f, Car->V / 15.f);
	if (Car->HitWall > 0.3f && Car->V > 8.f) Skid = FMath::Max(Skid, 0.7f);
	EngineSound->SetState(Car->Rpm(), Car->Throttle, Skid, Car->V);

	// voiture la plus proche : plus forte à mesure qu'elle approche, plus aiguë quand elle se rapproche (effet Doppler)
	const AGPMCar* Near = nullptr;
	float Best = 70.f;
	for (AGPMCar* C : Cars)
	{
		if (!C || C == Car || C->bParked) continue;
		const float D = FMath::Sqrt(FMath::Square(C->X - Car->X) + FMath::Square(C->Z - Car->Z));
		if (D < Best) { Best = D; Near = C; }
	}
	float Pitch = 1.f;
	if (Near && Near == SndNear && Dt > 1e-4f)
	{
		const float Closing = (SndNearD - Best) / Dt;   // m/s, positif quand elle se rapproche
		Pitch = FMath::Clamp(343.f / FMath::Max(200.f, 343.f - Closing), 0.85f, 1.2f);
	}
	SndNear = Near;
	SndNearD = Best;
	EngineSound->SetOther(Near ? Near->Rpm() : 4000.f, Near ? FMath::Square(1.f - Best / 70.f) : 0.f, Pitch);

	// foule : rumeur partout, forte près de la ligne (tribunes), sur la grille et pendant le tour d'honneur
	float Crowd = 0.12f;
	if (const FGPMTrackData* T = CurTrack())
	{
		const int32 N = T->WX.Num();
		if (N > 0)
		{
			const float Along = float(FMath::Min(Car->Idx, FMath::Max(0, N - Car->Idx))) * 2.f;   // mètres jusqu'à la ligne
			Crowd = FMath::Max(Crowd, FMath::Clamp(1.f - Along / 320.f, 0.f, 1.f) * 0.9f);
		}
	}
	if (Phase == EPhase::Lights || Phase == EPhase::Intro) Crowd = FMath::Max(Crowd, 0.85f);
	if (Phase == EPhase::Outro) Crowd = 1.f;
	EngineSound->SetAmbience(RainShow, Crowd);

	// chocs : nouveau contact avec le mur (plus fort à haute vitesse), accrochage avec une autre voiture
	if (Car->HitWall > SndHit + 0.2f) EngineSound->Thump(FMath::Clamp(0.25f + Car->V / 90.f, 0.25f, 0.75f));
	SndHit = Car->HitWall;
	if (Car->ContactT > SndContact + 0.2f) EngineSound->Thump(0.45f);
	SndContact = Car->ContactT;

	// feux : un bip par feu, un plus aigu au départ, et la foule se lève
	const int32 LitNow = LightsOn();
	if (Phase == EPhase::Lights && LitNow > SndLights && LitNow > 0) EngineSound->Blip(660.f, 0.18f, 0.18f, true);
	if (SndPhase == EPhase::Lights && Phase == EPhase::Race) { EngineSound->Blip(1320.f, 0.35f, 0.2f, true); EngineSound->Cheer(0.7f); }
	SndLights = LitNow;

	// radio du muret : bip-bip et grésillement à chaque nouveau message
	if (bRacing && RadioT > 0.f && !Radio.IsEmpty() && Radio != SndRadio) EngineSound->RadioBeep();
	SndRadio = RadioT > 0.f ? Radio : FString();

	// acclamations : arrivée (plus fortes sur le podium), prise de la tête, tour d'honneur
	const int32 MyPos = Order.IndexOfByKey(Car) + 1;
	if (Car->bFinished && !bSndFinished) EngineSound->Cheer(MyPos >= 1 && MyPos <= 3 ? 1.f : 0.55f);
	SndLeadT = FMath::Max(0.f, SndLeadT - Dt);
	if (Phase == EPhase::Race && MyPos == 1 && SndPos > 1 && Car->Lap >= 1 && SndLeadT <= 0.f) { EngineSound->Cheer(0.6f); SndLeadT = 20.f; }
	if (Phase == EPhase::Outro && SndPhase != EPhase::Outro) EngineSound->Cheer(1.f);
	bSndFinished = Car->bFinished;
	SndPos = MyPos;
	SndPhase = Phase;
}
