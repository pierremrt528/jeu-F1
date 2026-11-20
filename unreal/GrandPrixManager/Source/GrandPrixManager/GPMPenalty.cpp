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
		C->PenCd = 6.f;
		Penalize(C, 0, 5.f, bInside ? TEXT("Virage coupé avec un gain de temps") : TEXT("Avantage pris hors de la piste"), 5.f);
		return;
	}
	// barème : 3 avertissements (le 3e avec le drapeau noir et blanc), puis 5 s, 10 s, et drive-through
	++C->Strikes;
	if (C->Strikes <= 3)
	{
		Say(FString::Printf(TEXT("Commissaires : limites de piste, avertissement %d sur 3%s"), C->Strikes, C->Strikes == 3 ? TEXT(" (drapeau noir et blanc). La prochaine, c'est 5 s.") : TEXT(".")));
		StewardHead = FString::Printf(TEXT("AVERTISSEMENT %d/3"), C->Strikes); StewardWhy = TEXT("Limites de piste"); StewardCar = C; StewardT = 5.f; bStewardInquiry = false;
	}
	else if (C->Strikes == 4) Penalize(C, 0, 5.f, TEXT("Limites de piste (4e infraction)"), 4.f);
	else if (C->Strikes == 5) Penalize(C, 0, 10.f, TEXT("Limites de piste (5e infraction)"), 4.f);
	else Penalize(C, 1, 0.f, TEXT("Limites de piste répétées"), 4.f);
}

// accrochage : le fautif (celui qui arrive par l'arrière) ; IA contre IA seulement pour les gros chocs, pas dans la mêlée du départ
void AGPMGameMode::ContactPenalty(AGPMCar* Guilty, AGPMCar* Victim, float Close)
{
	if (Phase != EPhase::Race || !Guilty || !Victim || Guilty->PenCd > 0.f) return;
	if (Guilty == SC.Get() || Victim == SC.Get() || Guilty->bOut || Victim->bOut) return;
	const bool bMine = Guilty == Car || Victim == Car;
	if (Guilty == Car && !Car->bPlayerControlled) return;
	if (Close < (Clock < 20.f ? 14.f : 9.f) || (!bMine && (Close < 16.f || FMath::FRand() > 0.4f))) return;
	Guilty->PenCd = 8.f;
	const FString Why = FString::Printf(TEXT("Accrochage avec %s"), *Victim->DriverName);
	// sous voiture de sécurité ou très gros choc : drive-through ; gros choc : 10 s ; sinon 5 s
	if (SafetyOut() || Close > 24.f) Penalize(Guilty, 1, 0.f, Why, 8.f);
	else Penalize(Guilty, 0, Close > 16.f ? 10.f : 5.f, Why, 8.f);
	UE_LOG(LogGPM, Log, TEXT("Commissaires : accrochage, %s fautif (choc %.0f m/s)"), *Guilty->DriverName, Close);
}

void AGPMGameMode::Penalize(AGPMCar* C, uint8 Kind, float Sec, const FString& Why, float Delay)
{
	if (!C || C == SC.Get()) return;
	if (Delay <= 0.f) { Decide(C, Kind, Sec, Why); return; }
	FInquiry& Q = Inquiries.AddDefaulted_GetRef();
	Q.C = C; Q.Kind = Kind; Q.Sec = Sec; Q.At = Clock + Delay; Q.Why = Why;
	StewardHead = TEXT("INCIDENT EN ENQUÊTE"); StewardWhy = Why; StewardCar = C; StewardT = Delay + 0.5f; bStewardInquiry = true;
	if (C == Car) Say(FString::Printf(TEXT("Muret : les commissaires enquêtent sur toi : %s."), *Why.ToLower()));
}

