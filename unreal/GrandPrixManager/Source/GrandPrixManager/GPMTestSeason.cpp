// Test automatique d'une carrière : GPM.TestSaison [saisons] simule tous les Grands Prix (qualifications et course simulées,
// réponses à la presse et aux flash info au hasard, recherches lancées à l'usine, intersaisons) et vérifie l'état de la
// sauvegarde après chaque course. Lignes « TestSaison » dans le journal ; « TestSaison ANOMALIE » = quelque chose cloche.
// Il modifie la sauvegarde : la copier avant, la remettre après.
#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
#include "GPMDataSubsystem.h"
#include "GPMTrack.h"
#include "GPMCar.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

namespace GPMTestSeason
{
	struct FState
	{
		int32 Seasons = 2, Done = 0, Races = 0, Bad = 0, Stuck = 0, LastRound = -1, LastYear = -1;
		float T = 0.f;
		bool bStarted = false, bOver = false;
		FTimerHandle Handle;
	};

	void Bad(FState& St, const FString& Msg)
	{
		++St.Bad;
		UE_LOG(LogGPM, Warning, TEXT("TestSaison ANOMALIE : %s"), *Msg);
	}

	// l'état de la carrière tient-il debout ?
	void Check(FState& St, UGameInstance* GI, AGPMGameMode* GM, bool bCars = false)
	{
		UGPMSeasonSubsystem* Season = GI->GetSubsystem<UGPMSeasonSubsystem>();
		UGPMDataSubsystem* Data = GI->GetSubsystem<UGPMDataSubsystem>();
		const UGPMSaveGame* G = Season ? Season->State() : nullptr;
		if (!G || !Data) { Bad(St, TEXT("pas de sauvegarde")); return; }
		const FString Where = FString::Printf(TEXT("%d manche %d"), G->Year, G->Round);
		if (!FMath::IsFinite(G->Budget) || G->Budget < -500.f || G->Budget > 5000.f) Bad(St, FString::Printf(TEXT("%s : budget %.1f"), *Where, G->Budget));
		if (G->Moral < 0 || G->Moral > 100 || G->Rep < 0 || G->Rep > 100 || G->Conf < 0 || G->Conf > 100)
			Bad(St, FString::Printf(TEXT("%s : moral %d, réputation %d, confiance %d"), *Where, G->Moral, G->Rep, G->Conf));
		if (G->Results.Num() != G->Round) Bad(St, FString::Printf(TEXT("%s : %d résultats pour %d manches"), *Where, G->Results.Num(), G->Round));
		if (G->Mail.Num() > 150) Bad(St, FString::Printf(TEXT("%s : %d messages dans la boîte mail"), *Where, G->Mail.Num()));
		// écuries : 2 pilotes chacune, noms uniques, niveaux plausibles
		TSet<FString> Names;
		for (int32 t = 0; t < Data->Teams.Num(); ++t)
		{
			const FGPMTeam& Tm = Data->Teams[t];
			if (Tm.Drivers.Num() != 2) Bad(St, FString::Printf(TEXT("%s : %s a %d pilote(s)"), *Where, *Tm.Name, Tm.Drivers.Num()));
			if (!FMath::IsFinite(Tm.Perf) || Tm.Perf < 0.85f || Tm.Perf > 1.1f) Bad(St, FString::Printf(TEXT("%s : %s performance %.3f"), *Where, *Tm.Name, Tm.Perf));
			for (const FGPMDriver& D : Tm.Drivers)
			{
				if (D.Name.IsEmpty()) Bad(St, FString::Printf(TEXT("%s : pilote sans nom chez %s"), *Where, *Tm.Name));
				if (Names.Contains(D.Name)) Bad(St, FString::Printf(TEXT("%s : %s pilote deux voitures"), *Where, *D.Name));
				Names.Add(D.Name);
				if (!FMath::IsFinite(D.Skill) || D.Skill < 0.8f || D.Skill > 1.02f) Bad(St, FString::Printf(TEXT("%s : %s niveau %.3f"), *Where, *D.Name, D.Skill));
				if (G->Retired.Contains(D.Name)) Bad(St, FString::Printf(TEXT("%s : %s est retraité mais pilote pour %s"), *Where, *D.Name, *Tm.Name));
				if (G->Free.Contains(D.Name)) Bad(St, FString::Printf(TEXT("%s : %s est libre et pilote pour %s"), *Where, *D.Name, *Tm.Name));
			}
		}
		if (Data->Teams.Num() && Data->Teams[0].Drivers.Num() > 1 && Data->Teams[0].Drivers[1].Name != G->MateName)
			Bad(St, FString::Printf(TEXT("%s : équipier « %s » dans la sauvegarde, « %s » sur la voiture"), *Where, *G->MateName, *Data->Teams[0].Drivers[1].Name));
		if (G->MateSal < 0.f || G->MateSal > 60.f || !FMath::IsFinite(G->MateSal)) Bad(St, FString::Printf(TEXT("%s : salaire de l'équipier %.1f"), *Where, G->MateSal));
		// points : pilotes et écuries d'accord, 101 ou 102 points par course
		int32 DSum = 0, TSum = 0;
		for (const TPair<FString, int32>& P : G->DriverPts) DSum += P.Value;
		for (const TPair<FString, int32>& P : G->TeamPts) TSum += P.Value;
		if (DSum != TSum) Bad(St, FString::Printf(TEXT("%s : %d points pilotes, %d points écuries"), *Where, DSum, TSum));
		if (DSum < G->Round * 101 || DSum > G->Round * 102) Bad(St, FString::Printf(TEXT("%s : %d points distribués en %d courses"), *Where, DSum, G->Round));
		for (const TPair<FString, int32>& P : G->TeamPts)
			if (!Data->Teams.ContainsByPredicate([&P](const FGPMTeam& X) { return X.Name == P.Key; })) Bad(St, FString::Printf(TEXT("%s : points pour l'écurie inconnue « %s »"), *Where, *P.Key));
		// voitures en piste : mêmes noms que les écuries (vérifié avant la course, une fois le circuit de la manche chargé)
		if (GM && bCars)
			for (const AGPMCar* C : GM->Standings())
				if (C && !Names.Contains(C->DriverName)) Bad(St, FString::Printf(TEXT("%s : voiture de « %s », inconnu des écuries"), *Where, *C->DriverName));
	}

