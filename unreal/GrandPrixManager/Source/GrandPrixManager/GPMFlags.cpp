// Drapeaux de course : jaune local (voiture arrêtée ou en tête-à-queue : on ralentit, interdiction de dépasser dans la zone),
// bleu (le joueur va être pris d'un tour : il doit laisser passer, sinon 5 s), rouge (course suspendue : tout le monde ralentit,
// puis retour sur la grille dans l'ordre de la course, pneus neufs et voitures réparées, et nouveau départ arrêté aux feux).
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMTypes.h"

void AGPMCar::Regrid(int32 Slot, int32 NewLap, TCHAR NewTyre)
{
	if (!Track || Track->WX.Num() < 4) return;
	const int32 N = Track->WX.Num();
	Idx = ((-(5 + Slot * 4)) % N + N) % N;
	const float Side = Slot % 2 ? 3.f : -3.f;
	Lat = Lane = Side;
	X = Track->WX[Idx] - Track->TZ[Idx] * Side;
	Z = Track->WZ[Idx] + Track->TX[Idx] * Side;
	Y = Track->WY[Idx];
	Heading = Track->H[Idx];
	V = Beta = Ax = Ay = SpinT = 0.f;
	PassTimer = YieldCd = YieldRun = Slipstream = Wake = 0.f;
	Lap = NewLap;
	PitPhase = 0; PitStopT = 0.f; bPitManual = false; bBox = false; bPenVisit = bDriveThru = false;
	Tyre = NewTyre; Wear = 0.f; bFlat = false; bAdvised = false;
	Dmg = DmgR = DmgNew = DmgNewR = 0.f; DmgKd = RwKd = 1.f;
	RepairWings();
	SlowMul = 1.f; bSCMode = false; bHold = true;
	UpdateTransform();
}

void AGPMGameMode::RedFlag(const FString& Why, bool bForce)
{
	if (RedState || (bRedUsed && !bForce) || Phase != EPhase::Race || Order.Num() == 0) return;
	const FGPMTrackData* T = Order[0]->GetTrack();
	if (!T || Order[0]->Lap < 1) return;
	if (!bForce && (Order[0]->Progress() / float(T->WX.Num()) - 1.f) / TotalLaps > 0.9f) return;   // trop tard : on finit sous drapeau jaune
	bRedUsed = true; RedState = 1; RedT = 0.f; RedWhy = Why;
	// la voiture de sécurité rentre : le drapeau rouge prend le relais
	if (SC) { for (AGPMCar* C : Cars) C->Field.Remove(SC); SC->Destroy(); SC = nullptr; }
	SCState = 0; SCAhead.Reset(); Yellows.Reset(); YellowAhead.Reset();
	for (AGPMCar* C : Cars) { C->SlowMul = 0.5f; C->bSCMode = false; }
	StewardHead = TEXT("DRAPEAU ROUGE"); StewardWhy = Why; StewardCar = nullptr; StewardT = 6.f; bStewardInquiry = false;
	Say(TEXT("Muret : DRAPEAU ROUGE ! Course suspendue, ralentis, on retourne sur la grille."));
	UE_LOG(LogGPM, Log, TEXT("Drapeau rouge : %s"), *Why);
}

void AGPMGameMode::UpdateRed(float Dt)
{
	if (!RedState) return;
	RedT += Dt;
	if (RedState == 1 && RedT > 7.f && Order.Num())
	{
		// retour sur la grille dans l'ordre de la course ; les retardataires gardent leur(s) tour(s) de retard
		const AGPMCar* Lead = nullptr;
		for (const AGPMCar* C : Order) if (!C->bOut && !C->bFinished) { Lead = C; break; }
		const FGPMTrackData* T = Lead ? Lead->GetTrack() : nullptr;
		if (!T) { RedState = 0; return; }
		const int32 N = T->WX.Num();
		const float LeadP = Lead->Progress();
		const int32 LeadLap = Lead->Lap - (Lead->Idx < N / 3 ? 1 : 0);
		int32 Slot = 0;
		TArray<AGPMCar*> Run = Order;
		for (AGPMCar* C : Run)
		{
			if (C->bOut) { if (C != Car && !C->bParked) { Park(C, true); for (AGPMCar* O : Cars) O->Field.Remove(C); } continue; }
			if (C->bFinished) continue;
			const int32 Down = FMath::Max(0, FMath::FloorToInt((LeadP - C->Progress()) / N));
			const int32 Left = TotalLaps - LeadLap;
			C->Regrid(Slot++, LeadLap - Down, TrackWet > 0.35f ? RainTyre() : PickTyre(FMath::Max(1, Left)));
		}
		RedState = 2;
		Phase = EPhase::Lights; LightsT = -10.f;   // 10 s d'interruption, puis la séquence des feux
		GoAt = 4.6f + FMath::FRand() * 0.8f;
		Say(TEXT("Muret : course suspendue. Pneus neufs, voiture réparée : nouveau départ arrêté aux feux."));
	}
	else if (RedState == 2 && Phase == EPhase::Race)
	{
		RedState = 0; GreenT = 3.f;
		Say(TEXT("Muret : c'est reparti ! Bon départ."));
	}
}

