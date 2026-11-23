// Pré-saison de janvier : la saison commence le 1er janvier, deux mois et demi avant le premier Grand Prix.
//  1. révélation de la monoplace (scène à l'atelier, puis la présentation devant la presse : fenêtre à choix) ;
//  2. événements de l'intersaison (flash info, brèves) ;
//  3. essais hivernaux : trois journées de roulage seul sur le circuit de la première manche, chacune avec une mission.
//     Mission réussie : recherches en avance, moral en hausse. Ratée : moral en baisse. Casse moteur ou accident : voiture
//     abîmée, argent et jours perdus. Les ~60 jours d'hiver font avancer les recherches de toute façon (FinishPreseason).
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
#include "GPMSeason.h"
#include "GPMTrack.h"
#include "GPMDataSubsystem.h"
#include "GPMDebris.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FString Fmt(float T) { return FString::Printf(TEXT("%d:%06.3f"), int32(T) / 60, FMath::Fmod(T, 60.f)); }
	const float Gain[3] = { 6.f, 6.f, 8.f };
}

// calendrier de l'hiver : 1er janvier, 20 janvier (présentation), 20 février (essais), premier GP (2e dimanche de mars)
FDateTime AGPMGameMode::PreDate(int32 Stage, int32 Year)
{
	if (Stage <= 0) return FDateTime(Year, 1, 1);
	if (Stage == 1) return FDateTime(Year, 1, 20);
	if (Stage == 2) return FDateTime(Year, 2, 20);
	const FDateTime D(Year, 3, 8);
	const int32 JsDay = (int32(D.GetDayOfWeek()) + 1) % 7;
	return D + FTimespan::FromDays(double((7 - JsDay) % 7));
}

// l'usine avance d'une étape de l'hiver à la suivante : recherches, chantier, chefs libres ; le bilan arrive par mail
void AGPMGameMode::WinterAdvance(int32 From, int32 To)
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || !Career || !Factory) return;
	const int32 Days = FMath::Max(0, int32((PreDate(To, G->Year) - PreDate(From, G->Year)).GetTotalDays()));
	if (Days <= 0) return;
	G->News.Reset();
	Factory->Tick(Days);
	const FString Built = Factory->TickInfra(Days);
	Factory->RotateStaff();
	FString Body = FString::Printf(TEXT("%d jours sont passés à l'usine."), Days);
	for (const FString& N : G->News) Body += TEXT("\n• ") + N;
	if (!Built.IsEmpty()) Body += TEXT("\n• ") + Built;
	if (G->News.Num() == 0 && Built.IsEmpty()) Body += TEXT("\nLes recherches en cours avancent : lance-en de nouvelles au QG si des ateliers sont libres.");
	static const TCHAR* const Names[4] = { TEXT("1er janvier"), TEXT("20 janvier"), TEXT("20 février"), TEXT("premier Grand Prix") };
	Career->SendMail(TEXT("usine"), TEXT("Bureau technique"), FString::Printf(TEXT("Usine : du %s au %s"), Names[FMath::Clamp(From, 0, 3)], Names[FMath::Clamp(To, 0, 3)]), Body);
	LastLines = G->News;
}

int32 AGPMGameMode::PreStage() const
{
	const UGPMSeasonSubsystem* S = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const UGPMSaveGame* G = S ? S->State() : nullptr;
	return G && G->bCreated && bCareer ? G->PreStage : 3;
}

// ---------- 1. révélation ----------
void AGPMGameMode::StartReveal()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || !Career || G->PreStage != 0) return;
	WinterAdvance(0, 1);   // du 1er au 20 janvier
	G->PreStage = 1;
	Season->Save();
	Career->PreEvents(true);   // la présentation devant la presse suit la scène
	CineTitle = TEXT("LA NOUVELLE MONOPLACE");
	CineSub = FString::Printf(TEXT("%s · saison %d"), *G->TeamName, G->Year);
	bReveal = true; bRevealDialog = true;
	EndCine();
	StartCine(ECine::Welcome);
}

// ---------- 2. événements ----------
void AGPMGameMode::StartPreEvents()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || !Career || G->PreStage != 1) return;
	WinterAdvance(1, 2);   // du 20 janvier au 20 février
	G->PreStage = 2;
	Career->PreEvents(false);
	Season->Save();
	if (Career->Current()) { bNegoFromMercato = false; Phase = EPhase::Press; }
}

