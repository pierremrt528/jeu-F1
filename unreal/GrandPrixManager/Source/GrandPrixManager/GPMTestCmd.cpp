// Commandes console de test des grandes refontes du règlement (captures d'écran sans toucher la souris) :
// GPM.Refonte (le règlement de la prochaine ère est publié), GPM.Intersaison (passe à la saison suivante),
// GPM.Capture <nom>, GPM.Test (parcours complet avec captures dans Saved/Captures).
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
#include "GPMCar.h"
#include "GPMTypes.h"
#include "GPMHUD.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace GPMTest
{
	void Capture(const FString& Name)
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogGPM, Display, TEXT("Capture : %s"), *File);
	}

	void Publish(UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UGPMFactorySubsystem* F = GI ? GI->GetSubsystem<UGPMFactorySubsystem>() : nullptr;
		UGPMCareerSubsystem* C = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
		if (!F) return;
		F->ForceReformSoon();
		FString Subj, Body;
		if (C && F->EraPublished(Subj, Body)) C->SendMail(TEXT("reglement"), TEXT("Instance dirigeante du championnat"), Subj, Body);
	}

	void NextSeason(UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UGPMFactorySubsystem* F = GI ? GI->GetSubsystem<UGPMFactorySubsystem>() : nullptr;
		UGPMSeasonSubsystem* S = GI ? GI->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
		UGPMCareerSubsystem* C = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
		if (!F || !S) return;
		FString RegSubj, RegBody, PubSubj, PubBody;
		F->Intersaison(S->Year() + 1, RegSubj, RegBody);
		S->NewSeason(true);
		F->EraPublished(PubSubj, PubBody);
		if (C && !RegSubj.IsEmpty()) C->SendMail(TEXT("reglement"), TEXT("Instance dirigeante du championnat"), RegSubj, RegBody);
		if (C && !PubSubj.IsEmpty()) C->SendMail(TEXT("reglement"), TEXT("Instance dirigeante du championnat"), PubSubj, PubBody);
		S->Save();
	}

	// pièces de test : les premières pièces disponibles de chaque arbre (voiture en cours ou suivante)
	void GiveParts(UWorld* World, bool bFut, int32 PerTree)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		UGPMFactorySubsystem* F = GI ? GI->GetSubsystem<UGPMFactorySubsystem>() : nullptr;
		UGPMSeasonSubsystem* S = GI ? GI->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
		UGPMSaveGame* G = S ? S->State() : nullptr;
		if (!F || !G) return;
		for (const FGPMTree& T : F->Trees(bFut))
			for (int32 k = 0; k < PerTree; ++k)
				for (const FGPMPart& P : T.Parts)
					if (F->StateOf(P) != UGPMFactorySubsystem::EState::Owned && F->StateOf(P) != UGPMFactorySubsystem::EState::Locked && F->StateOf(P) != UGPMFactorySubsystem::EState::Running)
					{ (bFut ? G->FutOwned : G->Owned).Add(P.Id); break; }
		G->Budget = FMath::Max(G->Budget, 60.f);
		S->Save();
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRefonteCmd(TEXT("GPM.Refonte"), TEXT("Publie le règlement de la prochaine ère (refonte l'an prochain)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { GPMTest::Publish(World); }));

static FAutoConsoleCommandWithWorldAndArgs GInterCmd(TEXT("GPM.Intersaison"), TEXT("Passe à la saison suivante (refonte ou règlement annuel)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { GPMTest::NextSeason(World); }));

static FAutoConsoleCommandWithWorldAndArgs GCaptureCmd(TEXT("GPM.Capture"), TEXT("Capture d'écran dans Saved/Captures : GPM.Capture <nom>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*) { GPMTest::Capture(Args.Num() ? Args[0] : FString(TEXT("Capture"))); }));

static FAutoConsoleCommandWithWorldAndArgs GTestCmd(TEXT("GPM.Test"), TEXT("Parcours de test du règlement avec captures (Saved/Captures/T_*.png)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		TWeakObjectPtr<UWorld> W = World;
		struct FStep { float At; TFunction<void(UWorld*, AGPMGameMode*)> Do; };
		TArray<FStep> Steps = {
			{ 8.f, [](UWorld* Wd, AGPMGameMode* GM)
				{
					UGPMCareerSubsystem* C = Wd->GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
					if (C && C->IsCreated()) GM->ContinueCareer(); else GM->FinishSetup();
					GM->UiRequest = 0;
				} },
			{ 16.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T01_Saison")); } },
			{ 17.f, [](UWorld*, AGPMGameMode* GM) { GM->Phase = AGPMGameMode::EPhase::Factory; GM->UiRequest = 11; } },
			{ 21.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T02_Usine")); } },
			{ 22.f, [](UWorld* Wd, AGPMGameMode* GM) { GPMTest::Publish(Wd); GPMTest::GiveParts(Wd, true, 2); GM->Phase = AGPMGameMode::EPhase::Season; GM->UiRequest = 0; } },
			{ 26.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T03_Saison_Annonce")); } },
			{ 27.f, [](UWorld*, AGPMGameMode* GM) { GM->UiRequest = 3; } },
			{ 30.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T04_BoiteMail")); } },
			{ 31.f, [](UWorld*, AGPMGameMode* GM) { GM->Phase = AGPMGameMode::EPhase::Factory; GM->UiRequest = 10; } },
			{ 36.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T05_Usine_VoitureSuivante")); } },
			{ 37.f, [](UWorld* Wd, AGPMGameMode* GM) { GPMTest::NextSeason(Wd); GM->Phase = AGPMGameMode::EPhase::Season; GM->UiRequest = 0; } },
			{ 42.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T06_Saison_NouvelleEre")); } },
			{ 43.f, [](UWorld*, AGPMGameMode* GM) { GM->Phase = AGPMGameMode::EPhase::Factory; GM->UiRequest = 11; } },
			{ 48.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T07_Usine_NouvelleEre")); } },
			{ 49.f, [](UWorld*, AGPMGameMode* GM) { GM->Phase = AGPMGameMode::EPhase::Season; GM->UiRequest = 3; } },
			{ 53.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("T08_BoiteMail_NouvelleEre")); } },
		};
		for (const FStep& St : Steps)
		{
			FTimerHandle H;
			TFunction<void(UWorld*, AGPMGameMode*)> Do = St.Do;
			World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, Do]()
			{
				if (!W.IsValid()) return;
				if (AGPMGameMode* GM = W->GetAuthGameMode<AGPMGameMode>()) Do(W.Get(), GM);
			}), St.At, false);
		}
		UE_LOG(LogGPM, Display, TEXT("GPM.Test : parcours lancé"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GStandCmd(TEXT("GPM.TestStand"), TEXT("Course solo, voiture placée avant l'entrée des stands, arrêt dans le carré avec captures."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (!World) return;
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0; float T = 0.f; };
		TSharedPtr<FState> St = MakeShared<FState>();
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) { GM->bSoloQuali = false; GM->StartSolo(); }
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* C = GM ? GM->PlayerCar() : nullptr;
			if (!C || !C->GetTrack()) return;
			St->T += 0.25f;
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Prerace) { GM->Go(false); return; }
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Intro) { GM->EndCine(); return; }
			const FGPMTrackData& T = *C->GetTrack();
			const int32 N = T.WX.Num();
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Race && GM->Clock > 2.f)
			{
				// avant l'entrée des stands, à droite, déjà au 1er tour
				const int32 K = T.PitE0 - 6, i = ((K % N) + N) % N;
				C->Idx = i; C->Lat = C->Lane = T.PitHalf + 2.5f; C->V = 28.f; C->Lap = 1;
				C->X = T.WX[i] - T.TZ[i] * C->Lat; C->Z = T.WZ[i] + T.TX[i] * C->Lat; C->Y = T.WY[i]; C->Heading = T.H[i];
				C->bTestDrive = true;
				St->Step = 1; St->T = 0.f;
				UE_LOG(LogGPM, Display, TEXT("TestStand : voiture placée (K=%d, mur droit %.1f m)"), K, T.WallRight.IsValidIndex(i) ? T.WallRight[i] : -1.f);
			}
			else if (St->Step == 1 && C->bPitManual) { St->Step = 2; St->T = 0.f; UE_LOG(LogGPM, Display, TEXT("TestStand : entrée dans la voie")); }
			else if (St->Step == 1 && St->T > 25.f) { UE_LOG(LogGPM, Display, TEXT("TestStand : pas entré (Lat %.1f, K %d)"), C->Lat, T.PitK(C->Idx)); GPMTest::Capture(TEXT("S0_PasEntre")); St->Step = 9; }
			else if (St->Step == 2 && St->T >= 2.f) { GPMTest::Capture(TEXT("S1_Voie")); St->Step = 3; }
			else if (St->Step == 3 && C->PitPhase == 2 && C->PitStopT < 1e5f) { St->Step = 4; St->T = 0.f; UE_LOG(LogGPM, Display, TEXT("TestStand : arrêt dans le carré (%.1f s)"), C->PitStopT); }
			else if (St->Step == 4 && St->T >= 0.5f) { GPMTest::Capture(TEXT("S2_Arret")); St->Step = 5; }
			else if (St->Step == 5 && C->PitPhase == 3) { St->Step = 6; St->T = 0.f; }
			else if (St->Step == 6 && St->T >= 1.5f) { GPMTest::Capture(TEXT("S3_Sortie")); St->Step = 7; }
			else if (St->Step == 7 && C->PitPhase == 0) { UE_LOG(LogGPM, Display, TEXT("TestStand : ressorti, %d arrêt(s), pneus %s"), C->Stops, *FString::Chr(C->Tyre)); C->bTestDrive = false; St->Step = 8; }
			else if (St->Step == 3 && St->T > 40.f) { UE_LOG(LogGPM, Display, TEXT("TestStand : pas d'arrêt (K %.1f, O %.2f, V %.1f)"), C->PitKf, C->PitO, C->V); GPMTest::Capture(TEXT("S9_Bloque")); St->Step = 9; }
		}), 0.25f, true);
	}));

static FAutoConsoleCommandWithWorldAndArgs GDecorCmd(TEXT("GPM.TestDecor"), TEXT("Course solo sur le circuit n° <idx>, captures du décor à plusieurs endroits."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 Idx = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		const int32 Extra = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1;
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0; float T = 0.f; };
		TSharedPtr<FState> St = MakeShared<FState>();
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, Idx]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) { GM->bSoloQuali = false; GM->SoloIdx = Idx; GM->StartSolo(); }
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St, Idx, Extra]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* C = GM ? GM->PlayerCar() : nullptr;
			if (!C || !C->GetTrack()) return;
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Prerace) { GM->Go(false); return; }
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Intro) { GM->EndCine(); return; }
			if (GM->Phase != AGPMGameMode::EPhase::Race || GM->Clock < 2.f) return;
			St->T += 0.25f;
			const FGPMTrackData& T = *C->GetTrack();
			const int32 N = T.WX.Num();
			const int32 Spots[4] = { Extra >= 0 ? Extra : T.PitE0 - 20, T.PitE1 + 10, N / 3, (N * 2) / 3 };
			const int32 Spot = St->Step / 2;
			if (Spot >= 4) return;
			if (St->Step % 2 == 0)
			{
				const int32 i = ((Spots[Spot] % N) + N) % N;
				C->Idx = i; C->Lat = C->Lane = 0.f; C->V = 0.f; C->Lap = 1;
				C->X = T.WX[i]; C->Z = T.WZ[i]; C->Y = T.WY[i]; C->Heading = T.H[i];
				C->bHold = true;
				St->Step++; St->T = 0.f;
			}
			else if (St->T >= 1.5f) { GPMTest::Capture(FString::Printf(TEXT("D%d_%d"), Idx, Spot)); St->Step++; }
		}), 0.25f, true);
	}));

