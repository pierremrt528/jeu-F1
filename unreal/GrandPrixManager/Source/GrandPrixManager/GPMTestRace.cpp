// Test des abandons et de l'arrivée : GPM.TestAbandon <0|1> [circuit] lance une course solo de 4 tours en pilote automatique.
// 0 : une IA abandonne sur accident, une autre est ralentie pour finir à un tour, course jusqu'au drapeau (captures A1 à A3).
// 1 : le moteur du joueur casse au 2e tour (captures A4, A5). Classement final dans le journal (lignes « TestAbandon »).
#include "GPMGameMode.h"
#include "GPMDataSubsystem.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

static FAutoConsoleCommandWithWorldAndArgs GTestAbandonCmd(TEXT("GPM.TestAbandon"), TEXT("Course solo automatique : abandons et arrivée à un tour (GPM.TestAbandon <0 IA | 1 joueur> [circuit])."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const bool bPlayer = Args.Num() > 0 && FCString::Atoi(*Args[0]) == 1;
		const int32 Circ = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0; float T = 0.f, At = 0.f; bool bDone = false; AGPMCar* Slow = nullptr; };
		TSharedPtr<FState> St = MakeShared<FState>();
		auto Shot = [](const TCHAR* Name)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Name + TEXT(".png")), true, false);
		};
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, Circ]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) { GM->bSoloQuali = false; GM->SoloIdx = Circ; GM->SoloLaps = 4; GM->StartSolo(); }
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St, bPlayer, Shot]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* Me = GM ? GM->PlayerCar() : nullptr;
			if (!Me || !Me->GetTrack() || St->bDone) return;
			using EP = AGPMGameMode::EPhase;
			if (GM->Phase == EP::Tuning) { GM->SimQuali(); return; }
			if (GM->Phase == EP::QualiDone) { GM->OpenPrerace(); return; }
			if (GM->Phase == EP::Prerace) { GM->Go(false); return; }
			if (GM->Phase == EP::Intro) { GM->EndCine(); return; }
			if (GM->Phase != EP::Lights && GM->Phase != EP::Race && GM->Phase != EP::Done) return;
			Me->bPlayerControlled = false;
			if (AWorldSettings* WS = W->GetWorldSettings()) WS->SetTimeDilation(GM->Phase == EP::Race ? 2.f : 1.f);
			St->T += 0.1f;
			const TArray<AGPMCar*>& Ord = GM->Standings();
			if (GM->Phase == EP::Race && St->Step == 0 && GM->Clock > 14.f && Ord.Num() > 6)
			{
				if (bPlayer) { St->Step = 10; return; }
				// la 3e abandonne sur accident ; la dernière est bridée pour finir à un tour
				AGPMCar* Crash = Ord[2] != Me ? Ord[2] : Ord[3];
				St->Slow = Ord.Last() != Me ? Ord.Last() : Ord[Ord.Num() - 2];
				St->Slow->Perf *= 0.45f;
				UE_LOG(LogGPM, Display, TEXT("TestAbandon : abandon de %s, %s bridé"), *Crash->DriverName, *St->Slow->DriverName);
				GM->Retire(Crash, TEXT("accident"));
				St->Step = 1; St->At = St->T;
			}
			else if (St->Step == 1 && St->T > St->At + 2.f) { Shot(TEXT("A1_AbandonIA")); St->Step = 2; }
			else if (St->Step == 2 && Me->Lap >= 3 && Me->Idx > 200) { Shot(TEXT("A2_Tour3")); St->Step = 3; }
			else if (St->Step == 10 && Me->Lap >= 2 && Me->Idx > 150)
			{
				GM->Retire(Me, TEXT("moteur"));
				St->Step = 11; St->At = St->T;
			}
			else if (St->Step == 11 && St->T > St->At + 1.5f) { Shot(TEXT("A4_AbandonJoueur")); St->Step = 12; }
			if (GM->Phase == EP::Done && St->Step < 20) { St->Step = 20; St->At = St->T; if (AWorldSettings* WS = W->GetWorldSettings()) WS->SetTimeDilation(1.f); }
			if (St->Step == 20 && St->T > St->At + 2.f)
			{
				St->bDone = true;
				Shot(bPlayer ? TEXT("A5_ResultatsAbandon") : TEXT("A3_Arrivee"));
				int32 Fin = 0, Out = 0;
				for (int32 k = 0; k < Ord.Num(); ++k)
				{
					const AGPMCar* C = Ord[k];
					Fin += C->bFinished; Out += C->bOut;
					UE_LOG(LogGPM, Display, TEXT("TestAbandon P%-2d %-20s %-22s tours %d %s%s"), k + 1, *C->DriverName, k == 0 ? TEXT("vainqueur") : *GM->ResultGap(C), C->Lap - 1,
						C->bFinished ? TEXT("arrivé") : C->bOut ? TEXT("abandon") : TEXT("en piste"), C == Me ? TEXT(" (joueur)") : C == St->Slow ? TEXT(" (bridé)") : TEXT(""));
				}
				UE_LOG(LogGPM, Display, TEXT("TestAbandon FIN : %d arrivés, %d abandons, joueur P%d, chrono %.1f s"), Fin, Out, GM->PositionOf(Me), GM->Clock);
			}
			if (GM->Phase == EP::Race && St->T > 900.f) { St->bDone = true; UE_LOG(LogGPM, Warning, TEXT("TestAbandon FIN : course trop longue")); }
		}), 0.1f, true);
	}));