// ---------- 3. essais ----------
void AGPMGameMode::StartTest()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const UGPMSaveGame* G = Season ? Season->State() : nullptr;
	const FGPMTrackData* T = CurTrack();
	if (!G || !T || !Car || G->PreStage != 2 || G->TestDay >= 3) return;
	const int32 Day = G->TestDay;
	EndCine();
	bTesting = true; TestEnded = false; bQInvalid = false;
	Phase = EPhase::Quali;
	BaseUps(); EnsureSetup(); ApplySetup();
	if (SC) { for (AGPMCar* C : Cars) C->Field.Remove(SC); SC->Destroy(); SC = nullptr; }
	for (AGPMCar* C : Cars) if (C != Car) Park(C, true);
	Park(Car, false);
	Car->bPlayerControlled = !bTestAuto;
	const int32 N = T->WX.Num(), I = (N - 150) % N;
	Car->PlaceOnTrack(T, 0);
	Car->Idx = I; Car->X = T->WX[I]; Car->Z = T->WZ[I]; Car->Y = T->WY[I]; Car->Heading = T->H[I];
	Car->Lat = Car->Lane = 0.f; Car->V = 45.f; Car->bHold = false;
	Car->Field = { Car.Get() };
	bRainStart = false; WxChangeAt = -1.f; ResetWeather();   // essais par temps sec
	if (Debris) Debris->Clear();
	MisType = TestType(Day);
	TestWanted = MisType == 1 ? 4 : 3;
	Car->Tyre = TCHAR('M'); Car->Wear = 0.f; Car->Fuel = FMath::Min(100.f, 8.f + NomFuel() * (TestWanted + 0.6f)); Car->Mass = 1.f + Car->Fuel / 750.f;
	Car->GripMul = AGPMCar::WetGrip(Car->Tyre, 0.f);
	Car->BestLap = 0.f;
	Order = { Car.Get() };
	QTimes.Reset(); QOrder.Reset();
	QCum.Reset();
	TestLaps.Reset(); TestClean.Reset(); TestLapSeen = 0; TestDirty = false; TestHurt = 0.f;
	// référence : le record de l'écurie sur ce circuit (+1 %), sinon le meilleur temps estimé des rivaux
	{
		float Pole = 0.f;
		for (AGPMCar* C : Cars) if (C != Car) { const float Lt = LapTimeFor(T, C->Skill * C->CornerK, C->Perf) * 0.985f; if (Pole <= 0.f || Lt < Pole) Pole = Lt; }
		const float* Rec = G->LapRec.Find(T->Name);
		TestRef = Rec && *Rec > 10.f ? *Rec * 1.01f : Pole > 0.f ? Pole : BaseLap();
	}
	MisReset();
	if (MisType == 3) BuildGates();
	// casse moteur possible (le risque du motoriste, un peu plus fort à l'entraînement)
	TestEngineAt = FMath::FRand() < EngineRisk(Car) * 1.3f ? 0.3f + 0.6f * FMath::FRand() : -1.f;
	RadioT = 0.f;
	Say(FString::Printf(TEXT("Essais, jour %d : %s. Objectif : %s."), Day + 1, *MissionName(MisType), *MissionGoal(MisType)));
	UE_LOG(LogGPM, Log, TEXT("Essais hivernaux jour %d : référence %s, %d tours"), Day + 1, *Fmt(TestRef), TestWanted);
}

