// Palmarès de l'écurie, d'une saison à l'autre : départs, victoires, podiums, poles, meilleurs tours, doublés, abandons,
// meilleur résultat, vitrine des trophées (podiums et titres) et record du tour de l'écurie sur chaque circuit.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMTypes.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "Engine/GameInstance.h"

FString AGPMGameMode::RecordHonours(int32 Pos)
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || !Car || !CurTrack()) return FString();
	const FString Circ = CurTrack()->Name;
	auto Add = [G](const TCHAR* Key) { G->Rec.FindOrAdd(Key)++; };
	Add(TEXT("starts"));
	int32 MatePos = 0;
	for (int32 k = 0; k < Order.Num(); ++k) if (Order[k] != Car && Order[k]->TeamIdx == Car->TeamIdx && !Order[k]->bOut) { MatePos = k + 1; break; }
	if (Car->bOut) Add(TEXT("dnf"));
	if (Pos == 1) Add(TEXT("wins"));
	if (Pos >= 1 && Pos <= 3) { Add(TEXT("pods")); G->Trophies.Add(FString::Printf(TEXT("%d|GP %s|%d"), G->Year, *Circ, Pos)); }
	if (GridSlot == 0) Add(TEXT("poles"));
	if ((Pos == 1 && MatePos == 2) || (Pos == 2 && MatePos == 1)) Add(TEXT("doubles"));
	const AGPMCar* FastC = nullptr;
	for (const AGPMCar* C : Order) if (!C->bOut && C->BestLap > 0.f && (!FastC || C->BestLap < FastC->BestLap)) FastC = C;
	if (FastC == Car.Get()) Add(TEXT("fast"));
	if (Pos >= 1 && (!G->Rec.Contains(TEXT("best")) || Pos < G->Rec[TEXT("best")])) G->Rec.Add(TEXT("best"), Pos);
	// record du tour de l'écurie sur ce circuit (tour joué seulement)
	if (Car->BestLap > 1.f && Car->RaceTime > 1.f)
	{
		const float* Old = G->LapRec.Find(Circ);
		if (!Old || Car->BestLap < *Old)
		{
			G->LapRec.Add(Circ, Car->BestLap);
			if (Old) G->News.Add(FString::Printf(TEXT("Nouveau record de l'écurie à %s : %d:%06.3f"), *Circ, int32(Car->BestLap) / 60, FMath::Fmod(Car->BestLap, 60.f)));
		}
	}
	// duel avec l'équipier (course et qualifications)
	for (int32 k = 0; k < Order.Num(); ++k)
		if (Order[k] != Car && Order[k]->TeamIdx == Car->TeamIdx)
		{
			Add(TEXT("mateRaces"));
			if (Pos >= 1 && Pos < k + 1) Add(TEXT("mateBeat"));
			int32 MateGrid = -1;
			for (int32 q = 0; q < QOrder.Num(); ++q) if (QOrder[q] == Order[k]) MateGrid = q;
			if (MateGrid >= 0 && GridSlot < MateGrid) Add(TEXT("mateQBeat"));
			break;
		}
	// pilote du jour, le vote des fans : la plus belle remontée, un bonus pour le podium, un peu de hasard
	FString Dotd;
	{
		const AGPMCar* Best = nullptr;
		int32 BestFrom = 0, BestTo = 0;
		float BestScore = -1e9f;
		for (int32 k = 0; k < Order.Num(); ++k)
		{
			const AGPMCar* C = Order[k];
			if (C->bOut) continue;
			int32 From = k + 1;
			for (int32 q = 0; q < QOrder.Num(); ++q) if (QOrder[q] == C) From = q + 1;
			if (C == Car.Get()) From = GridSlot + 1;
			const float Score = float(From - (k + 1)) + (k == 0 ? 3.f : k < 3 ? 1.5f : 0.f) + FMath::FRandRange(0.f, 3.f);
			if (Score > BestScore) { BestScore = Score; Best = C; BestFrom = From; BestTo = k + 1; }
		}
		if (Best)
		{
			const int32 Votes = FMath::RandRange(24, 46);
			const FString Climb = BestFrom > BestTo ? FString::Printf(TEXT("de P%d à P%d"), BestFrom, BestTo) : FString::Printf(TEXT("P%d"), BestTo);
			if (Best == Car.Get())
			{
				Add(TEXT("dotd"));
				G->Rep = UGPMCareerSubsystem::Bump(G->Rep, 2.f);
				G->Moral = UGPMCareerSubsystem::Bump(G->Moral, 3.f);
				Dotd = FString::Printf(TEXT("Les fans t'élisent pilote du jour (%d %% des votes, %s) : réputation et moral en hausse."), Votes, *Climb);
			}
			else Dotd = FString::Printf(TEXT("Pilote du jour : %s (%d %% des votes, %s)."), *Best->DriverName, Votes, *Climb);
		}
	}
	if (Pos == 1)
	{
		const int32 W = G->Rec.FindRef(TEXT("wins"));
		if (W == 1 || W % 10 == 0) G->News.Add(W == 1 ? FString(TEXT("Première victoire de l'histoire de l'écurie !")) : FString::Printf(TEXT("%de victoire de l'histoire de l'écurie !"), W));
	}
	return Dotd;
}
