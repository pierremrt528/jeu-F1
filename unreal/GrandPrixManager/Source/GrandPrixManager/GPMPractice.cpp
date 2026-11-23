// Essais libres express, seul en piste avant les qualifications, trois missions (une fois chacune par week-end) :
//  - Trajectoire : passer dans les portes posées sur la trajectoire idéale (1 tour) → adhérence pour le week-end ;
//  - Pneus : un tour rapide en usant peu les mediums → usure réduite en course ;
//  - Essence : deux tours rapides en consommant peu (lever le pied avant les freinages) → consommation réduite en course.
// Le gain dépend du résultat (portes passées, usure ou essence mesurées) ; un tour trop lent ne compte qu'à moitié.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMTypes.h"
#include "Engine/GameInstance.h"

int32 AGPMGameMode::WeekendKey() const
{
	const UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	return bCareer && Season ? Season->Year() * 100 + Season->Round() : -100 - SoloIdx;
}

void AGPMGameMode::StartPractice(int32 Prog)
{
	if (!Car || Prog < 0 || Prog > 2) return;
	if (PracKey != WeekendKey()) { PracKey = WeekendKey(); PracMask = 0; PracMu = PracTyre = PracFuel = 1.f; PracReport.Reset(); }
	if (PracMask & (1 << Prog)) return;
	EnsureSetup();
	EndCine();
	// le tour lancé des qualifications (avant la séance seulement) ; les temps et records sont effacés à la fin
	if (bGridReady) return;
	bPractice = true; PracticeProg = Prog;
	StartQuali();
	if (Prog == 0) BuildGates();
	if (Prog == 1) { Car->Tyre = 'M'; Car->GripMul = AGPMCar::WetGrip(Car->Tyre, TrackWet); }
	if (Prog == 2) { Car->Fuel = FMath::Min(100.f, 6.f + NomFuel() * 2.8f); Car->Mass = 1.f + Car->Fuel / 750.f; }
	Say(Prog == 0 ? TEXT("Essais libres : passe dans les portes, elles marquent la trajectoire idéale. Plus tu en passes, mieux la voiture sera réglée.")
		: Prog == 1 ? TEXT("Essais libres : un tour en mediums. Rapide, mais freine en ligne droite et tourne en douceur pour ménager les pneus.")
		: TEXT("Essais libres : deux tours avec de l'essence. Lève le pied avant les freinages, chaque kilo économisé compte."));
}

void AGPMGameMode::EndPractice()
{
	bPractice = false;
	PracMask |= 1 << PracticeProg;
	const float Lap = Car ? Car->LastLap : 0.f, Ref = BaseLap();
	const FString LapTxt = Lap > 1.f ? FString::Printf(TEXT("%d:%06.3f"), int32(Lap) / 60, FMath::Fmod(Lap, 60.f)) : FString(TEXT("pas de temps"));
	// un tour trop lent (plus de 5 % au-dessus du rythme de la voiture) ne compte qu'à moitié
	const float Pace = Lap > 1.f && Ref > 0.f && Lap <= Ref * 1.05f && !bQInvalid ? 1.f : 0.5f;
	if (PracticeProg == 0)
	{
		const float Frac = LapGate.Num() ? LapGate.Last() : 0.f, Sc = Frac * Frac * Pace;
		PracMu = 1.f + 0.008f * Sc;
		PracReport = FString::Printf(TEXT("Portes : %.0f %% (%s). Voiture réglée sur ta trajectoire : +%.1f %% d'adhérence pour le week-end."), Frac * 100.f, *LapTxt, (PracMu - 1.f) * 100.f);
		// record de portes sur ce circuit (carrière)
		UGPMSeasonSubsystem* Ss = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
		UGPMSaveGame* Sg = bCareer && Ss ? Ss->State() : nullptr;
		if (Sg && CurTrack())
		{
			const FString Key = TEXT("gates|") + CurTrack()->Name;
			const int32 Pc = FMath::RoundToInt(Frac * 100.f), Old = Sg->Rec.FindRef(Key);
			if (Pc > Old) { Sg->Rec.Add(Key, Pc); if (Old > 0) PracReport += FString::Printf(TEXT(" Nouveau record ici (avant : %d %%) !"), Old); }
			else PracReport += FString::Printf(TEXT(" Record ici : %d %%."), Old);
		}
	}
	else if (PracticeProg == 1)
	{
		const float Used = LapWear.Num() ? LapWear.Last() : NomWear(), Ratio = Used / FMath::Max(0.001f, NomWear());
		const float Sc = FMath::Clamp((1.2f - Ratio) / 0.45f, 0.f, 1.f) * Pace;
		PracTyre = 1.f - 0.1f * Sc;
		PracReport = FString::Printf(TEXT("Usure : %.1f %% en un tour, %.0f %% de la normale (%s). Les pneus s'useront %.0f %% moins vite en course."), Used * 100.f, Ratio * 100.f, *LapTxt, Sc * 10.f);
	}
	else
	{
		float Sum = 0.f;
		for (const float F : LapFuel) Sum += F;
		const float Avg = LapFuel.Num() ? Sum / LapFuel.Num() : NomFuel(), Ratio = Avg / FMath::Max(0.1f, NomFuel());
		const float Sc = FMath::Clamp((1.05f - Ratio) / 0.3f, 0.f, 1.f) * Pace;
		PracFuel = 1.f - 0.06f * Sc;
		PracReport = FString::Printf(TEXT("Essence : %.1f kg par tour, %.0f %% de la normale (dernier tour %s). Consommation en course : -%.0f %%."), Avg, Ratio * 100.f, *LapTxt, Sc * 6.f);
	}
	ClearGates();
	// le rapport de l'ingénieur arrive aussi dans la boîte mail
	if (bCareer)
		if (UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>())
			Career->SendMail(TEXT("equipe"), TEXT("Ingénieur de course"), FString::Printf(TEXT("Essais libres : %s"), *FString(PracticeName(PracticeProg)).ToLower()), PracReport);
	// retour à l'écran des réglages, les autres voitures reviennent pour les qualifications
	QTimes.Reset();
	for (int32 k = 0; k < 3; ++k) QSec[k] = QPB[k] = 0.f;
	if (Car) { Car->V = 0.f; Car->bHold = true; }
	OpenTuning();
	Say(TEXT("Muret : bien reçu, on analyse les données."));
}
