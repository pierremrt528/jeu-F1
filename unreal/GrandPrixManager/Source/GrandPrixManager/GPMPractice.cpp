// Essais libres express : 1 tour (réglages, pneus) ou 2 tours (long run) seul en piste, avant les qualifications.
// Chaque programme se fait une fois par week-end et apporte un petit avantage pour la course : voiture mieux réglée (adhérence),
// gommes mieux comprises (usure), consommation mieux connue (carburant). Les temps des qualifications ne sont pas touchés.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMSeason.h"
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
	if (Prog == 1) { Car->Tyre = 'M'; Car->GripMul = AGPMCar::WetGrip(Car->Tyre, TrackWet); }
	if (Prog == 2) { Car->Fuel = 40.f; Car->Mass = 1.f + Car->Fuel / 750.f; }
	Say(Prog == 0 ? TEXT("Essais libres : un tour pour valider les réglages. Pousse, on regarde les données.")
		: Prog == 1 ? TEXT("Essais libres : un tour en mediums, ménage-les, on mesure l'usure.")
		: TEXT("Essais libres : long run de deux tours avec de l'essence, rythme de course."));
}

void AGPMGameMode::EndPractice()
{
	bPractice = false;
	PracMask |= 1 << PracticeProg;
	const float Lap = Car ? Car->LastLap : 0.f;
	const FString LapTxt = Lap > 1.f ? FString::Printf(TEXT("%d:%06.3f"), int32(Lap) / 60, FMath::Fmod(Lap, 60.f)) : FString(TEXT("pas de temps"));
	if (PracticeProg == 0)
	{
		PracMu = 1.004f;
		PracReport = FString::Printf(TEXT("Réglages validés (%s) : un peu plus d'adhérence pour tout le week-end."), *LapTxt);
	}
	else if (PracticeProg == 1)
	{
		const float Used = Car ? Car->Wear * 100.f : 0.f;
		PracTyre = 0.94f;
		PracReport = FString::Printf(TEXT("Usure mesurée : %.1f %% en un tour (%s). Les pneus s'useront 6 %% moins vite en course."), Used, *LapTxt);
	}
	else
	{
		PracFuel = 0.97f;
		PracReport = FString::Printf(TEXT("Long run bouclé (dernier tour %s) : consommation affinée, 3 %% d'essence en moins en course."), *LapTxt);
	}
	// retour à l'écran des réglages, les autres voitures reviennent pour les qualifications
	QTimes.Reset();
	for (int32 k = 0; k < 3; ++k) QSec[k] = QPB[k] = 0.f;
	if (Car) { Car->V = 0.f; Car->bHold = true; }
	OpenTuning();
	Say(TEXT("Muret : bien reçu, on analyse les données."));
}