// GPM.TestRecommencer [écurie à rejoindre, −1 : créer la 11e] : écran de saison (bouton « Recommencer la carrière »), création d'une nouvelle écurie, puis état de la
// nouvelle carrière dans le journal (lignes « TestRecommencer ») et captures N1 à N3. Il efface la sauvegarde : la copier avant.
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "Engine/GameInstance.h"
static FAutoConsoleCommandWithWorldAndArgs GTestRestartCmd(TEXT("GPM.TestRecommencer"), TEXT("Recommence la carrière au début et vérifie la nouvelle sauvegarde."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 JoinIdx = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1;
		TWeakObjectPtr<UWorld> W = World;
		auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Name + TEXT(".png")), true, false); };
		auto State = [W](const TCHAR* Tag)
		{
			const UGPMSeasonSubsystem* S = W.IsValid() ? W->GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
			const UGPMSaveGame* G = S ? S->State() : nullptr;
			if (!G) return;
			int32 Pts = 0;
			for (const TPair<FString, int32>& P : G->DriverPts) Pts += P.Value;
			UE_LOG(LogGPM, Display, TEXT("TestRecommencer %s : %s, saison %d, manche %d / %d, %d résultats, %d points distribués, budget %.1f, %d pièces, %d messages, confiance %d, moral %d, équipier %s, ère %d"),
				Tag, *G->TeamName, G->Year, G->Round + 1, G->Calendar.Num(), G->Results.Num(), Pts, G->Budget, G->Owned.Num(), G->Mail.Num(), G->Conf, G->Moral, *G->MateName, G->EraI);
			if (const UGPMDataSubsystem* Dt = W->GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
			{
				FString Lst;
				for (const FGPMTeam& Tm : Dt->Teams) Lst += FString::Printf(TEXT("%s (%s / %s) · "), *Tm.Name, Tm.Drivers.Num() ? *Tm.Drivers[0].Name : TEXT("?"), Tm.Drivers.Num() > 1 ? *Tm.Drivers[1].Name : TEXT("?"));
				UE_LOG(LogGPM, Display, TEXT("TestRecommencer %s : mise en place %d, %d écuries : %s"), Tag, G->Layout, Dt->Teams.Num(), *Lst);
			}
		};
		struct FStep { float At; TFunction<void(AGPMGameMode*)> Do; };
		const TArray<FStep> Steps = {
			{ 8.f, [State](AGPMGameMode* GM) { State(TEXT("avant")); GM->ContinueCareer(); } },
			{ 14.f, [Shot](AGPMGameMode*) { Shot(TEXT("N1_Saison_Bouton")); } },
			{ 15.f, [JoinIdx](AGPMGameMode* GM) { GM->NewCareer(); if (UGPMCareerSubsystem* CS = GM->GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>()) CS->Pick.Join = JoinIdx; } },
			{ 19.f, [Shot](AGPMGameMode*) { Shot(TEXT("N2_Creation")); } },
			{ 20.f, [](AGPMGameMode* GM) { GM->FinishSetup(); } },
			{ 23.f, [](AGPMGameMode* GM) { if (GM->RoomCine()) GM->EndCine(); } },
			{ 28.f, [Shot, State](AGPMGameMode*) { Shot(TEXT("N3_NouvelleCarriere")); State(TEXT("après")); UE_LOG(LogGPM, Display, TEXT("TestRecommencer FIN")); } },
		};
		for (const FStep& St : Steps)
		{
			FTimerHandle H;
			TFunction<void(AGPMGameMode*)> Do = St.Do;
			World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, Do]() { if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) Do(GM); }), St.At, false);
		}
	}));

