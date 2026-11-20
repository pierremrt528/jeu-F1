// Commandes console de test des pilotes IA et des réglages (sans toucher la souris) :
// GPM.TestIA <n° circuit> <tours> : course solo en pilote automatique (temps accéléré ×2), bilan par pilote dans le log
// (dépassements, défenses, erreurs, sorties de piste, murs, contacts, tête-à-queue) et captures dans Saved/Captures ;
// GPM.TestReglages <n° circuit> : écran de réglages (conseil de l'ingénieur, réglage modifié) avec captures.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace GPMTestAI
{
	void Capture(const FString& Name)
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, true, false);
		UE_LOG(LogGPM, Display, TEXT("Capture : %s"), *File);
	}

	struct FStats { int32 Off = 0, Wall = 0, Spin = 0, Contact = 0, Mistake = 0, Defend = 0, Blue = 0, Gain = 0, Lost = 0, Pos = 0, Grid = 0; bool bOff = false, bWall = false, bSpin = false, bCont = false, bMis = false, bDef = false, bBlue = false; };
}

static FAutoConsoleCommandWithWorldAndArgs GTestIACmd(TEXT("GPM.TestIA"), TEXT("Course solo en pilote automatique : bilan des pilotes IA dans le log (GPM.TestIA <circuit> <tours>)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 Circ = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		const int32 Laps = Args.Num() > 1 ? FMath::Max(1, FCString::Atoi(*Args[1])) : 3;
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0; float T = 0.f; TMap<AGPMCar*, GPMTestAI::FStats> S; bool bDone = false; };
		TSharedPtr<FState> St = MakeShared<FState>();
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, Circ, Laps]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr)
			{
				GM->bSoloQuali = false; GM->SoloIdx = Circ; GM->SoloLaps = Laps; GM->StartSolo();
			}
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* Me = GM ? GM->PlayerCar() : nullptr;
			if (!Me || !Me->GetTrack() || St->bDone) return;
			using EP = AGPMGameMode::EPhase;
			if (GM->Phase == EP::Tuning) { GM->SimQuali(); return; }
			if (GM->Phase == EP::QualiDone) { GM->OpenPrerace(); return; }
			if (GM->Phase == EP::Prerace) { GM->Go(false); return; }
			if (GM->Phase == EP::Intro) { GM->EndCine(); return; }
			const bool bOn = GM->Phase == EP::Lights || GM->Phase == EP::Race || GM->Phase == EP::Done;
			if (!bOn) return;
			Me->bPlayerControlled = false;   // pilote automatique
			if (AWorldSettings* WS = W->GetWorldSettings()) WS->SetTimeDilation(GM->Phase == EP::Race ? 2.f : 1.f);
			St->T += 0.1f;
			const int32 N = Me->GetTrack()->WX.Num();
			// bilan : chaque événement compté une fois (front montant)
			for (AGPMCar* C : Me->Field)
			{
				if (!C) continue;
				GPMTestAI::FStats& S = St->S.FindOrAdd(C);
				const int32 P = GM->PositionOf(C);
				if (GM->Phase == EP::Lights) { S.Grid = P; S.Pos = P; continue; }
				if (GM->Phase != EP::Race) continue;
				if (S.Pos > 0 && C->Lap >= 1 && P < S.Pos) S.Gain += S.Pos - P;
				if (S.Pos > 0 && C->Lap >= 1 && P > S.Pos) S.Lost += P - S.Pos;
				S.Pos = P;
				auto Edge = [](bool bNow, bool& bWas, int32& Count) { if (bNow && !bWas) ++Count; bWas = bNow; };
				Edge(C->bOffTrack, S.bOff, S.Off);
				Edge(C->HitWall > 0.3f, S.bWall, S.Wall);
				Edge(C->SpinT > 0.f, S.bSpin, S.Spin);
				Edge(C->ContactT > 0.45f, S.bCont, S.Contact);
				Edge(C->bMistake, S.bMis, S.Mistake);
				Edge(C->DefendT > 0.f, S.bDef, S.Defend);
				Edge(C->bBlue, S.bBlue, S.Blue);
			}
			if (St->Step == 0 && GM->Phase == EP::Race && GM->Clock > 4.f) { GPMTestAI::Capture(TEXT("IA1_Depart")); St->Step++; }
			else if (St->Step == 1 && Me->Lap >= 1 && Me->Idx > N / 3) { GPMTestAI::Capture(TEXT("IA2_Tour1")); St->Step++; }
			else if (St->Step == 2 && Me->Lap >= 2 && Me->Idx > N / 2) { GPMTestAI::Capture(TEXT("IA3_Tour2")); St->Step++; }
			if (GM->Phase == EP::Done || (GM->Phase == EP::Race && St->T > 900.f))
			{
				St->bDone = true;
				if (AWorldSettings* WS = W->GetWorldSettings()) WS->SetTimeDilation(1.f);
				int32 Tot[9] = { 0 };
				float Best = 1e9f, Worst = 0.f;
				TArray<AGPMCar*> Cars = Me->Field;
				Cars.Sort([GM](const AGPMCar& A, const AGPMCar& B) { return GM->PositionOf(&A) < GM->PositionOf(&B); });
				for (AGPMCar* C : Cars)
				{
					const GPMTestAI::FStats& S = St->S.FindRef(C);
					UE_LOG(LogGPM, Display, TEXT("TestIA P%-2d (grille P%-2d) %-22s meilleur %.3f  gagné %d perdu %d  défenses %d  erreurs %d  sorties %d  murs %d  contacts %d  tête-à-queue %d  bleus %d"),
						GM->PositionOf(C), S.Grid, *C->DriverName, C->BestLap, S.Gain, S.Lost, S.Defend, S.Mistake, S.Off, S.Wall, S.Contact, S.Spin, S.Blue);
					const int32 V[9] = { S.Gain, S.Defend, S.Mistake, S.Off, S.Wall, S.Contact, S.Spin, S.Blue, C->bFinished ? 1 : 0 };
					for (int32 k = 0; k < 9; ++k) Tot[k] += V[k];
					if (C->BestLap > 0.f) { Best = FMath::Min(Best, C->BestLap); Worst = FMath::Max(Worst, C->BestLap); }
				}
				UE_LOG(LogGPM, Display, TEXT("TestIA bilan : %d voitures, %d à l'arrivée, places gagnées %d, défenses %d, erreurs %d, sorties %d, murs %d, contacts %d, tête-à-queue %d, drapeaux bleus %d, meilleurs tours %.3f à %.3f (écart %.2f s)"),
					Cars.Num(), Tot[8], Tot[0], Tot[1], Tot[2], Tot[3], Tot[4], Tot[5], Tot[6], Tot[7], Best, Worst, Worst - Best);
				GPMTestAI::Capture(TEXT("IA4_Arrivee"));
			}
		}), 0.1f, true);
	}));

