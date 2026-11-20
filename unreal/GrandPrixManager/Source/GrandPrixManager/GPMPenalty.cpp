// Commissaires de course : limites de piste (couper un virage ou élargir sans perdre de temps) et accrochages.
// L'herbe ne ralentit plus la voiture : couper est donc tentant, et sanctionné. Trois avertissements valent 5 s de pénalité ;
// un gain de temps net (chicane coupée) vaut 5 s tout de suite ; en qualifications, le tour est annulé.
// Percuter l'arrière d'une voiture : 5 s (10 s pour un gros choc). Les pénalités s'ajoutent au temps de course.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMTypes.h"

void AGPMGameMode::TrackLimits(float Dt)
{
	for (AGPMCar* C : Cars) if (C) C->PenCd = FMath::Max(0.f, C->PenCd - Dt);
	AGPMCar* C = Car.Get();
	const FGPMTrackData* T = C ? C->GetTrack() : nullptr;
	if (!C || !T || (Phase != EPhase::Race && Phase != EPhase::Quali)) return;
	const int32 N = T->WX.Num();
	if (!C->bPlayerControlled || C->bOut || C->bFinished || C->PitPhase || C->Lap < 1 || N < 50 || T->AISpeed.Num() != N) { C->OffIdx = -1; return; }
	if (C->bOffTrack)
	{
		if (C->OffIdx < 0) { C->OffIdx = C->Idx; C->OffT = C->RaceTime; C->OffSide = C->Lat > 0.f ? 1 : -1; C->bOffGravel = false; }
		C->bOffGravel |= C->Ground == 2 || C->SpinT > 0.f;   // gravier ou tête-à-queue : la sortie est déjà punie
		return;
	}
	if (C->OffIdx < 0) return;
	// retour en piste : temps mis sur ce tronçon, comparé au temps normal de cette voiture
	const int32 From = C->OffIdx, Len = ((C->Idx - From) % N + N) % N;
	const float Took = C->RaceTime - C->OffT;
	const bool bGravel = C->bOffGravel;
	C->OffIdx = -1;
	if (Len < 6 || Len > N / 3 || bGravel || (Phase == EPhase::Race && Clock < 12.f)) return;
	float Ref = 0.f, RefLap = 0.f, MaxC = 0.f;
	int32 Turn = 0;
	for (int32 i = 0; i < N; ++i) RefLap += 2.f / FMath::Max(5.f, T->AISpeed[i]);
	for (int32 k = 0; k < Len; ++k)
	{
		const int32 i = (From + k) % N;
		Ref += 2.f / FMath::Max(5.f, T->AISpeed[i]);
		const float Cv = T->Curvature.IsValidIndex(i) ? FMath::Abs(T->Curvature[i]) : 0.f;
		if (Cv > MaxC) { MaxC = Cv; Turn = T->TurnAt(i); }
	}
	const float MyLap = C->BestLap > 0.f ? C->BestLap : Phase == EPhase::Quali && QPole() > 0.f ? QPole() * 1.03f : LapEstimate;
	const float Gain = Ref * MyLap / FMath::Max(1.f, RefLap) - Took;
	const bool bInside = MaxC > 1.f / 200.f && Turn == C->OffSide;   // sorti du côté intérieur du virage : il a coupé
	if (bInside ? Gain < -0.15f : Gain < 0.25f) return;              // temps perdu : pas de sanction
	UE_LOG(LogGPM, Log, TEXT("Commissaires : limites de piste (%s, %d m, gain %.2f s)"), bInside ? TEXT("virage coupé") : TEXT("sortie large"), Len * 2, Gain);
	if (Phase == EPhase::Quali)
	{
		if (!bQInvalid) Say(TEXT("Commissaires : limites de piste franchies, ce tour ne comptera pas."));
		bQInvalid = true;
		return;
	}
	if (Gain > 0.6f && C->PenCd <= 0.f)
	{
		C->Penalty += 5.f; C->PenCd = 6.f;
		Say(bInside ? TEXT("Commissaires : virage coupé avec un gain de temps, pénalité de 5 s.") : TEXT("Commissaires : sortie de piste avec un gain de temps, pénalité de 5 s."));
	}
	else if (++C->Strikes >= 3)
	{
		C->Strikes = 0; C->Penalty += 5.f;
		Say(TEXT("Commissaires : troisième avertissement pour les limites de piste, pénalité de 5 s."));
	}
	else Say(FString::Printf(TEXT("Commissaires : limites de piste, avertissement %d sur 3."), C->Strikes));
}

// accrochage : seulement ceux qui concernent le joueur (les IA règlent leurs comptes entre elles), pas dans la mêlée du départ
void AGPMGameMode::ContactPenalty(AGPMCar* Guilty, AGPMCar* Victim, float Close)
{
	if (Phase != EPhase::Race || !Guilty || !Victim || (Guilty != Car && Victim != Car) || Guilty->PenCd > 0.f) return;
	if (Guilty == SC.Get() || Victim == SC.Get() || Guilty->bOut || Victim->bOut) return;
	if (Guilty == Car && !Car->bPlayerControlled) return;
	if (Close < (Clock < 20.f ? 14.f : 9.f)) return;
	const float Pen = Close > 16.f ? 10.f : 5.f;
	Guilty->Penalty += Pen; Guilty->PenCd = 8.f;
	if (Guilty == Car) Say(FString::Printf(TEXT("Commissaires : tu as percuté %s, pénalité de %.0f s."), *Victim->DriverName, Pen));
	else Say(FString::Printf(TEXT("Commissaires : %s t'a percuté, il prend %.0f s de pénalité."), *Guilty->DriverName, Pen));
	UE_LOG(LogGPM, Log, TEXT("Commissaires : accrochage, %s pénalisé de %.0f s (choc %.0f m/s)"), *Guilty->DriverName, Pen, Close);
}
