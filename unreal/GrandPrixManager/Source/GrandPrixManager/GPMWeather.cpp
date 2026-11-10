// Course complète comme le jeu web (strategy.js, qsession.js, debris.js) : météo du week-end et piste qui mouille,
// ciel couvert, dégâts d'aileron, décisions d'arrêt (pluie, crevaison, casse), radio du muret, secteurs des qualifications.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMTrack.h"
#include "GPMDebris.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "GPMSky.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// ---------- météo ----------

// tirage du week-end (forecast) : sec, pluie toute la course, averse en cours de course, ou éclaircie ; ?meteo=pluie|averse|eclaircie|sec
void AGPMGameMode::Forecast()
{
	const FGPMTrackData* T = CurTrack();
	bRainStart = false;
	WxChangeAt = -1.f;
	// météo imposée : option de test, ou choix de la course solo (1 sec, 2 pluie, 3 averse, 4 éclaircie)
	const FString Opt = UGameplayStatics::ParseOption(OptionsString, TEXT("meteo"));
	const int32 Forced = !Opt.IsEmpty() ? (Opt == TEXT("pluie") ? 2 : Opt == TEXT("averse") ? 3 : Opt == TEXT("eclaircie") ? 4 : 1) : !bCareer ? SoloWeather : 0;
	if (Forced == 2) bRainStart = true;
	else if (Forced == 3) WxChangeAt = 0.3f + 0.2f * FMath::FRand();
	else if (Forced == 4) { bRainStart = true; WxChangeAt = 0.4f; }
	else if (Forced == 0 && T && T->Theme.Surface != TEXT("sand"))   // désert : toujours sec
	{
		const float DryP = FMath::Clamp(T->Theme.Dry, 0.f, 1.f), Wk = (1.f - DryP) / 0.45f, Roll = FMath::FRand();
		if (Roll >= DryP)
		{
			if (Roll < DryP + 0.15f * Wk) bRainStart = true;
			else if (Roll < DryP + 0.33f * Wk) WxChangeAt = 0.25f + 0.4f * FMath::FRand();
			else { bRainStart = true; WxChangeAt = 0.3f + 0.35f * FMath::FRand(); }
		}
	}
	ResetWeather();
	UE_LOG(LogGPM, Log, TEXT("Météo du week-end : %s"), *WeatherLabel());
}

void AGPMGameMode::ResetWeather()
{
	bRaining = bRainStart;
	TrackWet = bRainStart ? 0.9f : 0.f;
	bWxDone = bWxWarned = false;
}

FString AGPMGameMode::WeatherLabel() const
{
	if (WxChangeAt >= 0.f && !bWxDone) return bRainStart ? TEXT("Pluie, éclaircie attendue") : TEXT("Sec, averse attendue");
	if (bRaining) return TEXT("Pluie");
	return TrackWet > 0.25f ? TEXT("Piste humide") : TEXT("Sec");
}

// humidité moyenne de la course, pour les courses simulées
float AGPMGameMode::RaceWetK() const
{
	float Avg = bRainStart ? 0.9f : 0.f;
	if (WxChangeAt >= 0.f)
		Avg = bRainStart ? 0.9f * WxChangeAt + 0.3f * (1.f - WxChangeAt) : 0.8f * (1.f - WxChangeAt);
	return 1.f + 0.1f * Avg;
}

// pendant la course : changement de temps quand le leader atteint la part prévue, piste qui mouille (35 s) ou sèche (90 s)
void AGPMGameMode::UpdateWeather(float Dt)
{
	if (WxChangeAt >= 0.f && !bWxDone && Order.Num() && TotalLaps > 0)
	{
		const AGPMCar* Lead = Order[0];
		const FGPMTrackData* T = Lead->GetTrack();
		const float Frac = T ? (Lead->Progress() / float(FMath::Max(1, T->WX.Num())) - 1.f) / TotalLaps : 0.f;
		if (!bWxWarned && Frac >= WxChangeAt - 0.08f)
		{
			bWxWarned = true;
			if (!bRainStart) Say(TEXT("Muret : averse prévue dans quelques minutes."));
		}
		if (Frac >= WxChangeAt)
		{
			bWxDone = true;
			bRaining = !bRainStart;
			Say(bRaining ? TEXT("Muret : la pluie arrive ! Surveille la piste.") : TEXT("Muret : la pluie s'arrête, la piste va sécher."));
		}
	}
	TrackWet = FMath::Clamp(TrackWet + (bRaining ? Dt / 35.f : -Dt / 90.f), 0.f, 1.f);
}