// GPM.TestBords [circuit] : compte les dégagements de tous les circuits (journal), puis pose la voiture du joueur en roue libre à
// 160 km/h dans l'herbe, dans un bac à gravier et sur une voie de dégagement : vitesse 1,5 s plus tard dans le journal, captures B1-B3.
static FAutoConsoleCommandWithWorldAndArgs GTestBordsCmd(TEXT("GPM.TestBords"), TEXT("Dégagements des circuits et effet de l'herbe, du gravier et de l'asphalte de dégagement."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 Circ = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		TWeakObjectPtr<UWorld> W = World;
		if (UGPMDataSubsystem* Data = World->GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
			for (const FString& Id : Data->GetCircuitIds()) Data->GetTrack(Id);
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, Circ]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) { GM->bSoloQuali = false; GM->SoloIdx = Circ; GM->SoloLaps = 3; GM->StartSolo(); }
		}), 6.f, false);
		struct FState { int32 Step = 0; float T = 0.f, At = 0.f; bool bDone = false; };
		TSharedPtr<FState> St = MakeShared<FState>();
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* Me = GM ? GM->PlayerCar() : nullptr;
			const FGPMTrackData* T = Me ? Me->GetTrack() : nullptr;
			if (!T || St->bDone) return;
			using EP = AGPMGameMode::EPhase;
			if (GM->Phase == EP::Tuning) { GM->SimQuali(); return; }
			if (GM->Phase == EP::QualiDone) { GM->OpenPrerace(); return; }
			if (GM->Phase == EP::Prerace) { GM->Go(false); return; }
			if (GM->Phase == EP::Intro) { GM->EndCine(); return; }
			if (GM->Phase != EP::Race) return;
			St->T += 0.1f;
			static const TCHAR* Names[3] = { TEXT("herbe"), TEXT("gravier"), TEXT("dégagement") };
			static const TCHAR* Shots[3] = { TEXT("B1_Herbe"), TEXT("B2_Gravier"), TEXT("B3_Degagement") };
			const int32 K = St->Step / 2;
			if (K >= 3) { St->bDone = true; UE_LOG(LogGPM, Display, TEXT("TestBords FIN")); return; }
			if (St->Step % 2 == 0)
			{
				if (St->T < St->At + 1.f) return;
				// un point de la surface voulue (0 herbe : un bord sans dégagement), loin de la ligne
				const int32 N = T->WX.Num();
				int32 At = -1, Side = 0;
				for (int32 i = 150; i < N - 150 && At < 0; ++i)
					for (int32 Sd = 0; Sd < 2 && At < 0; ++Sd)
						if (T->RunAt(i, Sd == 1) == K && T->RunAt(i + 12, Sd == 1) == K && (K > 0 || FMath::Abs(T->Curvature[i]) < 0.001f)) { At = i; Side = Sd; }
				if (At < 0) { UE_LOG(LogGPM, Display, TEXT("TestBords %s : aucune zone sur ce circuit"), Names[K]); St->Step += 2; return; }
				const float Lat = (Side ? 1.f : -1.f) * (T->Width * 0.5f + 4.5f);
				Me->bPlayerControlled = true;   // roue libre (personne au clavier)
				Me->Idx = At; Me->Lat = Me->Lane = Lat; Me->X = T->WX[At] - T->TZ[At] * Lat; Me->Z = T->WZ[At] + T->TX[At] * Lat; Me->Y = T->WY[At];
				Me->Heading = T->H[At]; Me->V = 45.f; Me->Beta = 0.f; Me->SpinT = 0.f;
				St->At = St->T; St->Step++;
			}
			else if (St->T >= St->At + 0.4f && St->T < St->At + 0.55f) FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Shots[K] + TEXT(".png")), true, false);
			else if (St->T >= St->At + 1.5f)
			{
				UE_LOG(LogGPM, Display, TEXT("TestBords %s : 162 km/h → %.0f km/h en 1,5 s (sol %d)"), Names[K], Me->V * 3.6f, int32(Me->Ground));
				St->At = St->T; St->Step++;
			}
		}), 0.1f, true);
	}));