// HUD de course façon télé : course solo en pilote automatique, captures au départ, au 1er et au 2e tour, en tactile et en pause ;
// « GPM.TestHud q » : qualifications (tour de lancement puis tour lancé)
static FAutoConsoleCommandWithWorldAndArgs GHudCmd(TEXT("GPM.TestHud"), TEXT("Course solo en pilote automatique avec captures du HUD (Saved/Captures/H*.png) ; « q » : qualifications."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const bool bQ = Args.Num() > 0 && Args[0] == TEXT("q");
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0; float T = 0.f; };
		TSharedPtr<FState> St = MakeShared<FState>();
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, bQ]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) { GM->bSoloQuali = bQ; GM->SoloLaps = 5; GM->StartSolo(); }
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St, bQ]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* C = GM ? GM->PlayerCar() : nullptr;
			if (!C || !C->GetTrack()) return;
			if (GM->Phase == AGPMGameMode::EPhase::Tuning) { GM->StartQuali(); return; }
			if (GM->Phase == AGPMGameMode::EPhase::Prerace) { GM->Go(false); return; }
			if (GM->Phase == AGPMGameMode::EPhase::Intro) { GM->EndCine(); return; }
			const bool bOn = GM->Phase == AGPMGameMode::EPhase::Quali || GM->Phase == AGPMGameMode::EPhase::Lights || GM->Phase == AGPMGameMode::EPhase::Race;
			if (!bOn) return;
			C->bPlayerControlled = false;   // pilote automatique
			St->T += 0.25f;
			const int32 N = C->GetTrack()->WX.Num();
			APlayerController* PC = W->GetFirstPlayerController();
			AGPMHUD* Hud = PC ? Cast<AGPMHUD>(PC->GetHUD()) : nullptr;
			auto Shot = [St](const TCHAR* Name) { GPMTest::Capture(Name); St->Step++; St->T = 0.f; };
			if (bQ)
			{
				if (St->Step == 0 && St->T >= 4.f) Shot(TEXT("HQ1_Lancement"));
				else if (St->Step == 1 && C->Lap >= 1 && C->Idx > N / 2) Shot(TEXT("HQ2_TourLance"));
				else if (St->Step == 2 && C->Lap >= 1 && C->Idx > N - 25) Shot(TEXT("HQ3_FinDuTour"));
				return;
			}
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Lights && GM->LightsOn() >= 3) Shot(TEXT("H1_Feux"));
			else if (St->Step == 1 && GM->Phase == AGPMGameMode::EPhase::Race && GM->Clock > 6.f) Shot(TEXT("H2_Depart"));
			else if (St->Step == 2 && C->Lap >= 2 && C->Idx > N / 2)
			{
				UE_LOG(LogGPM, Display, TEXT("TestHud : P%d, tour %d, meilleur %.3f, dernier %.3f"), GM->PositionOf(C), C->Lap, C->BestLap, C->LastLap);
				Shot(TEXT("H3_Tour2"));
			}
			else if (St->Step == 3 && C->Lap >= 3 && C->Idx > N / 3) { if (Hud) Hud->bShowTouch = true; St->Step++; St->T = 0.f; }
			else if (St->Step == 4 && St->T >= 0.5f) Shot(TEXT("H4_Tactile"));
			else if (St->Step == 5 && St->T >= 0.5f)
			{
				if (Hud) Hud->bShowTouch = false;
				UGameplayStatics::SetGamePaused(W.Get(), true);
				Shot(TEXT("H5_Pause"));
			}
		}), 0.25f, true);
	}));