// ciel couvert sous la pluie : soleil voilé, ciel plus blanc, brouillard gris ; asphalte sombre et brillant (matériau)
void AGPMGameMode::UpdateSky(float Dt)
{
	RainShow = FMath::FInterpConstantTo(RainShow, bRaining ? 1.f : 0.f, Dt, 0.25f);
	const float Want = FMath::Max(bRaining ? 1.f : 0.f, TrackWet * 0.45f);
	SkyGloom = FMath::FInterpConstantTo(SkyGloom, Want, Dt, 0.12f);
	if (Track && FMath::Abs(TrackWet - WetSet) > 0.01f) { WetSet = TrackWet; Track->SetWet(TrackWet); }
	if (FMath::Abs(SkyGloom - SkyGloomSet) < 0.01f) return;
	SkyGloomSet = SkyGloom;
	const float G = SkyGloom;
	if (Sun)
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) SunLight->SetIntensity(8.f * (1.f - 0.72f * G));
	if (SkyDome) SkyDome->SetGloom(G);
	if (Fog)
	{
		UExponentialHeightFogComponent* FogC = Fog->GetComponent();
		FogC->SetFogDensity(FogBase * (1.f + 2.2f * G));
		FogC->SetFogInscatteringColor(FogTint + (AGPMSky::GloomGrey() - FogTint) * (0.85f * G));   // même gris que l'horizon du ciel couvert
	}
	// lumière d'ambiance : nouvelle capture du ciel par paliers (coûteuse, donc rare)
	const bool bEnd = (G <= 0.001f || G >= 0.999f) && !FMath::IsNearlyEqual(G, SkyCaptured, 0.001f);
	if (SkyLight && (FMath::Abs(G - SkyCaptured) > 0.2f || bEnd))
	{
		SkyCaptured = G;
		SkyLight->GetLightComponent()->RecaptureSky();
	}
}

// ---------- dégâts ----------

// addDamage : éclats de carbone, aileron tordu à 30 %, arraché sur un gros choc (il part rebondir sur la piste)
void AGPMGameMode::AddDamage(AGPMCar* C, float Amount, bool bRear)
{
	if (!C || Amount < 0.03f) return;
	float& Part = bRear ? C->DmgR : C->Dmg;
	const float Before = Part;
	Part = FMath::Min(1.f, Part + Amount);
	if (Debris) Debris->Burst(C, FMath::Min(20, 3 + FMath::RoundToInt(Amount * 50.f)), bRear);
	if (Amount > 0.18f) RequestSafety(false);
	const bool bGone = bRear ? C->bRwOff : C->bFwOff;
	if (!bGone && ((Amount > 0.22f && FMath::FRand() < Amount * 2.5f) || (Part >= 0.95f && Amount > 0.08f)))
	{
		Part = 1.f;
		C->DetachWing(bRear);
		if (Debris) Debris->LooseWing(C, bRear);
		if (C == Car) Say(bRear ? TEXT("Muret : tu as perdu l'aileron arrière ! L'arrière va décrocher, rentre au stand.") : TEXT("Muret : l'aileron avant est parti ! Grosse perte d'appui, rentre au stand."));
		else Say(FString::Printf(TEXT("Muret : %s a perdu son aileron %s, débris sur la piste !"), *C->DriverName, bRear ? TEXT("arrière") : TEXT("avant")));
		return;
	}
	if (Before < 0.3f && Part >= 0.3f) C->BendWing(bRear, FMath::FRand() < 0.5f ? -1.f : 1.f);
	if (C == Car && Before < 0.15f && Part >= 0.15f)
		Say(bRear ? TEXT("Muret : aileron arrière abîmé, l'arrière sera instable. Un arrêt le réparera.") : TEXT("Muret : aileron avant endommagé, tu perds de l'appui. Un arrêt le réparera."));
}

// ---------- arrêts ----------

// faut-il rentrer à ce tour ? (IA, conseils du muret) : pluie ou piste sèche, crevaison, pneus à bout, aileron cassé
bool AGPMGameMode::WantBox(const AGPMCar* C) const
{
	const int32 Left = TotalLaps - C->Lap;
	if (C->Lap < 1 || Left < 1) return false;
	if (C->bFlat) return true;
	const bool bWetNeed = TrackWet > C->WetCall, bDryNeed = TrackWet < 0.2f && !bRaining;
	if (C->Tyre != TCHAR('W') && bWetNeed) return true;
	if (C->Tyre == TCHAR('W') && bDryNeed && Left > 2) return true;
	if (1.f - C->Wear < 0.25f && Left > 1) return true;
	if ((C->Dmg > 0.5f || C->DmgR > 0.5f) && Left > 2) return true;
	return false;
}