// GPM.TestPreseason : pré-saison complète en pilote automatique (temps ×3) : révélation, événements (choix au hasard), trois journées
// d'essais, bilan. Lignes « TestPreseason » dans le journal ; captures P1 (écran), P2 (essais), P3 (bilan).
static FAutoConsoleCommandWithWorldAndArgs GTestPreCmd(TEXT("GPM.TestPreseason"), TEXT("Joue la pré-saison de janvier en pilote automatique."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (!World) return;
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0; float T = 0.f, At = 0.f; bool bDone = false; };
		TSharedPtr<FState> St = MakeShared<FState>();
		auto Shot = [](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Name + TEXT(".png")), true, false); };
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St, Shot]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			UGPMCareerSubsystem* CS = GM ? GM->GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
			UGPMSeasonSubsystem* SS = GM ? GM->GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
			UGPMSaveGame* G = SS ? SS->State() : nullptr;
			if (!GM || !CS || !G || St->bDone) return;
			St->T += 0.1f;
			using EP = AGPMGameMode::EPhase;
			if (AWorldSettings* WS = W->GetWorldSettings()) WS->SetTimeDilation(GM->bTesting ? 3.f : 1.f);
			if (St->T < 8.f) return;
			if (St->Step == 0)
			{
				GM->bTestAuto = true;
				if (CS->IsCreated()) GM->ContinueCareer(); else GM->FinishSetup();
				G->PreStage = 0; G->TestDay = 0; G->TestDays = 0.f; G->TestRes.Reset(); G->TestReport.Reset();   // repartir du 1er janvier
				UE_LOG(LogGPM, Display, TEXT("TestPreseason : départ, saison %d, étape %d"), G->Year, G->PreStage);
				St->Step = 1; St->At = St->T; return;
			}
			if (St->T < St->At + 3.f) return;
			if (GM->Phase == EP::Press)
			{
				if (FGPMDialog* Dl = CS->Current())
				{
					if (Dl->Chosen < 0) { UE_LOG(LogGPM, Display, TEXT("TestPreseason : fenêtre « %s »"), *Dl->Title); CS->Choose(FMath::RandRange(0, Dl->Choices.Num() - 1)); St->At = St->T - 2.f; }
					else CS->Next();
				}
				if (!CS->Current()) GM->EndDialogs();
				return;
			}
			if (GM->bTesting) { St->At = St->T; return; }
			if (GM->Phase != EP::Season || GM->RoomCine()) { if (GM->RoomCine() && GM->CineT > 6.f) GM->EndCine(); return; }
			if (St->Step == 1 && G->PreStage == 0) { Shot(TEXT("P1_PreSaison")); GM->StartReveal(); St->At = St->T + 4.f; return; }
			if (G->PreStage == 1) { GM->StartPreEvents(); St->At = St->T; if (G->PreStage == 2 && GM->Phase == EP::Season) St->At = St->T; return; }
			if (G->PreStage == 2 && G->TestDay < 3) { if (G->TestDay == 0) Shot(TEXT("P2_Essais")); GM->StartTest(); St->At = St->T + 2.f; return; }
			if (G->PreStage >= 3)
			{
				St->bDone = true;
				Shot(TEXT("P3_Bilan"));
				if (AWorldSettings* WS = W->GetWorldSettings()) WS->SetTimeDilation(1.f);
				UE_LOG(LogGPM, Display, TEXT("TestPreseason FIN : %d journées, résultats %d/%d/%d, %.0f jours gagnés, moral %d, budget %.1f M€"), G->TestDay,
					G->TestRes.IsValidIndex(0) ? G->TestRes[0] : 9, G->TestRes.IsValidIndex(1) ? G->TestRes[1] : 9, G->TestRes.IsValidIndex(2) ? G->TestRes[2] : 9, G->TestDays, G->Moral, G->Budget);
			}
		}), 0.1f, true);
	}));