	void Step(TWeakObjectPtr<UWorld> W, TSharedRef<FState> St)
	{
		if (!W.IsValid() || St->bOver) return;
		AGPMGameMode* GM = W->GetAuthGameMode<AGPMGameMode>();
		UGameInstance* GI = W->GetGameInstance();
		UGPMSeasonSubsystem* Season = GI ? GI->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
		UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
		UGPMFactorySubsystem* Factory = GI ? GI->GetSubsystem<UGPMFactorySubsystem>() : nullptr;
		if (!GM || !Season || !Career || !Factory) return;
		St->T += 0.2f;
		if (St->T < 8.f) return;
		typedef AGPMGameMode::EPhase EP;
		if (!St->bStarted)
		{
			St->bStarted = true;
			if (Career->IsCreated()) GM->ContinueCareer(); else GM->FinishSetup();
			UE_LOG(LogGPM, Display, TEXT("TestSaison : départ, saison %d manche %d / %d"), Season->Year(), Season->Round() + 1, Season->NumRounds());
			return;
		}
		auto Finish = [&](const TCHAR* Why)
		{
			St->bOver = true;
			W->GetTimerManager().ClearTimer(St->Handle);
			UE_LOG(LogGPM, Display, TEXT("TestSaison FIN (%s) : %d saison(s), %d course(s), %d anomalie(s)"), Why, St->Done, St->Races, St->Bad);
		};
		// le jeu avance-t-il ? (même manche pendant 40 s : bloqué)
		if (Season->Round() == St->LastRound && Season->Year() == St->LastYear) { if (++St->Stuck > 200) { Bad(*St, FString::Printf(TEXT("bloqué en phase %d"), int32(GM->Phase))); Finish(TEXT("bloqué")); return; } }
		else { St->Stuck = 0; St->LastRound = Season->Round(); St->LastYear = Season->Year(); }
		if (GM->RoomCine()) { GM->EndCine(); return; }
		const UGPMSaveGame* G = Season->State();
		switch (GM->Phase)
		{
		case EP::Season:
		{
			if (Season->IsOver())
			{
				// bilan de la saison, puis intersaison
				const TArray<FGPMStanding> Dr = Season->Drivers(), Tm = Season->Teams();
				int32 MyD = 0, MyT = 0;
				for (int32 k = 0; k < Dr.Num(); ++k) if (Dr[k].bPlayer) MyD = k + 1;
				for (int32 k = 0; k < Tm.Num(); ++k) if (Tm[k].bPlayer) MyT = k + 1;
				UE_LOG(LogGPM, Display, TEXT("TestSaison BILAN %d : champion %s (%d pts), écurie %s (%d pts) · joueur P%d, son écurie P%d · budget %.1f M€, confiance %d, %d pièces, équipier %s"),
					Season->Year(), Dr.Num() ? *Dr[0].Name : TEXT("?"), Dr.Num() ? Dr[0].Pts : 0, Tm.Num() ? *Tm[0].Name : TEXT("?"), Tm.Num() ? Tm[0].Pts : 0,
					MyD, MyT, G ? G->Budget : 0.f, G ? G->Conf : 0, G ? G->Owned.Num() : 0, G ? *G->MateName : TEXT("?"));
				if (MyD == 0 || MyT == 0) Bad(*St, TEXT("le joueur ou son écurie manque au classement"));
				// indice de performance de chaque écurie en fin de saison (la première est celle du joueur)
				if (G && G->PerfN > 0 && G->PerfHist.Num() >= G->PerfN)
				{
					FString Perf;
					for (int32 k = G->PerfHist.Num() - G->PerfN; k < G->PerfHist.Num(); ++k) Perf += FString::Printf(TEXT("%.3f "), G->PerfHist[k]);
					FString Lv;
					for (const FGPMBuilding& B : Factory->Buildings()) Lv += FString::FromInt(Factory->Level(B.Id));
					UE_LOG(LogGPM, Display, TEXT("TestSaison PERF %d : %s· installations %s"), Season->Year(), *Perf, *Lv);
				}
				++St->Done;
				if (St->Done >= St->Seasons) { Finish(TEXT("terminé")); return; }
				GM->Continue();
				Check(*St, GI, GM);
				return;
			}
			if (const FGPMTrackData* T = GM->Circuit()) { if (T->Id != Season->CurrentCircuit()) return; }   // le circuit de la manche se charge
			else return;
			if (G->PreStage < 3) { GM->SkipPreseason(); return; }   // la pré-saison est testée à part (GPM.TestPreseason)
			// usine : comme un joueur appliqué, tous les ateliers tournent (en gardant 10 M€), puis un chantier quand il reste 20 M€ de marge
			for (const FGPMTree& Tree : Factory->Trees())
				for (const FGPMPart& P : Tree.Parts)
					if (Factory->StateOf(P) == UGPMFactorySubsystem::EState::Available && Factory->PriceOf(P) < Factory->Budget() - 10.f) Factory->Start(P.Id);
			for (const FGPMBuilding& B : Factory->Buildings())
			{
				FString Why;
				if (Factory->CanBuild(B.Id, Why) && UGPMFactorySubsystem::BuildCost(B.Id, Factory->Level(B.Id)) < Factory->Budget() - 20.f) { Factory->Build(B.Id); break; }
			}
			Check(*St, GI, GM, true);
			GM->SimGrandPrix();
			++St->Races;
			if (GM->Phase != EP::Done) Bad(*St, FString::Printf(TEXT("après la course simulée : phase %d"), int32(GM->Phase)));
			if (const UGPMSaveGame* G2 = Season->State())
				if (G2->Results.Num())
					UE_LOG(LogGPM, Display, TEXT("TestSaison %d GP %d %s : P%d (%d pts), vainqueur %s · budget %.1f M€, moral %d, confiance %d"), G2->Year, G2->Round, *G2->Results.Last().Circuit,
						G2->Results.Last().Pos, G2->Results.Last().Pts, *G2->Results.Last().Winner, G2->Budget, G2->Moral, G2->Conf);
			Check(*St, GI, GM);
			return;
		}
		case EP::Done: GM->Continue(); return;
		case EP::Press:
			if (FGPMDialog* Dl = Career->Current())
			{
				if (Dl->Chosen < 0)
				{
					TArray<int32> Ok;
					for (int32 k = 0; k < Dl->Choices.Num(); ++k) if (!Dl->Choices[k].bOff) Ok.Add(k);
					if (Ok.Num() == 0) { Bad(*St, FString::Printf(TEXT("fenêtre sans choix possible : %s"), *Dl->Title)); Career->Queue.RemoveAt(0); return; }
					if (Dl->Title.IsEmpty() && Dl->Text.IsEmpty()) Bad(*St, TEXT("fenêtre vide"));
					if (Dl->Text.Contains(TEXT("{")) || Dl->Title.Contains(TEXT("{")) || Dl->Text.Contains(TEXT("undefined"))) Bad(*St, FString::Printf(TEXT("texte non rempli : %s | %s"), *Dl->Title, *Dl->Text));
					Career->Choose(Ok[FMath::RandRange(0, Ok.Num() - 1)]);
					if (FGPMDialog* D2 = Career->Current())
						if (D2->Result.Contains(TEXT("{")) || D2->FxText.Contains(TEXT("{"))) Bad(*St, FString::Printf(TEXT("résultat non rempli : %s | %s"), *D2->Result, *D2->FxText));
				}
				else { Career->Next(); if (!Career->Current()) GM->EndDialogs(); }
			}
			else GM->EndDialogs();
			return;
		case EP::Fired:
			// licencié : on le note et on continue la carrière pour tester la suite
			UE_LOG(LogGPM, Display, TEXT("TestSaison : licencié en %d (confiance %d), le test continue"), Season->Year(), G ? G->Conf : 0);
			Career->SecondChance();   // le repreneur garde le joueur
			GM->Phase = EP::Season;
			return;
		case EP::Mercato: case EP::Factory: GM->Phase = EP::Season; return;
		default: return;
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTestSeasonCmd(TEXT("GPM.TestSaison"), TEXT("Simule une carrière entière et vérifie la sauvegarde : GPM.TestSaison [saisons, 2 par défaut]."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		TSharedRef<GPMTestSeason::FState> St = MakeShared<GPMTestSeason::FState>();
		if (Args.Num()) St->Seasons = FMath::Clamp(FCString::Atoi(*Args[0]), 1, 30);
		TWeakObjectPtr<UWorld> W = World;
		World->GetTimerManager().SetTimer(St->Handle, FTimerDelegate::CreateLambda([W, St]() { GPMTestSeason::Step(W, St); }), 0.2f, true);
		UE_LOG(LogGPM, Display, TEXT("GPM.TestSaison : %d saison(s)"), St->Seasons);
	}));