// pneus du prochain arrêt : pluie si la piste est mouillée, sinon ceux de la stratégie (ou la gomme la plus tendre qui tient)
TCHAR AGPMGameMode::NextStopTyre(const AGPMCar* C) const
{
	const bool bMine = C == Car.Get();
	if (bMine && bTyrePicked && ManualTyre != 0) return ManualTyre;
	if (TrackWet > 0.4f || (bRaining && TrackWet > 0.15f)) return TCHAR('W');
	const TArray<TPair<TCHAR, int32>>& Pl = bMine ? Stints : C->Plan;
	TCHAR Want = Pl.IsValidIndex(C->Stops + 1) ? Pl[C->Stops + 1].Key : TCHAR(0);
	if (bMine && ManualTyre != 0) Want = ManualTyre;
	if (Want == 0 || Want == TCHAR('W')) Want = PickTyre(FMath::Max(1, TotalLaps - C->Lap));
	return Want;
}

// ---------- radio du muret ----------

void AGPMGameMode::RadioCall(int32 K)
{
	if (!Car || Phase != EPhase::Race || Car->bFinished) return;
	if (K == 0) { Say(RadioBrief()); RadioT = 8.f; return; }
	if (K == 1)
	{
		Car->Mode = (Car->Mode + 1) % 3;   // normal → économie → attaque → normal
		Say(FString::Printf(TEXT("Muret : bien reçu, mode %s."), *AGPMCar::ModeName(Car->Mode)));
		return;
	}
	// pneus du prochain arrêt : tendres → mediums → durs → pluie
	const TCHAR Cur = Car->bBox ? Car->NextTyre : NextStopTyre(Car);
	const TCHAR Pick = Cur == TCHAR('S') ? TCHAR('M') : Cur == TCHAR('M') ? TCHAR('H') : Cur == TCHAR('H') ? TCHAR('W') : TCHAR('S');
	ManualTyre = Pick;
	bTyrePicked = true;
	if (Car->bBox) Car->NextTyre = Pick;
	Say(FString::Printf(TEXT("Muret : prochains pneus %s."), *TyreName(Pick)));
}

// point complet en un message : pneus, essence, écarts, prochain arrêt, dégâts, météo
FString AGPMGameMode::RadioBrief() const
{
	const FGPMTrackData* T = Car ? Car->GetTrack() : nullptr;
	if (!T) return FString();
	const int32 N = FMath::Max(1, T->WX.Num());
	const float Life = FMath::Max(0.f, 1.f - Car->Wear);
	const TCHAR* Band = Life >= 0.85f ? TEXT("maximal") : Life >= 0.6f ? TEXT("bon") : Life >= 0.4f ? TEXT("faible") : TEXT("quasi nul");
	const float PerLapFuel = N * 2.f / 1000.f * 1.7f * Car->UpFuel * AGPMCar::ModeFuel(Car->Mode);
	const float FuelLaps = PerLapFuel > 0.f ? Car->Fuel / PerLapFuel : 99.f;
	const float Need = FMath::Max(0.f, TotalLaps - Car->Lap + 1 - float(Car->Idx) / N);
	FString Msg = FString::Printf(TEXT("Muret : pneus %d %% (grip %s) · essence %.1f tours%s"), FMath::RoundToInt(Life * 100.f), Band, FuelLaps, FuelLaps < Need + 0.2f ? TEXT(" (juste !)") : TEXT(""));
	const int32 Me = Order.IndexOfByKey(Car.Get());
	auto GapS = [this, N](const AGPMCar* Front, const AGPMCar* Back) { return (Front->Progress() - Back->Progress()) / N * LapEstimate; };
	if (Me > 0) Msg += FString::Printf(TEXT(" · devant : %s à %.1f s"), *Order[Me - 1]->DriverName, GapS(Order[Me - 1], Car.Get()));
	else Msg += TEXT(" · en tête");
	if (Me >= 0 && Order.IsValidIndex(Me + 1)) Msg += FString::Printf(TEXT(" · derrière à %.1f s"), GapS(Car.Get(), Order[Me + 1]));
	if (Stints.IsValidIndex(Car->Stops + 1))
	{
		int32 Planned = 0;
		for (int32 s = 0; s <= Car->Stops; ++s) Planned += Stints[s].Value;
		Msg += FString::Printf(TEXT(" · arrêt fin du tour %d (%s)"), Planned, *TyreName(NextStopTyre(Car)));
	}
	else Msg += TEXT(" · plus d'arrêt prévu");
	if (Car->Dmg > 0.15f || Car->DmgR > 0.15f) Msg += Car->Dmg >= Car->DmgR ? TEXT(" · aileron avant abîmé") : TEXT(" · aileron arrière abîmé");
	if (Car->Mode != 1) Msg += FString::Printf(TEXT(" · mode %s"), *AGPMCar::ModeName(Car->Mode));
	if (bRaining) Msg += TEXT(" · pluie");
	else if (WxChangeAt >= 0.f && !bWxDone) Msg += bRainStart ? TEXT(" · éclaircie attendue") : TEXT(" · averse attendue");
	return Msg;
}