void AGPMGameMode::UpdateFlags(float Dt)
{
	bYellow = bBlueFlag = false;
	if (Phase != EPhase::Race || !Car || Cars.Num() == 0) { Yellows.Reset(); YellowAhead.Reset(); BlueT = 0.f; return; }
	const FGPMTrackData* T = Car->GetTrack();
	if (!T || RedState) return;
	const int32 N = T->WX.Num();

	// zones de drapeau jaune : de 300 m avant la voiture en difficulté à 30 m après, encore 6 s après l'incident
	for (FYellowZone& Zn : Yellows) Zn.Left -= Dt;
	for (AGPMCar* C : Cars)
	{
		if (!C || C == SC.Get() || C->PitPhase || C->bFinished || C->bParked || C->Lap < 1) continue;
		const bool bTrouble = C->bOut || C->SpinT > 0.f || (C->V < 12.f && Clock > 8.f);
		if (!bTrouble) continue;
		FYellowZone* Zn = Yellows.FindByPredicate([C](const FYellowZone& Z) { return Z.C.Get() == C; });
		if (!Zn) { Zn = &Yellows.AddDefaulted_GetRef(); Zn->C = C; }
		Zn->At = C->Idx; Zn->Left = 6.f;
	}
	Yellows.RemoveAll([](const FYellowZone& Z) { return Z.Left <= 0.f || !Z.C.IsValid() || Z.C->bParked; });
	auto InZone = [&](const AGPMCar* C)
	{
		for (const FYellowZone& Zn : Yellows)
		{
			if (Zn.C.Get() == C) continue;
			const int32 D = ((Zn.At - C->Idx) % N + N) % N;
			if (D <= 150 || D >= N - 15) return true;
		}
		return false;
	};
	const bool bNeutral = SCState != 0;
	for (AGPMCar* C : Cars)
	{
		if (!C || C == SC.Get()) continue;
		const bool bIn = !bNeutral && !C->PitPhase && InZone(C);
		if (C == Car.Get())
		{
			bYellow = bIn;
			if (bIn && !bYellowPrev)
			{
				// entrée dans la zone : les voitures juste devant ne doivent pas être doublées (sauf celle en difficulté)
				YellowAhead.Reset();
				for (AGPMCar* O : Cars)
				{
					if (!O || O == C || O->bOut || O->PitPhase || O == SC.Get()) continue;
					const float Ahead = O->Progress() - C->Progress();
					if (Ahead > 0.f && Ahead < 150.f && !Yellows.ContainsByPredicate([O](const FYellowZone& Z) { return Z.C.Get() == O; })) YellowAhead.Add(O);
				}
			}
			if (!bIn) YellowAhead.Reset();
			bYellowPrev = bIn;
		}
		else if (!bNeutral) C->SlowMul = bIn ? 0.9f : 1.f;   // les IA lèvent le pied dans la zone
	}
	if (bYellow && Car->bPlayerControlled)
		for (int32 k = YellowAhead.Num() - 1; k >= 0; --k)
		{
			AGPMCar* O = YellowAhead[k].Get();
			if (!O || O->bOut || O->PitPhase || O->SpinT > 0.f) { YellowAhead.RemoveAt(k); continue; }
			if (Car->Progress() - O->Progress() > 3.f)
			{
				Penalize(Car, 0, 5.f, FString::Printf(TEXT("Dépassement de %s sous drapeau jaune"), *O->DriverName), 5.f);
				YellowAhead.Reset();
				break;
			}
		}

	// drapeau bleu : une voiture qui a un tour d'avance arrive dans les 120 m derrière le joueur
	const AGPMCar* By = nullptr;
	if (!Car->bOut && !Car->PitPhase && !bNeutral)
		for (const AGPMCar* C : Cars)
		{
			if (!C || C == Car.Get() || C == SC.Get() || C->bOut || C->PitPhase) continue;
			const float Lead = C->Progress() - Car->Progress();
			if (Lead >= N - 60 && Lead < N - 2) { By = C; break; }
		}
	bBlueFlag = By != nullptr;
	if (By)
	{
		BlueBy = By->DriverName;
		BlueT += Dt;
		if (BlueWarn == 0) { BlueWarn = 1; Say(FString::Printf(TEXT("Muret : drapeau bleu, laisse passer %s, il te prend un tour."), *By->DriverName)); }
		if (BlueT > 20.f && Car->bPlayerControlled)
		{
			Penalize(Car, 0, 5.f, TEXT("Drapeaux bleus ignorés"), 3.f);
			BlueT = 0.f;
		}
	}
	else { BlueT = FMath::Max(0.f, BlueT - Dt * 2.f); if (BlueT <= 0.f) BlueWarn = 0; }
}

// Commande console : GPM.RF (drapeau rouge tout de suite)
static FAutoConsoleCommandWithWorldAndArgs GRedFlagCmd(
	TEXT("GPM.RF"),
	TEXT("Drapeau rouge : course suspendue puis nouveau départ."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (AGPMGameMode* GM = World ? World->GetAuthGameMode<AGPMGameMode>() : nullptr) GM->RedFlag(TEXT("Accident : barrières à réparer"), true);
	}));
