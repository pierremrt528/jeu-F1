// Test des cinématiques sans toucher la souris : GPM.TestCine
// Course solo : survol avant le départ (4 plans), tour d'honneur d'une victoire (3 plans), sacre au QG (3 plans),
// signature d'un pilote ; une capture au milieu de chaque plan dans Saved/Captures (C*, P*, K*, G*).
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

static FAutoConsoleCommandWithWorldAndArgs GCineTestCmd(TEXT("GPM.TestCine"), TEXT("Joue les cinématiques (survol, tour d'honneur, sacre, signature) avec captures (Saved/Captures/C*, P*, K*, G*)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 Idx = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		TWeakObjectPtr<UWorld> W = World;
		struct FState { int32 Step = 0, Shot = 0; };
		TSharedPtr<FState> St = MakeShared<FState>();
		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, Idx]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) { GM->bSoloQuali = false; GM->SoloIdx = Idx; GM->StartSolo(); }
		}), 6.f, false);
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([W, St]()
		{
			using EC = AGPMGameMode::ECine;
			using EP = AGPMGameMode::EPhase;
			AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr;
			AGPMCar* C = GM ? GM->PlayerCar() : nullptr;
			if (!C) return;
			auto Shot = [](const FString& Name)
			{
				const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Name + TEXT(".png"));
				FScreenshotRequest::RequestScreenshot(File, true, false);
				UE_LOG(LogGPM, Display, TEXT("TestCine : capture %s"), *Name);
			};
			// une capture au milieu de chaque plan
			auto Shoot = [&](const TCHAR* Prefix, const float* At, int32 N)
			{
				if (St->Shot < N && GM->CineT >= At[St->Shot]) { Shot(FString::Printf(TEXT("%s%d"), Prefix, St->Shot)); St->Shot++; }
			};
			static const float IntroAt[4] = { 1.7f, 5.1f, 8.3f, 11.2f };
			static const float PodiumAt[3] = { 1.4f, 4.2f, 7.2f };
			static const float CrownAt[3] = { 1.5f, 4.5f, 7.6f };
			static const float SignAt[2] = { 1.2f, 3.4f };
			if (St->Step == 0)
			{
				if (GM->Phase == EP::Prerace) { GM->Go(false); return; }
				if (GM->Cine == EC::Intro) { Shoot(TEXT("C"), IntroAt, 4); return; }
				if (GM->Phase == EP::Lights || GM->Phase == EP::Race) { St->Step = 1; St->Shot = 0; }
				return;
			}
			if (St->Step == 1)
			{
				C->bPlayerControlled = false;   // pilote automatique
				if (GM->Phase == EP::Race && GM->Clock > 10.f)
				{
					GM->CineTitle = TEXT("VICTOIRE !");
					GM->CineSub = TEXT("Test des cinématiques");
					GM->Phase = EP::Outro;
					GM->StartCine(EC::Podium);
					St->Step = 2; St->Shot = 0;
				}
				return;
			}
			if (St->Step == 2)
			{
				if (GM->Cine == EC::Podium) { Shoot(TEXT("P"), PodiumAt, 3); return; }
				GM->CineTitle = TEXT("CHAMPION DU MONDE !");
				GM->CineSub = TEXT("Saison 2027 · 412 points");
				GM->StartCine(EC::Crown);
				St->Step = 3; St->Shot = 0;
				return;
			}
			if (St->Step == 3)
			{
				if (GM->Cine == EC::Crown) { Shoot(TEXT("K"), CrownAt, 3); return; }
				GM->StartSign(TEXT("Test Pilote"));
				St->Step = 4; St->Shot = 0;
				return;
			}
			if (St->Step == 4)
			{
				if (GM->Cine == EC::Sign) { Shoot(TEXT("G"), SignAt, 2); return; }
				UE_LOG(LogGPM, Display, TEXT("TestCine : fini"));
				St->Step = 5;
			}
		}), 0.1f, true);
	}));
