// Palmarès de l'écurie, d'une saison à l'autre : départs, victoires, podiums, poles, meilleurs tours, doublés, abandons,
// meilleur résultat, vitrine des trophées (podiums et titres) et record du tour de l'écurie sur chaque circuit.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMTypes.h"
#include "GPMSeason.h"
#include "Engine/GameInstance.h"

void AGPMGameMode::RecordHonours(int32 Pos)
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || !Car || !CurTrack()) return;
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
	if (Pos == 1)
	{
		const int32 W = G->Rec.FindRef(TEXT("wins"));
		if (W == 1 || W % 10 == 0) G->News.Add(W == 1 ? FString(TEXT("Première victoire de l'histoire de l'écurie !")) : FString::Printf(TEXT("%de victoire de l'histoire de l'écurie !"), W));
	}
}