static FAutoConsoleCommandWithWorldAndArgs GMercatoCmd(TEXT("GPM.TestMercato"), TEXT("Parcours du mercato avec captures (Saved/Captures/M_*.png) : négociation, clause, prolongation, offre d'un rival, intersaison."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (!World) return;
		TWeakObjectPtr<UWorld> W = World;
		auto CS = [](UWorld* Wd) { return Wd->GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>(); };
		auto Pick = [](UGPMCareerSubsystem* C, int32 Act) { if (FGPMDialog* Dl = C->Current()) for (int32 k = 0; k < Dl->Choices.Num(); ++k) if (Dl->Choices[k].Action == Act) { C->Choose(k); return; } };
		auto Log = [](UGPMCareerSubsystem* C, const TCHAR* Tag)
		{
			if (FGPMDialog* Dl = C->Current()) UE_LOG(LogGPM, Display, TEXT("TestMercato %s : %s | %s | %s"), Tag, *Dl->Kicker, *Dl->Text, *Dl->Result);
			if (const UGPMSaveGame* G = C->Save()) UE_LOG(LogGPM, Display, TEXT("TestMercato %s : équipier %s, %d saison(s), %.1f M€, clause %.1f, budget %.1f"), Tag, *G->MateName, G->MateYears, G->MateSal, G->MateClause, G->Budget);
		};
		struct FStep { float At; TFunction<void(UWorld*, AGPMGameMode*)> Do; };
		TArray<FStep> Steps = {
			{ 8.f, [CS](UWorld* Wd, AGPMGameMode* GM) { if (CS(Wd)->IsCreated()) GM->ContinueCareer(); else GM->FinishSetup(); } },
			{ 14.f, [](UWorld*, AGPMGameMode* GM) { GM->OpenMercato(); } },
			{ 18.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M01_Mercato")); } },
			{ 19.f, [CS, Log](UWorld* Wd, AGPMGameMode* GM)
				{
					UGPMCareerSubsystem* C = CS(Wd);
					if (C->Save()) C->Save()->Budget = FMath::Max(C->Save()->Budget, 80.f);
					for (const FGPMMarketDriver& D : C->MarketList()) if (D.Seat >= 0 && D.Rating() < 85.f) { C->OpenNego(D); break; }
					GM->bNegoFromMercato = true; GM->Phase = AGPMGameMode::EPhase::Press;
					Log(C, TEXT("nego"));
				} },
			{ 23.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M02_Negociation")); } },
			{ 24.f, [CS, Pick, Log](UWorld* Wd, AGPMGameMode*) { Pick(CS(Wd), 6); Log(CS(Wd), TEXT("clause")); } },
			{ 27.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M03_Clause")); } },
			{ 28.f, [CS, Log](UWorld* Wd, AGPMGameMode* GM)
				{
					UGPMCareerSubsystem* C = CS(Wd);
					C->Queue.Reset(); GM->EndCine();
					if (UGPMSaveGame* G = C->Save()) G->MateYears = FMath::Min(G->MateYears, 1);
					C->OpenExtend(); GM->bNegoFromMercato = true; GM->Phase = AGPMGameMode::EPhase::Press;
					Log(C, TEXT("prolongation"));
				} },
			{ 32.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M04_Prolongation")); } },
			{ 33.f, [CS, Log](UWorld* Wd, AGPMGameMode* GM)
				{
					UGPMCareerSubsystem* C = CS(Wd);
					if (UGPMSaveGame* G = C->Save()) G->MateClause = 0.f;
					C->DebugRivalOffer(); GM->bNegoFromMercato = false; GM->Phase = AGPMGameMode::EPhase::Press;
					Log(C, TEXT("offre"));
				} },
			{ 37.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M05_OffreRival")); } },
			{ 38.f, [CS, Pick, Log](UWorld* Wd, AGPMGameMode*) { Pick(CS(Wd), 10); Log(CS(Wd), TEXT("vente")); } },
			{ 41.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M06_Vente")); } },
			{ 42.f, [CS, Log](UWorld* Wd, AGPMGameMode* GM)
				{
					UGPMCareerSubsystem* C = CS(Wd);
					C->Queue.Reset(); GM->EndCine();
					C->Intersaison();
					GPMTest::NextSeason(Wd);
					GM->OpenMercato();
					Log(C, TEXT("intersaison"));
					if (const UGPMSaveGame* G = C->Save())
						for (int32 k = 0; k < G->Mail.Num() && k < 8; ++k) if (G->Mail[k].Tag == TEXT("mercato")) UE_LOG(LogGPM, Display, TEXT("TestMercato mail : %s\n%s"), *G->Mail[k].Subject, *G->Mail[k].Body);
				} },
			{ 47.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M07_Mercato_Intersaison")); } },
			{ 48.f, [](UWorld*, AGPMGameMode* GM) { GM->Phase = AGPMGameMode::EPhase::Season; GM->UiRequest = 3; } },
			{ 52.f, [](UWorld*, AGPMGameMode*) { GPMTest::Capture(TEXT("M08_BoiteMail")); } },
		};
		for (const FStep& St : Steps)
		{
			FTimerHandle H;
			TFunction<void(UWorld*, AGPMGameMode*)> Do = St.Do;
			World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, Do]()
			{
				if (!W.IsValid()) return;
				if (AGPMGameMode* GM = W->GetAuthGameMode<AGPMGameMode>()) Do(W.Get(), GM);
			}), St.At, false);
		}
		UE_LOG(LogGPM, Display, TEXT("GPM.TestMercato : parcours lancé"));
	}));


// « GPM.TestSC » : cinématique d'avant-course (une capture par plan), puis course en pilote automatique avec voiture de sécurité,
// drive-through, 5 s de pénalité et VSC (Saved/Captures/S*.png)
static FAutoConsoleCommandWithWorldAndArgs GTestScCmd(TEXT("GPM.TestSC"), TEXT("Cinématique, voiture de sécurité, pénalités et VSC avec captures (Saved/Captures/S*.png)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (!World) return;
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0, Shot = -1; float T = 0.f, ShotT = 0.f; };
		TSharedPtr<FState> St = MakeShared<FState>();
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) { GM->bSoloQuali = false; GM->SoloLaps = 6; GM->StartSolo(); }
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* C = GM ? GM->PlayerCar() : nullptr;
			if (!C || !C->GetTrack()) return;
			if (GM->Phase == AGPMGameMode::EPhase::Tuning) { GM->StartQuali(); return; }
			if (GM->Phase == AGPMGameMode::EPhase::Prerace) { GM->Go(false); return; }
			St->T += 0.25f;
			if (GM->Phase == AGPMGameMode::EPhase::Intro)
			{
				const int32 Sh = GM->CineShot();
				if (Sh != St->Shot) { St->Shot = Sh; St->ShotT = 0.f; }
				St->ShotT += 0.25f;
				if (FMath::IsNearlyEqual(St->ShotT, 1.25f)) GPMTest::Capture(FString::Printf(TEXT("S0_Intro%d"), Sh + 1));
				return;
			}
			if (GM->Phase != AGPMGameMode::EPhase::Race) return;
			C->bPlayerControlled = false;
			APlayerController* PC = W->GetFirstPlayerController();
			auto Shot = [St](const TCHAR* Name) { GPMTest::Capture(Name); St->Step++; St->T = 0.f; };
			switch (St->Step)
			{
			case 0: if (GM->Clock > 14.f) { GM->RequestSafety(true); St->Step++; St->T = 0.f; } break;
			case 1: if (St->T >= 4.f && GM->SafetyCar() && PC) { PC->SetViewTargetWithBlend(GM->SafetyCar(), 0.f); St->Step++; St->T = 0.f; } break;
			case 2: if (St->T >= 1.f) { Shot(TEXT("S1_VoitureSecurite")); if (PC) PC->SetViewTargetWithBlend(C, 0.f); } break;
			case 3: if (St->T >= 8.f) Shot(TEXT("S2_SousSC")); break;
			case 4: if (GM->SCState == 2) Shot(TEXT("S3_RentreCeTour")); break;
			case 5: if (GM->SCState == 3) Shot(TEXT("S4_Relance")); break;
			case 6: if (GM->SCState == 0) { Shot(TEXT("S5_DrapeauVert")); GM->TestPenalty(1, 0.f); } break;
			case 7: if (St->T >= 1.f) Shot(TEXT("S6_Enquete")); break;
			case 8: if (St->T >= 2.f) { Shot(TEXT("S7_DriveThrough")); UE_LOG(LogGPM, Display, TEXT("TestSC : drive-through à purger, tour %d"), C->Lap); } break;
			case 9: if (!C->PenKind && !C->PitPhase) { UE_LOG(LogGPM, Display, TEXT("TestSC : drive-through purgé, tour %d, arrêts %d"), C->Lap, C->Stops); GM->TestPenalty(0, 5.f); St->Step++; St->T = 0.f; } break;
			case 10: if (St->T >= 3.f) { Shot(TEXT("S8_Penalite5s")); GM->RequestSafety(true, true); } break;
			case 11: if (St->T >= 3.f) Shot(TEXT("S9_VSC")); break;
			case 12: if (GM->SCState == 0) { UE_LOG(LogGPM, Display, TEXT("TestSC : fin, pénalité restante %.0f s, total %.0f s"), C->Penalty, C->PenSum); St->Step++; } break;
			default: break;
			}
		}), 0.25f, true);
	}));