void AGPMGameMode::TestLines(TArray<FString>& Out) const
{
	if (bPractice)
	{
		static const TCHAR* const Goal[3] = { TEXT("Passe dans les portes : elles marquent la trajectoire idéale"), TEXT("Un tour rapide en usant les pneus le moins possible"), TEXT("Deux tours rapides avec le moins d'essence possible") };
		Out.Add(FString::Printf(TEXT("ESSAIS LIBRES · %s"), PracticeName(PracticeProg)));
		Out.Add(Goal[FMath::Clamp(PracticeProg, 0, 2)]);
		Out.Add(MisStatus(PracticeProg == 0 ? 3 : PracticeProg == 1 ? 4 : 5));
		return;
	}
	const UGPMSeasonSubsystem* S = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const UGPMSaveGame* G = S ? S->State() : nullptr;
	if (!G || !bTesting) return;
	const int32 Day = FMath::Clamp(G->TestDay, 0, 2);
	Out.Add(FString::Printf(TEXT("ESSAIS HIVERNAUX · JOUR %d / 3 · %s"), Day + 1, *MissionName(MisType).ToUpper()));
	Out.Add(MissionGoal(MisType).Left(1).ToUpper() + MissionGoal(MisType).Mid(1));
	int32 Clean = 0;
	float Best = 0.f;
	for (int32 k = 0; k < TestLaps.Num(); ++k) if (TestClean[k]) { ++Clean; Best = Best <= 0.f ? TestLaps[k] : FMath::Min(Best, TestLaps[k]); }
	const int32 Lap = Car ? FMath::Clamp(Car->Lap, 1, TestWanted) : 1;
	Out.Add(FString::Printf(TEXT("Tour %d/%d · propres %d · meilleur %s · réf. %s%s%s"), Lap, TestWanted, Clean, Best > 0.f ? *Fmt(Best) : TEXT("--:--.---"), *Fmt(TestRef),
		bQInvalid || TestDirty ? TEXT(" · TOUR SALE") : TEXT(""), MisType >= 3 ? *(TEXT(" · ") + MisStatus(MisType)) : TEXT("")));
}

void AGPMGameMode::TestTick(float Dt)
{
	RadioT -= Dt;
	const FGPMTrackData* T = Car ? Car->GetTrack() : nullptr;
	if (!T || TestEnded) return;
	const int32 N = T->WX.Num();
	if (Car->Lap >= 1 && ((Car->bOffTrack && Car->Ground != 3) || Car->HitWall > 0.3f)) TestDirty = true;
	WearAndFuel(Car, Dt);
	GateTick();
	if (Car->Lap != TestLapSeen)
	{
		MisLap();
		if (Car->Lap >= 2 && Car->LastLap > 0.f)
		{
			const bool bClean = !TestDirty && !bQInvalid;
			TestLaps.Add(Car->LastLap); TestClean.Add(bClean);
			FString Extra;
			if (MisType == 3 && LapGate.Num()) Extra = FString::Printf(TEXT(", portes %.0f %%"), LapGate.Last() * 100.f);
			else if (MisType == 4 && LapWear.Num()) Extra = FString::Printf(TEXT(", usure %.1f %% (normale %.1f %%)"), LapWear.Last() * 100.f, NomWear() * 100.f);
			else if (MisType == 5 && LapFuel.Num()) Extra = FString::Printf(TEXT(", essence %.1f kg (normale %.1f)"), LapFuel.Last(), NomFuel());
			Say(FString::Printf(TEXT("Tour %d : %s%s%s"), TestLaps.Num(), *Fmt(Car->LastLap), *Extra, bClean ? TEXT("") : TEXT(" (tour sale, ne compte pas)")));
		}
		TestLapSeen = Car->Lap; TestDirty = false; bQInvalid = false;
	}
	// chocs : trop abîmée, la voiture rentre au stand
	TestHurt += Car->DmgNew + Car->DmgNewR; Car->DmgNew = Car->DmgNewR = 0.f;
	if (TestHurt >= 0.2f) { EndTest(TEXT("accident")); return; }
	const float Frac = ((Car->Lap - 1) + float(Car->Idx) / FMath::Max(1, N)) / FMath::Max(1, TestWanted);
	if (TestEngineAt >= 0.f && Car->Lap >= 1 && Frac >= TestEngineAt) { EndTest(TEXT("moteur")); return; }
	if (Car->Lap >= TestWanted + 1) { EndTest(TEXT("fin")); return; }
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (PC->WasInputKeyJustPressed(EKeys::Escape)) EndTest(TEXT("abandon"));
}

