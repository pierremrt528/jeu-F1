// Commandes console de test des grandes refontes du règlement (captures d'écran sans toucher la souris) :
// GPM.Refonte (le règlement de la prochaine ère est publié), GPM.Intersaison (passe à la saison suivante),
// GPM.Capture <nom>, GPM.Test (parcours complet avec captures dans Saved/Captures).
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
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