// ---------- secteurs des qualifications (qsession.js) ----------

float AGPMGameMode::QPole() const
{
	float Pole = 0.f;
	for (const TPair<AGPMCar*, float>& Q : QTimes)
		if (Q.Key != Car.Get() && (Pole <= 0.f || Q.Value < Pole)) Pole = Q.Value;
	return Pole;
}

bool AGPMGameMode::QDelta(float& Out) const
{
	const FGPMTrackData* T = Car ? Car->GetTrack() : nullptr;
	const float Ref = QPole();
	if (!T || Phase != EPhase::Quali || Car->Lap != 1 || Ref <= 0.f || QCum.Num() != T->WX.Num() + 1) return false;
	const int32 N = T->WX.Num();
	if (float(Car->Idx) < N * 0.03f) return false;
	Out = Car->LapTime() - QCum[Car->Idx] * Ref / FMath::Max(0.001f, QCum[N]);
	return true;
}

// passage à 1/3 et 2/3 du tour (instant corrigé avec la vitesse) ; le 3e secteur à l'arrivée (EndQuali)
void AGPMGameMode::UpdateSectors()
{
	const FGPMTrackData* T = Car ? Car->GetTrack() : nullptr;
	if (!T || Car->Lap != 1) return;
	const int32 N = T->WX.Num();
	for (int32 k = 0; k < 2; ++k)
	{
		const int32 Bd = N * (k + 1) / 3;
		if (QSplit[k] > 0.f || Car->Idx < Bd || Car->Idx > Bd + N / 6 || (k > 0 && QSplit[0] <= 0.f)) continue;
		QSplit[k] = FMath::Max(0.01f, Car->LapTime() - (Car->Idx - Bd) * 2.f / FMath::Max(5.f, Car->V));
		QSec[k] = QSplit[k] - (k > 0 ? QSplit[0] : 0.f);
		QCol[k] = SectorColour(k);
	}
}

// violet = meilleur de la séance (part de la pole), vert = record perso, jaune = plus lent
int32 AGPMGameMode::SectorColour(int32 K) const
{
	const FGPMTrackData* T = Car ? Car->GetTrack() : nullptr;
	const int32 N = T ? T->WX.Num() : 0;
	if (N < 3 || QCum.Num() != N + 1) return 2;
	const float Share = (QCum[N * (K + 1) / 3] - QCum[N * K / 3]) / FMath::Max(0.001f, QCum[N]);
	const float Sec = QSec[K], Fast = QPole() * Share, Own = QPB0[K];
	if (Sec <= Fast && (Own <= 0.f || Sec <= Own)) return 1;
	return Own <= 0.f || Sec <= Own ? 2 : 3;
}

bool AGPMGameMode::DigitPressed(const APlayerController* PC, int32 D)
{
	if (!PC || D < 1 || D > 6) return false;
	static const FKey Top[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
	static const FKey Pad[6] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix };
	static const FKey Azerty[6] = { EKeys::Ampersand, EKeys::E_AccentAigu, EKeys::Quote, EKeys::Apostrophe, EKeys::LeftParantheses, EKeys::Hyphen };
	return PC->WasInputKeyJustPressed(Top[D - 1]) || PC->WasInputKeyJustPressed(Pad[D - 1]) || PC->WasInputKeyJustPressed(Azerty[D - 1]);
}