void AGPMGameMode::Decide(AGPMCar* C, uint8 Kind, float Sec, const FString& Why)
{
	if (!C || C->bOut) return;
	// à moins de 3 tours de l'arrivée (ou arrivé) : drive-through = 20 s, stop-and-go = 30 s ajoutées au temps
	if (Kind && (C->bFinished || TotalLaps - C->Lap <= 3)) { Sec = Kind == 1 ? 20.f : 30.f; Kind = 0; }
	if (Kind == 0)
	{
		C->Penalty += Sec; C->PenSum += Sec;
		if (!C->bFinished) C->PenHold += Sec;
	}
	else
	{
		C->PenKind = FMath::Max(C->PenKind, Kind); C->PenDue = C->Lap + 3;
		if (!C->bPlayerControlled || C != Car) C->bBox = true;
	}
	const FString What = Kind == 1 ? FString(TEXT("DRIVE-THROUGH")) : Kind == 2 ? FString(TEXT("STOP-AND-GO 10 S")) : FString::Printf(TEXT("PÉNALITÉ DE %.0f S"), Sec);
	StewardHead = What; StewardWhy = Why; StewardCar = C; StewardT = 7.f; bStewardInquiry = false;
	UE_LOG(LogGPM, Log, TEXT("Commissaires : %s pour %s (%s)"), *What, *C->DriverName, *Why);
	if (C == Car)
		Say(Kind == 0 ? FString::Printf(TEXT("Commissaires : %.0f s de pénalité (%s). À purger au prochain arrêt, sinon ajoutées à ton temps."), Sec, *Why.ToLower())
			: FString::Printf(TEXT("Commissaires : %s (%s) ! Passe par la voie des stands avant la fin du tour %d, sinon drapeau noir."), Kind == 1 ? TEXT("drive-through") : TEXT("stop-and-go de 10 s"), *Why.ToLower(), C->PenDue + 1));
	else if (Car && C->TeamIdx == Car->TeamIdx) Say(FString::Printf(TEXT("Muret : %s pénalisé : %s."), *C->DriverName, *What.ToLower()));
}

// enquêtes qui aboutissent, et pénalités de passage non purgées à temps : drapeau noir
void AGPMGameMode::Stewards(float)
{
	for (int32 i = Inquiries.Num() - 1; i >= 0; --i)
	{
		if (Clock < Inquiries[i].At) continue;
		const FInquiry Q = Inquiries[i];
		Inquiries.RemoveAt(i);
		if (AGPMCar* C = Q.C.Get()) Decide(C, Q.Kind, Q.Sec, Q.Why);
	}
	for (AGPMCar* C : Cars)
	{
		if (!C || !C->PenKind || C->bOut || C->bFinished || C->PitPhase || C->Lap <= C->PenDue) continue;
		C->bOut = true; C->OutT = 0.f; C->OutWhy = TEXT("drapeau noir"); C->FinProgress = C->Progress(); C->bBox = false; C->PenKind = 0;
		StewardHead = TEXT("DRAPEAU NOIR · DISQUALIFIÉ"); StewardWhy = TEXT("Pénalité non purgée"); StewardCar = C; StewardT = 7.f; bStewardInquiry = false;
		Say(C == Car ? FString(TEXT("Muret : drapeau noir… tu n'as pas purgé ta pénalité, tu es disqualifié.")) : FString::Printf(TEXT("Muret : drapeau noir pour %s, disqualifié."), *C->DriverName));
	}
}

// arrêt dans le carré : stop-and-go (10 s, pas de travail), drive-through fait à l'arrêt (on repart), ou secondes à purger
float AGPMGameMode::ServeAtStop(AGPMCar* C)
{
	if (!C) return 0.f;
	if (C->PenKind)
	{
		const bool bSG = C->PenKind == 2;
		C->PenKind = 0; C->bPenVisit = true;
		if (C == Car) Say(bSG ? TEXT("Muret : stop-and-go, 10 s sans toucher la voiture… puis go !") : TEXT("Muret : drive-through purgé, repars !"));
		return bSG ? 10.f : 0.3f;
	}
	const float H = C->PenHold;
	if (H <= 0.f) return 0.f;
	C->PenHold = 0.f; C->Penalty = FMath::Max(0.f, C->Penalty - H);
	if (C == Car) Say(FString::Printf(TEXT("Muret : on purge tes %.0f s de pénalité, les mécaniciens attendent…"), H));
	return H;
}
