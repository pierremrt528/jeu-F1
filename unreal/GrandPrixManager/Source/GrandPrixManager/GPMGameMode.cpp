#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "GPMTrack.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GPMCar.h"
#include "GPMHUD.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

AGPMGameMode::AGPMGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ASpectatorPawn::StaticClass();   // remplacé par la monoplace dès que le circuit est prêt
	HUDClass = AGPMHUD::StaticClass();
}

void AGPMGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogGPM, Log, TEXT("Grand Prix Manager : démarrage"));

	SetupWorld();
	FString Id = UGameplayStatics::ParseOption(OptionsString, TEXT("circuit"));
	if (Id.IsEmpty()) Id = TEXT("valmont");
	LoadCircuit(Id);
}

void AGPMGameMode::LoadCircuit(const FString& Id)
{
	if (!Track) Track = GetWorld()->SpawnActor<AGPMTrack>();
	if (Track && Track->BuildCircuit(Id))
	{
		// Le joueur n'existe pas toujours encore au BeginPlay : on place la caméra à l'image suivante.
		GetWorldTimerManager().SetTimerForNextTick(this, &AGPMGameMode::PlaceCar);
	}
}

void AGPMGameMode::SetupWorld()
{
	// Soleil, ciel et brouillard (la carte de départ est vide). Matériaux et ambiance définitifs : étape 9.
	UWorld* W = GetWorld();
	if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-40.f, 35.f, 0.f)))
	{
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(8.f);
		if (UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Sun->GetLightComponent())) D->SetAtmosphereSunLight(true);
	}
	if (AActor* Sky = W->SpawnActor<AActor>())
	{
		USkyAtmosphereComponent* C = NewObject<USkyAtmosphereComponent>(Sky);
		Sky->SetRootComponent(C);
		C->RegisterComponent();
	}
	if (AExponentialHeightFog* Fog = W->SpawnActor<AExponentialHeightFog>())
		Fog->GetComponent()->SetFogDensity(0.004f);
}

void AGPMGameMode::PlaceCar()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !Track) return;
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	const FGPMTrackData* T = Data ? Data->GetTrack(Track->CircuitId) : nullptr;
	if (!T || !Data) return;

	// Une voiture par pilote de chaque écurie ; le joueur est le premier pilote de TEAMS[0]
	if (Cars.Num() == 0)
	{
		for (const FGPMTeam& Team : Data->Teams)
			for (int32 d = 0; d < Team.Drivers.Num(); ++d)
			{
				AGPMCar* C = GetWorld()->SpawnActor<AGPMCar>();
				if (!C) continue;
				C->SetLivery(Team.Color, Team.Accent);
				C->Perf = Team.Perf;
				C->DriverName = Team.Drivers[d].Name;
				C->TeamName = Team.Name;
				C->TeamColor = Team.Color;
				if (!Car && (Team.Drivers[d].bPlayer || &Team == &Data->Teams[0])) Car = C;
				else C->MakeAI(Team.Drivers[d].Skill);   // difficulté (DIFFS.k) : 1 en « Normal »
				Cars.Add(C);
			}
		if (!Car) return;
		APawn* Old = PC->GetPawn();
		PC->Possess(Car);
		if (Old && Old != Car) Old->Destroy();
	}
	Car->bPlayerControlled = !UGameplayStatics::HasOption(OptionsString, TEXT("demo"));   // ?demo : pilote automatique

	// Grille : IA rangées par niveau, le joueur en milieu de grille (?grille=N pour choisir)
	TArray<AGPMCar*> Grid;
	for (AGPMCar* C : Cars) if (C != Car) Grid.Add(C);
	Grid.Sort([](const AGPMCar& A, const AGPMCar& B) { return A.Skill * A.Perf > B.Skill * B.Perf; });
	const FString GridOpt = UGameplayStatics::ParseOption(OptionsString, TEXT("grille"));
	const int32 Slot = FMath::Clamp((GridOpt.IsEmpty() ? 10 : FCString::Atoi(*GridOpt)) - 1, 0, Grid.Num());
	Grid.Insert(Car, Slot);
	for (int32 k = 0; k < Grid.Num(); ++k)
	{
		Grid[k]->PlaceOnTrack(T, k);
		Grid[k]->Field = Grid;
		Grid[k]->bHold = true;
	}
	Order = Grid;
	for (AGPMCar* C : Grid) C->BestLap = 0.f;
	Phase = EPhase::Lights; LightsT = -1.f; Clock = DoneT = 0.f;
	GoAt = 4.2f + 0.4f + FMath::FRand() * 0.8f;
	// Nombre de tours : format Sprint (~11 min, lapsFor de strategy.js), ou ?tours=N
	LapEstimate = 0.f;
	for (int32 i = 0; i < T->AISpeed.Num(); ++i) LapEstimate += 2.f / FMath::Max(5.f, T->AISpeed[i]);
	LapEstimate *= 1.02f / FMath::Max(0.9f, Car->Perf);
	const FString ToursOpt = UGameplayStatics::ParseOption(OptionsString, TEXT("tours"));
	TotalLaps = ToursOpt.IsEmpty() ? FMath::Clamp(FMath::RoundToInt(11.f * 60.f / (LapEstimate * 1.12f)), 3, 40) : FMath::Max(1, FCString::Atoi(*ToursOpt));
	UE_LOG(LogGPM, Log, TEXT("Grille : %d voitures, joueur en P%d"), Grid.Num(), Slot + 1);
}

void AGPMGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Cars.Num() == 0) return;
	if (Phase == EPhase::Lights)
	{
		LightsT += DeltaSeconds;
		if (LightsT >= GoAt) { Phase = EPhase::Race; for (AGPMCar* C : Cars) C->bHold = false; }
	}
	else if (Phase == EPhase::Race) Clock += DeltaSeconds;
	Aspiration(DeltaSeconds);
	Collide();
	// arrivée : tour de plus que la distance de course
	for (AGPMCar* C : Cars)
		if (!C->bFinished && C->Lap > TotalLaps) { C->bFinished = true; C->FinishTime = Clock; }
	if (Phase == EPhase::Race && Car && Car->bFinished)
	{
		Car->bPlayerControlled = false;   // le pilote automatique ramène la voiture
		DoneT += DeltaSeconds;
		bool bAll = true;
		for (AGPMCar* C : Cars) bAll &= C->bFinished;
		if (DoneT > 4.f || bAll) Phase = EPhase::Done;
	}
	// classement : arrivés par temps, puis tours et distance parcourue
	Order.Sort([](const AGPMCar& A, const AGPMCar& B)
	{
		if (A.bFinished != B.bFinished) return A.bFinished;
		if (A.bFinished) return A.FinishTime < B.FinishTime;
		return A.Progress() > B.Progress();
	});
	if (Phase == EPhase::Done)
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->WasInputKeyJustPressed(EKeys::Enter)) Restart();
}

float AGPMGameMode::ResultTime(const AGPMCar* C) const
{
	if (C->bFinished) return C->FinishTime;
	const FGPMTrackData* T = C->GetTrack();
	const float Total = (TotalLaps + 1) * (T ? T->WX.Num() : 1);
	return Clock * Total / FMath::Max(1.f, C->Progress());
}

// Aspiration : voiture devant à moins de 45 m, alignée ; dans les 30 derniers mètres l'air sale coûte de l'appui
void AGPMGameMode::Aspiration(float Dt)
{
	for (AGPMCar* C : Cars)
	{
		float S = 0.f, Wk = 0.f;
		const float Fx = FMath::Sin(C->Heading), Fz = FMath::Cos(C->Heading);
		for (AGPMCar* O : Cars)
		{
			if (O == C) continue;
			const float Dx = O->X - C->X, Dz = O->Z - C->Z, Along = Dx * Fx + Dz * Fz;
			if (Along < 4 || Along > 45) continue;
			const float Side = FMath::Abs(Dx * -Fz + Dz * Fx);
			if (Side < 2.2f) S = FMath::Max(S, 1 - Along / 45);
			if (Side < 3.2f && Along < 30) Wk = FMath::Max(Wk, (1 - Along / 30) * (1 - Side / 4));
		}
		C->Slipstream += (S - C->Slipstream) * FMath::Min(1.f, Dt * 3);
		C->Wake += (Wk - C->Wake) * FMath::Min(1.f, Dt * 4);
	}
}

// Contacts entre voitures : deux cercles par voiture (avant / arrière), comme collide() de race.js (dégâts : étape 8)
void AGPMGameMode::Collide()
{
	const float Rad = 1.05f, Pts[2] = { 1.4f, -1.4f };
	for (int32 a = 0; a < Cars.Num(); ++a)
		for (int32 b = a + 1; b < Cars.Num(); ++b)
		{
			AGPMCar& A = *Cars[a];
			AGPMCar& B = *Cars[b];
			if (FMath::Abs(A.X - B.X) > 6 || FMath::Abs(A.Z - B.Z) > 6 || FMath::Abs(A.Y - B.Y) > 3) continue;
			const float FaX = FMath::Sin(A.Heading), FaZ = FMath::Cos(A.Heading), FbX = FMath::Sin(B.Heading), FbZ = FMath::Cos(B.Heading);
			for (float Pa : Pts) for (float Pb : Pts)
			{
				float Dx = (B.X + FbX * Pb) - (A.X + FaX * Pa), Dz = (B.Z + FbZ * Pb) - (A.Z + FaZ * Pa);
				const float D = FMath::Sqrt(Dx * Dx + Dz * Dz);
				if (D >= 2 * Rad || D == 0) continue;
				Dx /= D; Dz /= D;
				const float Push = (2 * Rad - D) / 2;
				A.X -= Dx * Push; A.Z -= Dz * Push; B.X += Dx * Push; B.Z += Dz * Push;
				const float An = A.V * (FaX * Dx + FaZ * Dz), Bn = B.V * (FbX * Dx + FbZ * Dz);
				const float Close = An - Bn;
				if (Close > 0)
				{
					const float K = Close * 0.55f;
					A.V -= K * (FaX * Dx + FaZ * Dz);
					B.V += K * (FbX * Dx + FbZ * Dz);
					if (Close > 5)
					{
						const float Kick = FMath::Min(0.5f, (Close - 5) * 0.03f) * (FMath::FRand() < 0.5f ? -1.f : 1.f);
						A.Beta += Kick * (Pa < 0 && !A.Assist.bStab ? 1.f : 0.3f);
						B.Beta += Kick * (Pb < 0 && !B.Assist.bStab ? 1.f : 0.3f);
					}
				}
			}
		}
}

// Commande console : GPM.Circuit sierra
static FAutoConsoleCommandWithWorldAndArgs GCircuitCmd(
	TEXT("GPM.Circuit"),
	TEXT("Charge un circuit : GPM.Circuit <id> (valmont, sierra, rocheval…). Sans argument : liste des circuits."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World) return;
		if (Args.Num() == 0)
		{
			if (UGPMDataSubsystem* Data = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UGPMDataSubsystem>() : nullptr)
				UE_LOG(LogGPM, Display, TEXT("Circuits : %s"), *FString::Join(Data->GetCircuitIds(), TEXT(", ")));
			return;
		}
		if (AGPMGameMode* GM = World->GetAuthGameMode<AGPMGameMode>()) GM->LoadCircuit(Args[0]);
	}));