void AGPMGameMode::EndTest(const TCHAR* Why)
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || !Career || !Factory || TestEnded) return;
	TestEnded = true;
	const int32 Day = FMath::Clamp(G->TestDay, 0, 2);
	const FString Reason = Why;
	// la mission, jugée sur les tours propres
	TArray<float> Clean;
	for (int32 k = 0; k < TestLaps.Num(); ++k) if (TestClean[k]) Clean.Add(TestLaps[k]);
	Clean.Sort();
	bool bOk = false;
	float Best = Clean.Num() ? Clean[0] : 0.f, Ratio = TestRef > 0.f && Best > 0.f ? Best / TestRef : 0.f;
	FString Detail;
	// tours propres assez rapides (à Lim près de la référence) : leurs mesures d'usure, d'essence et de portes
	auto Fast = [this](float Lim, TArray<int32>& Ks) { for (int32 k = 0; k < TestLaps.Num(); ++k) if (TestClean[k] && TestLaps[k] <= TestRef * Lim) Ks.Add(k); };
	if (MisType == 0) bOk = Best > 0.f && Ratio <= 1.03f;
	else if (MisType == 1) bOk = Clean.Num() >= 3 && (Clean[2] - Clean[0]) / Clean[0] <= 0.015f;
	else if (MisType == 2) bOk = Best > 0.f && Ratio <= 1.0f;
	else if (MisType == 3)
	{
		TArray<int32> Ks; Fast(1.04f, Ks);
		float Top = 0.f;
		for (int32 k = 0; k < LapGate.Num(); ++k) Top = FMath::Max(Top, LapGate[k]);
		for (int32 k : Ks) if (LapGate.IsValidIndex(k) && LapGate[k] >= 0.8f) bOk = true;
		Detail = FString::Printf(TEXT(", meilleur passage %.0f %% des portes"), Top * 100.f);
	}
	else if (MisType == 4)
	{
		TArray<int32> Ks; Fast(1.035f, Ks);
		float Sum = 0.f;
		for (const float W : LapWear) Sum += W;
		const float Avg = LapWear.Num() ? Sum / LapWear.Num() : 1.f;
		bOk = Ks.Num() >= 2 && Avg <= 0.75f * NomWear();
		Detail = FString::Printf(TEXT(", usure %.1f %% par tour (normale %.1f %%)"), Avg * 100.f, NomWear() * 100.f);
	}
	else
	{
		TArray<int32> Ks; Fast(1.04f, Ks);
		float Sum = 0.f;
		for (int32 k : Ks) if (LapFuel.IsValidIndex(k)) Sum += LapFuel[k];
		const float Avg = Ks.Num() ? Sum / Ks.Num() : 99.f;
		bOk = Ks.Num() >= 2 && Avg <= 0.8f * NomFuel();
		if (Ks.Num()) Detail = FString::Printf(TEXT(", %.1f kg d'essence par tour (normale %.1f)"), Avg, NomFuel());
	}
	const bool bEngine = Reason == TEXT("moteur"), bCrash = Reason == TEXT("accident"), bQuit = Reason == TEXT("abandon");
	FString Line = FString::Printf(TEXT("Jour %d · %s : "), Day + 1, *MissionName(MisType));
	int32 Res = 0;
	float Days = 0.f;
	// mission brillante (bien au-delà de l'objectif) : 3 jours de plus
	bool bStar = false;
	if (bOk)
	{
		float TopG = 0.f, SumW = 0.f;
		for (const float Gv : LapGate) TopG = FMath::Max(TopG, Gv);
		for (const float Wv : LapWear) SumW += Wv;
		bStar = (MisType == 3 && TopG >= 0.95f) || (MisType == 4 && LapWear.Num() && SumW / LapWear.Num() <= 0.6f * NomWear())
			|| ((MisType == 0 || MisType == 2) && Ratio <= 0.99f) || (MisType == 1 && Clean.Num() >= 3 && (Clean[2] - Clean[0]) / Clean[0] <= 0.006f)
			|| (MisType == 5 && Detail.Contains(TEXT("kg")) && LapFuel.Num() && FMath::Min(LapFuel) <= 0.7f * NomFuel());
	}
	if (bOk) { Days = Gain[Day] + (bStar ? 3.f : 0.f); Res = 1; G->Moral = UGPMCareerSubsystem::Bump(G->Moral, 4.f); G->Rep = UGPMCareerSubsystem::Bump(G->Rep, 1.f); Career->ShiftDev(-Days); Line += FString::Printf(TEXT("%s, recherches en avance de %.0f jours, moral en hausse"), bStar ? TEXT("MISSION BRILLANTE") : TEXT("mission réussie"), Days); }
	else if (bQuit) Line += TEXT("séance arrêtée, mission non remplie");
	else { G->Moral = FMath::Clamp(G->Moral - 2, 0, 100); Line += TEXT("mission ratée, moral en baisse"); }
	if (bEngine || bCrash)
	{
		Res = -1;
		Factory->AddMoney(bEngine ? -1.5f : -1.f);
		G->Moral = FMath::Clamp(G->Moral - (bEngine ? 4 : 3), 0, 100);
		Career->ShiftDev(2.f);
		Days -= 2.f;
		Line += bEngine ? TEXT(" · CASSE MOTEUR : 1,5 M€ de réparations et 2 jours perdus à l'usine") : TEXT(" · ACCIDENT : 1,0 M€ de réparations et 2 jours perdus à l'usine");
	}
	if (Best > 0.f) Line += FString::Printf(TEXT(" (meilleur tour propre %s, %+.1f %% de la référence%s)"), *Fmt(Best), (Ratio - 1.f) * 100.f, *Detail);
	G->TestDays = FMath::Max(0.f, G->TestDays + Days);
	G->TestRes.Add(Res); G->TestReport.Add(Line);
	G->TestDay++;
	LastLines = { Line };
	UE_LOG(LogGPM, Log, TEXT("Essais : %s"), *Line);
	// fiche de données : chaque tour avec son temps, l'usure, l'essence et les portes
	FString Data;
	for (int32 k = 0; k < TestLaps.Num(); ++k)
	{
		Data += FString::Printf(TEXT("\n• Tour %d : %s%s"), k + 1, *Fmt(TestLaps[k]), TestClean.IsValidIndex(k) && TestClean[k] ? TEXT("") : TEXT(" (sale)"));
		if (LapWear.IsValidIndex(k)) Data += FString::Printf(TEXT(" · usure %.1f %%"), LapWear[k] * 100.f);
		if (LapFuel.IsValidIndex(k)) Data += FString::Printf(TEXT(" · essence %.1f kg"), LapFuel[k]);
		if (MisType == 3 && LapGate.IsValidIndex(k)) Data += FString::Printf(TEXT(" · portes %.0f %%"), LapGate[k] * 100.f);
	}
	if (!Data.IsEmpty()) Data = FString::Printf(TEXT("\n\nDonnées (normale : usure %.1f %%, essence %.1f kg par tour) :"), NomWear() * 100.f, NomFuel()) + Data;
	Career->SendMail(TEXT("equipe"), TEXT("Ingénieur de course"), FString::Printf(TEXT("Essais hivernaux, jour %d"), Day + 1), Line + TEXT(".") + Data);
	ReturnToSeason();
	if (G->TestDay >= 3) FinishPreseason();
	else Season->Save();
}

