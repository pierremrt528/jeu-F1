// Fiabilité : le motoriste choisi (note « Fiabilité ») et les pièces du pôle Fiabilité de l'usine décident du risque d'ennui
// moteur à chaque course. Un moteur qui faiblit perd de la puissance jusqu'à l'arrivée (courses jouées et simulées).
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
#include "GPMSeason.h"
#include "Engine/GameInstance.h"

// risque d'ennui moteur sur une course (0 … 0,2)
float AGPMGameMode::EngineRisk(const AGPMCar* C) const
{
	if (!C) return 0.f;
	const UGameInstance* GI = GetGameInstance();
	if (bCareer && Car && C->TeamIdx == Car->TeamIdx && GI)
	{
		const UGPMCareerSubsystem* Career = GI->GetSubsystem<UGPMCareerSubsystem>();
		const UGPMFactorySubsystem* Factory = GI->GetSubsystem<UGPMFactorySubsystem>();
		const UGPMSaveGame* G = Career ? Career->Save() : nullptr;
		if (G && Factory)
		{
			int32 Fiab = 82;
			for (const FGPMEngineOpt& E : Career->Engines) if (E.Id == G->Engine) Fiab = E.Fiab;
			float Rel = 0.f;
			for (const FString& Id : G->Owned)
				if (const FGPMPart* P = Factory->Part(Id))
					if (const float* V = P->Fx.Find(TEXT("rel"))) Rel += *V;
			// motoriste : 72 → 14 %, 94 → 3 % ; les pièces de fiabilité retirent leur pourcentage du risque
			return FMath::Clamp((100 - Fiab) / 100.f * 0.5f * (1.f - FMath::Min(Rel, 90.f) / 100.f), 0.005f, 0.2f);
		}
	}
	// écuries rivales et course solo : 4 à 10 % selon l'écurie
	return 0.04f + 0.06f * ((GetTypeHash(C->TeamName) % 7) / 6.f);
}

// au départ : quels moteurs faibliront, et à quelle part de la course
void AGPMGameMode::RollReliability()
{
	EngineFailAt.Reset();
	EngineTold.Reset();
	EngineBlown.Reset();
	bHadFlat = false;
	for (const AGPMCar* C : Order)
		if (C && FMath::FRand() < EngineRisk(C))
		{
			EngineFailAt.Add(C, 0.15f + 0.75f * FMath::FRand());
			if (FMath::FRand() < 0.4f) EngineBlown.Add(C);   // 4 fois sur 10, le moteur casse pour de bon
		}
}

// le moteur de cette voiture faiblit-il maintenant ? (message du muret la première fois)
bool AGPMGameMode::EngineTrouble(const AGPMCar* C)
{
	const float* At = EngineFailAt.Find(C);
	const FGPMTrackData* T = C ? C->GetTrack() : nullptr;
	if (!At || !T || TotalLaps <= 0 || C->bFinished || C->bOut) return false;
	const float Frac = (C->Progress() / float(FMath::Max(1, T->WX.Num())) - 1.f) / TotalLaps;
	if (Frac < *At) return false;
	if (EngineBlown.Contains(C))
	{
		if (C->PitPhase) return false;   // dans la voie des stands : la casse attend la sortie
		for (AGPMCar* X : Cars) if (X == C) Retire(X, TEXT("moteur"));
		return false;
	}
	if (!EngineTold.Contains(C))
	{
		EngineTold.Add(C);
		if (C == Car.Get()) Say(TEXT("Muret : problème moteur ! On perd de la puissance. Ramène la voiture à l'arrivée."));
		else if (Car && C->TeamIdx == Car->TeamIdx) Say(FString::Printf(TEXT("Muret : ennui moteur pour %s, il perd de la puissance."), *C->DriverName));
		UE_LOG(LogGPM, Log, TEXT("Fiabilité : ennui moteur pour %s (%s) à %.0f %% de la course"), *C->DriverName, *C->TeamName, Frac * 100.f);
	}
	return true;
}

// abandon : la voiture coupe son effort et s'arrête (Strategy l'évacue ensuite) ; voiture de sécurité possible
void AGPMGameMode::Retire(AGPMCar* C, const TCHAR* Why)
{
	if (!C || C->bOut || C->bFinished || C == SC.Get() || Phase != EPhase::Race) return;
	C->bOut = true; C->OutT = 0.f; C->OutWhy = Why;
	C->FinProgress = C->Progress();
	C->bBox = false;
	const bool bEngine = C->OutWhy == TEXT("moteur");
	if (C == Car.Get()) Say(bEngine ? TEXT("Muret : le moteur a cassé… C'est l'abandon. Gare la voiture, désolé.") : TEXT("Muret : la voiture est trop abîmée, c'est l'abandon. Tu vas bien ?"));
	else Say(FString::Printf(TEXT("Muret : abandon de %s (%s)."), *C->DriverName, bEngine ? TEXT("casse moteur") : TEXT("accident")));
	UE_LOG(LogGPM, Log, TEXT("Abandon : %s (%s), %s, tour %d"), *C->DriverName, *C->TeamName, Why, C->Lap);
	RequestSafety(false, bEngine ? FMath::FRand() < 0.7f : FMath::FRand() < 0.35f);
	// deux accidents rapprochés (ou parfois un seul très gros) : drapeau rouge, la piste est à nettoyer
	if (bEngine) return;
	CrashRun = Clock - CrashAt < 10.f ? CrashRun + 1 : 1;
	CrashAt = Clock;
	if ((CrashRun >= 2 && FMath::FRand() < 0.6f) || FMath::FRand() < 0.05f)
		RedFlag(CrashRun >= 2 ? TEXT("Plusieurs voitures accidentées : piste encombrée") : TEXT("Accident : barrières à réparer"));
}

FString AGPMGameMode::ResultGap(const AGPMCar* C) const
{
	if (!C || Order.Num() == 0) return FString();
	if (C->bOut) return C->OutWhy == TEXT("moteur") ? TEXT("Casse moteur") : C->OutWhy == TEXT("drapeau noir") ? TEXT("Disqualifié") : TEXT("Accident");
	const AGPMCar* Win = Order[0];
	const FGPMTrackData* T = C->GetTrack();
	const float N = T ? FMath::Max(1, T->WX.Num()) : 1.f;
	const float WinAt = Win->Classified() ? Win->FinProgress : Win->Progress();
	const int32 Down = FMath::FloorToInt((WinAt - (C->Classified() ? C->FinProgress : C->Progress())) / N + 0.001f);
	if (Down >= 1 && (C->Classified() || Win->Classified())) return Down == 1 ? FString(TEXT("+1 tour")) : FString::Printf(TEXT("+%d tours"), Down);
	return FString::Printf(TEXT("+%.3f%s"), FMath::Max(0.f, ResultTime(C) - ResultTime(Win)), C->Classified() ? TEXT("") : TEXT(" (est.)"));
}