static FAutoConsoleCommandWithWorldAndArgs GTestSetupCmd(TEXT("GPM.TestReglages"), TEXT("Écran de réglages du circuit <n°> : conseil de l'ingénieur et captures (GPM.TestReglages <circuit>)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 Circ = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		TWeakObjectPtr<UWorld> W = World;
		TSharedPtr<int32> Step = MakeShared<int32>(0);
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, Circ]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr)
			{
				GM->bSoloQuali = true; GM->SoloIdx = Circ; GM->SoloLaps = 3; GM->StartSolo();
			}
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, Step]()
		{
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			if (!GM || GM->Phase != AGPMGameMode::EPhase::Tuning || *Step > 3) return;
			const int32 S = (*Step)++;
			if (S == 0)
			{
				const FGPMCarSetup& A = GM->Advice();
				FString Line;
				for (int32 k = 0; k < FGPMCarSetup::Num; ++k) Line += FString::Printf(TEXT("%s %s ; "), FGPMCarSetup::Name(k), *A.Value(k));
				const float Ref = GM->SetupLap(FGPMCarSetup());
				UE_LOG(LogGPM, Display, TEXT("TestReglages %s : conseil = %s"), GM->Circuit() ? *GM->Circuit()->Name : TEXT("?"), *Line);
				UE_LOG(LogGPM, Display, TEXT("TestReglages tour simulé : base %.3f, conseil %.3f, appui maxi %.3f, vitesse maxi %.3f"), Ref, GM->SetupLap(A),
					[GM]() { FGPMCarSetup X; X.V[0] = X.V[1] = 10; X.V[2] = 3; return GM->SetupLap(X); }(),
					[GM]() { FGPMCarSetup X; X.V[0] = X.V[1] = 0; X.V[2] = 7; return GM->SetupLap(X); }());
				for (int32 k = 0; k < FGPMCarSetup::Num; ++k)
				{
					FGPMCarSetup X; X.V[k] = A.V[k];
					FGPMCarSetup Lo; Lo.V[k] = 0;
					FGPMCarSetup Hi; Hi.V[k] = 10;
					UE_LOG(LogGPM, Display, TEXT("TestReglages %-24s conseil %2d : %+.3f s   cran 0 : %+.3f s   cran 10 : %+.3f s"), FGPMCarSetup::Name(k), A.V[k],
						GM->SetupLap(X) - Ref, GM->SetupLap(Lo) - Ref, GM->SetupLap(Hi) - Ref);
				}
				GPMTestAI::Capture(TEXT("R1_Conseil"));
			}
			else if (S == 1)
			{
				GM->SetupRow = FGPMCarSetup::BrakeBias;
				GM->Setup.V[FGPMCarSetup::BrakeBias] = 10;
				GM->Setup.V[FGPMCarSetup::FrontWing] = 0;
			}
			else if (S == 2) GPMTestAI::Capture(TEXT("R2_Modifie"));
		}), 1.5f, true);
	}));