void AGPMGameMode::ReturnToSeason()
{
	bTesting = false; bQInvalid = false;
	ClearGates();
	for (AGPMCar* C : Cars) if (C) Park(C, false);
	QTimes.Reset(); QOrder.Reset();
	StageGrid();
	Forecast();   // météo du premier Grand Prix, tirée à nouveau
	Phase = EPhase::Season;
	if (Car) Car->bPlayerControlled = false;
}

// ---------- fin ----------
void AGPMGameMode::FinishPreseason()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || !Career || !Factory || G->PreStage >= 3) return;
	const int32 From = G->PreStage;
	WinterAdvance(From, 3);   // jusqu'au premier Grand Prix (tout l'hiver d'un coup si la pré-saison est passée)
	G->PreStage = 3;
	FString Body = FString::Printf(TEXT("L'hiver est terminé : la voiture a progressé depuis le 1er janvier (voir les rapports de l'usine).\n"));
	if (G->TestReport.Num())
	{
		Body += TEXT("\nEssais hivernaux :");
		for (const FString& L : G->TestReport) Body += TEXT("\n• ") + L;
		Body += FString::Printf(TEXT("\n\nBilan : %+.0f jours sur les recherches en cours, moral %d."), G->TestDays, G->Moral);
	}
	else Body += TEXT("\nTu n'as pas fait les essais hivernaux : aucun bonus de développement.");
	Career->SendMail(TEXT("usine"), TEXT("Bureau technique"), TEXT("Bilan de la pré-saison"), Body + TEXT("\n\nLe premier Grand Prix approche."));
	Season->Save();
}

void AGPMGameMode::SkipPreseason()
{
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMSaveGame* G = Season ? Season->State() : nullptr;
	if (!G || G->PreStage >= 3) return;
	if (bTesting) { TestEnded = true; ReturnToSeason(); }
	FinishPreseason();
}
