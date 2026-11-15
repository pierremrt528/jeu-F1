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
