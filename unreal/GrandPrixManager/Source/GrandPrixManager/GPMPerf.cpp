// Mesure de la fluidité en course, sans toucher la souris : GPM.TestPerf [n° circuit] [météo 1 sec, 2 pluie]
// Course solo en pilote automatique, puis fenêtres de 8 s avec un réglage graphique changé à chaque fois
// (ombres, anticrénelage, résolution, distance, caméra) : le journal dit ce qui coûte le plus à la carte graphique.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMCar.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMemory.h"

namespace GPMPerf
{
	void SetCVar(const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(Name)) CV->Set(Value, ECVF_SetByCode);
	}
	FString GetCVar(const TCHAR* Name)
	{
		IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(Name);
		return CV ? CV->GetString() : FString(TEXT("?"));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GPerfCmd(TEXT("GPM.TestPerf"), TEXT("Course solo automatique et mesure des i/s avec plusieurs réglages graphiques."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		const int32 Idx = Args.Num() ? FCString::Atoi(*Args[0]) : 0;
		const int32 Wx = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 2;
		TWeakObjectPtr<UWorld> W = World;
		struct FWin { const TCHAR* Name; TArray<TPair<const TCHAR*, FString>> Set; int32 Cam = -1; };
		struct FState
		{
			int32 Step = 0, Win = -1, Frames = 0, Hitch = 0;
			bool bShot = false;
			float T = 0.f, Sum = 0.f, Worst = 0.f, Gpu = 0.f, Game = 0.f, Rend = 0.f, Draws = 0.f, Prims = 0.f;
			TArray<FWin> Wins;
			TMap<FString, FString> Saved;
		};
		TSharedPtr<FState> St = MakeShared<FState>();
		auto Add = [&St](const TCHAR* Name, TArray<TPair<const TCHAR*, FString>> Set, int32 Cam = -1) { St->Wins.Add({ Name, MoveTemp(Set), Cam }); };
		Add(TEXT("réglages du jeu"), {});
		Add(TEXT("ancien lissage TSR"), { { TEXT("r.AntiAliasingMethod"), TEXT("4") } });
		Add(TEXT("caméra T-cam"), {}, 1);
		Add(TEXT("caméra cockpit"), {}, 2);
		Add(TEXT("réglages du jeu (fin)"), {}, 0);
		UE_LOG(LogGPM, Display, TEXT("TestPerf : AA %s · circuit %d, météo %d · sg.Shadow %s, cascades %s, MSAA %s, vue %s, %%écran %s"), *GPMPerf::GetCVar(TEXT("r.AntiAliasingMethod")), Idx, Wx,
			*GPMPerf::GetCVar(TEXT("sg.ShadowQuality")), *GPMPerf::GetCVar(TEXT("r.Shadow.CSM.MaxCascades")), *GPMPerf::GetCVar(TEXT("r.MSAACount")),
			*GPMPerf::GetCVar(TEXT("r.ViewDistanceScale")), *GPMPerf::GetCVar(TEXT("r.ScreenPercentage")));

		FTimerHandle H0;
		World->GetTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([W, Idx, Wx]()
		{
			if (AGPMGameMode* GM = W.IsValid() ? W->GetAuthGameMode<AGPMGameMode>() : nullptr)
			{ GM->bSoloQuali = false; GM->SoloIdx = Idx; GM->SoloWeather = Wx; GM->SoloLaps = 5; GM->StartSolo(); }
		}), 6.f, false);

		// une fois par image (pas un minuteur, qui peut se déclencher plusieurs fois dans la même image)
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([W, St](float Dt) -> bool
		{
			if (!W.IsValid() || St->Step == 2) return false;
			AGPMGameMode* GM = W->GetAuthGameMode<AGPMGameMode>();
			AGPMCar* C = GM ? GM->PlayerCar() : nullptr;
			if (!C || !C->GetTrack()) return true;
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Prerace) { GM->Go(false); return true; }
			if (St->Step == 0 && GM->Phase == AGPMGameMode::EPhase::Intro) { GM->EndCine(); return true; }
			if (GM->Phase != AGPMGameMode::EPhase::Race || GM->Clock < 1.f) return true;
			if (St->Step == 0) { C->bPlayerControlled = false; C->SetCamMode(0); St->Step = 1; St->T = -4.f; return true; }   // pilote automatique, 4 s pour se lancer
			St->T += Dt;
			if (St->T < 0.f) return true;
			// mesure de l'image (sautée pendant 1,5 s après chaque changement de réglage)
			if (St->Win >= 0 && St->T > 1.5f)
			{
				St->Frames++; St->Sum += Dt; St->Worst = FMath::Max(St->Worst, Dt);
				if (Dt > 1.f / 40.f) St->Hitch++;
				St->Gpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
				St->Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
				St->Rend += FPlatformTime::ToMilliseconds(GRenderThreadTime);
				St->Draws += GNumDrawCallsRHI[0];
				St->Prims += GNumPrimitivesDrawnRHI[0];
			}
			// capture d'écran au milieu des fenêtres de caméra (Saved/Captures/P_Cam*.png)
			if (St->Win >= 0 && !St->bShot && St->T > 5.f && St->Wins[St->Win].Cam >= 0)
			{
				St->bShot = true;
				FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Captures/P_Cam%d.png"), St->Wins[St->Win].Cam)), true, false);
			}
			if (St->Win >= 0 && St->T < 9.5f) return true;
			if (St->Win >= 0 && St->Frames > 0)
			{
				const float N = St->Frames;
				const FPlatformMemoryStats Mem = FPlatformMemory::GetStats();
				UE_LOG(LogGPM, Display, TEXT("TestPerf [%s] : %.0f i/s, pire image %.0f ms, %d à-coups · GPU %.1f ms, jeu %.1f ms, rendu %.1f ms · %.0f appels de dessin, %.0f k triangles · mémoire du jeu %.0f Mo, libre %.0f Mo"),
					St->Wins[St->Win].Name, N / St->Sum, St->Worst * 1000.f, St->Hitch, St->Gpu / N, St->Game / N, St->Rend / N, St->Draws / N, St->Prims / N / 1000.f,
					Mem.UsedPhysical / 1048576.0, Mem.AvailablePhysical / 1048576.0);
				// remet les réglages de la fenêtre
				for (const TPair<const TCHAR*, FString>& S : St->Wins[St->Win].Set)
					if (const FString* Old = St->Saved.Find(S.Key)) GPMPerf::SetCVar(S.Key, **Old);
			}
			St->Win++;
			if (St->Win == 0 && GEngine) GEngine->Exec(W.Get(), TEXT("CsvProfile Start"));
			St->T = 0.f; St->bShot = false; St->Frames = St->Hitch = 0; St->Sum = St->Worst = St->Gpu = St->Game = St->Rend = St->Draws = St->Prims = 0.f;
			if (St->Win >= St->Wins.Num())
			{
				UE_LOG(LogGPM, Display, TEXT("TestPerf : terminé"));
				if (GEngine) GEngine->Exec(W.Get(), TEXT("CsvProfile Stop"));
				St->Step = 2;
				GM->bPerfTest = false;
				return false;
			}
			const FWin& Wn = St->Wins[St->Win];
			for (const TPair<const TCHAR*, FString>& S : Wn.Set)
			{
				St->Saved.Add(S.Key, GPMPerf::GetCVar(S.Key));
				GPMPerf::SetCVar(S.Key, *S.Value);
			}
			if (Wn.Cam >= 0) C->SetCamMode(Wn.Cam);
			// la résolution adaptative ne doit pas fausser la mesure
			GM->ScreenPct = FCString::Atoi(*GPMPerf::GetCVar(TEXT("r.ScreenPercentage")));
			GM->bPerfTest = true;
			return true;
		}));
	}));
