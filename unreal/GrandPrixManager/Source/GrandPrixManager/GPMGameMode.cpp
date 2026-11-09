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
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "GPMScenery.h"
#include "GPMEngineSound.h"
#include "GPMSeason.h"
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
	bCareer = Id.IsEmpty();   // sans ?circuit : la saison (22 GP, points, sauvegarde)
	if (bCareer)
		if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>()) Id = Season->CurrentCircuit();
	if (Id.IsEmpty()) Id = TEXT("valmont");
	LoadCircuit(Id);
}

void AGPMGameMode::LoadCircuit(const FString& Id)
{
	if (!Track) Track = GetWorld()->SpawnActor<AGPMTrack>();
	if (Track && Track->BuildCircuit(Id))
	{
		if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
			if (const FGPMTrackData* T = Data->GetTrack(Id)) ApplyTheme(T);
		// Le joueur n'existe pas toujours encore au BeginPlay : on place la caméra à l'image suivante.
		GetWorldTimerManager().SetTimerForNextTick(this, &AGPMGameMode::PlaceCar);
	}
}

void AGPMGameMode::SetupWorld()
{
	// Soleil (ombres dynamiques proches seulement), ciel physique, lumière d'ambiance capturée du ciel, brouillard, post-traitement léger
	UWorld* W = GetWorld();
	Sun = W->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-40.f, 35.f, 0.f));
	if (Sun)
	{
		UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
		D->SetMobility(EComponentMobility::Movable);
		D->SetIntensity(8.f);
		D->SetAtmosphereSunLight(true);
		D->SetCastShadows(true);
		D->SetDynamicShadowDistanceMovableLight(9000.f);   // 90 m : ombres des voitures proches
		D->SetDynamicShadowCascades(2);
	}
	if (AActor* Sky = W->SpawnActor<AActor>())
	{
		USkyAtmosphereComponent* C = NewObject<USkyAtmosphereComponent>(Sky);
		Sky->SetRootComponent(C);
		C->RegisterComponent();
	}
	SkyLight = W->SpawnActor<ASkyLight>();
	if (SkyLight)
	{
		USkyLightComponent* L = SkyLight->GetLightComponent();
		L->SetMobility(EComponentMobility::Movable);
		L->bRealTimeCapture = true;
		L->SetIntensity(1.f);
		L->RecaptureSky();
	}
	Fog = W->SpawnActor<AExponentialHeightFog>();
	if (Fog) Fog->GetComponent()->SetFogDensity(0.004f);
	if (APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>())
	{
		PP->bUnbound = true;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.35f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.25f;
		S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.08f, 1.08f, 1.08f, 1.f);
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.3f;
	}
}

// Ambiance du circuit (thème de data.js) : direction du soleil, brouillard, puis le décor
void AGPMGameMode::ApplyTheme(const FGPMTrackData* T)
{
	const FGPMTheme& Th = T->Theme;
	if (Sun) Sun->SetActorRotation((-Th.Sun).Rotation());
	if (Fog)
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		F->SetFogDensity(0.004f * 1500.f / FMath::Max(600.f, Th.FogEnd));
		F->SetStartDistance(Th.FogStart * 30.f);
		F->SetFogInscatteringColor(Th.Horizon * 0.6f);
		Fog->SetActorLocation(FVector(0, 0, T->Points.Num() ? T->Points[0].Z : 0.f));
	}
	if (SkyLight) SkyLight->GetLightComponent()->RecaptureSky();
	if (!Scenery) Scenery = GetWorld()->SpawnActor<AGPMScenery>();
	if (Scenery) Scenery->Build(T);
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
				C->TeamIdx = Data->Teams.IndexOfByPredicate([&Team](const FGPMTeam& X) { return &X == &Team; });
				if (!Car && (Team.Drivers[d].bPlayer || &Team == &Data->Teams[0])) Car = C;
				else C->MakeAI(Team.Drivers[d].Skill);   // difficulté (DIFFS.k) : 1 en « Normal »
				Cars.Add(C);
			}
		if (!Car) return;
		APawn* Old = PC->GetPawn();
		PC->Possess(Car);
		// son du moteur du joueur (généré, sans fichier audio)
		EngineSound = NewObject<UGPMEngineSound>(Car);
		if (EngineSound)
		{
			EngineSound->SetupAttachment(Car->GetRootComponent());
			EngineSound->RegisterComponent();
			EngineSound->Start();
		}
		if (Old && Old != Car) Old->Destroy();
	}
	// Week-end : qualifications jouées (un tour lancé), puis la course ; ?course : directement la course
	QOrder.Reset(); QTimes.Reset();
	if (UGameplayStatics::HasOption(OptionsString, TEXT("course"))) StartRace(); else StartQuali();
}

const FGPMTrackData* AGPMGameMode::CurTrack() const
{
	UGPMDataSubsystem* Data = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	return Data && Track ? Data->GetTrack(Track->CircuitId) : nullptr;
}

// Estimation du tour (lapEstimate de season.js) : profil de vitesse × niveau, accélération limitée par la vitesse de pointe
float AGPMGameMode::LapTimeFor(const FGPMTrackData* T, float Skill, float Perf)
{
	const int32 N = T->AISpeed.Num();
	if (N == 0) return 90.f;
	const float VTop = AGPMCar::VTop * Perf;
	float V = T->AISpeed[0] * Skill, Tm = 0.f;
	for (int32 Pass = 0; Pass < 2; ++Pass)
		for (int32 i = 0; i < N; ++i)
		{
			const float A = AGPMCar::Acc * Perf * FMath::Max(0.f, 1.f - FMath::Pow(V / VTop, 2.2f));
			V = FMath::Min(T->AISpeed[(i + 1) % N] * Skill, FMath::Sqrt(V * V + 2.f * A * 2.f));
			if (Pass == 1) Tm += 2.f / FMath::Max(V, 5.f);
		}
	return Tm;
}

void AGPMGameMode::Park(AGPMCar* C, bool bPark)
{
	C->bParked = bPark;
	C->SetActorHiddenInGame(bPark);
	C->SetActorTickEnabled(!bPark);
}

// Qualifications (qsession.js) : la voiture du joueur seule, tour lancé depuis 300 m avant la ligne ; temps des IA estimés
void AGPMGameMode::StartQuali()
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return;
	Phase = EPhase::Quali;
	if (SC) { for (AGPMCar* C : Cars) C->Field.Remove(SC); SC->Destroy(); SC = nullptr; }
	for (AGPMCar* C : Cars) if (C != Car) Park(C, true);
	Park(Car, false);
	Car->bPlayerControlled = !UGameplayStatics::HasOption(OptionsString, TEXT("demo"));
	const int32 N = T->WX.Num(), I = (N - 150) % N;
	Car->PlaceOnTrack(T, 0);
	Car->Idx = I; Car->X = T->WX[I]; Car->Z = T->WZ[I]; Car->Y = T->WY[I]; Car->Heading = T->H[I];
	Car->Lat = Car->Lane = 0.f; Car->V = 45.f; Car->bHold = false;
	Car->Field = { Car.Get() };
	Car->Tyre = 'S'; Car->Wear = 0.f; Car->Fuel = 10.f; Car->Mass = 1.f + Car->Fuel / 750.f; Car->GripMul = AGPMCar::TyreGrip('S');
	Order = { Car.Get() };
	if (QTimes.Num() == 0)
		for (AGPMCar* C : Cars)
			if (C != Car) QTimes.Add(C, LapTimeFor(T, C->Skill, C->Perf) * (1.f + 0.006f * (FMath::FRand() - 0.3f)) * 0.985f);
	RadioT = 0.f;
	Say(TEXT("Qualifications : un tour lancé. Donne tout !"));
}

void AGPMGameMode::EndQuali()
{
	Phase = EPhase::QualiDone;
	const float Mine = Car->LastLap;
	if (float* Prev = QTimes.Find(Car)) { if (Mine > 0.f && Mine < *Prev) *Prev = Mine; }
	else QTimes.Add(Car, Mine > 0.f ? Mine : 999.f);
	QTimes.GenerateKeyArray(QOrder);
	QOrder.Sort([this](const AGPMCar& A, const AGPMCar& B) { return QTimes.FindRef(const_cast<AGPMCar*>(&A)) < QTimes.FindRef(const_cast<AGPMCar*>(&B)); });
	Order = QOrder;
}

void AGPMGameMode::StartRace()
{
	const FGPMTrackData* T = CurTrack();
	if (!T || !Car) return;
	for (AGPMCar* C : Cars) Park(C, false);
	Car->bPlayerControlled = !UGameplayStatics::HasOption(OptionsString, TEXT("demo"));   // ?demo : pilote automatique

	// Grille : ordre des qualifications ; sinon IA rangées par niveau et joueur en milieu de grille (?grille=N)
	TArray<AGPMCar*> Grid;
	int32 Slot = 0;
	if (QOrder.Num() == Cars.Num()) { Grid = QOrder; Slot = Grid.IndexOfByKey(Car); }
	else
	{
		for (AGPMCar* C : Cars) if (C != Car) Grid.Add(C);
		Grid.Sort([](const AGPMCar& A, const AGPMCar& B) { return A.Skill * A.Perf > B.Skill * B.Perf; });
		const FString GridOpt = UGameplayStatics::ParseOption(OptionsString, TEXT("grille"));
		Slot = FMath::Clamp((GridOpt.IsEmpty() ? 10 : FCString::Atoi(*GridOpt)) - 1, 0, Grid.Num());
		Grid.Insert(Car, Slot);
	}
	for (int32 k = 0; k < Grid.Num(); ++k)
	{
		Grid[k]->PlaceOnTrack(T, k);
		Grid[k]->Field = Grid;
		Grid[k]->bHold = true;
	}
	Order = Grid;
	for (AGPMCar* C : Grid) C->BestLap = 0.f;
	Phase = EPhase::Lights; LightsT = -1.f; Clock = DoneT = 0.f; bRecorded = false;
	if (SC) { for (AGPMCar* C : Cars) C->Field.Remove(SC); SC->Destroy(); SC = nullptr; }
	bSCUsed = false; SCAhead.Reset();
	GoAt = 4.2f + 0.4f + FMath::FRand() * 0.8f;
	// Nombre de tours : format Sprint (~11 min, lapsFor de strategy.js), ou ?tours=N
	LapEstimate = 0.f;
	for (int32 i = 0; i < T->AISpeed.Num(); ++i) LapEstimate += 2.f / FMath::Max(5.f, T->AISpeed[i]);
	LapEstimate *= 1.02f / FMath::Max(0.9f, Car->Perf);
	const FString ToursOpt = UGameplayStatics::ParseOption(OptionsString, TEXT("tours"));
	TotalLaps = ToursOpt.IsEmpty() ? FMath::Clamp(FMath::RoundToInt(11.f * 60.f / (LapEstimate * 1.12f)), 3, 40) : FMath::Max(1, FCString::Atoi(*ToursOpt));
	// pneus et carburant au départ : assez d'essence pour la course (+3 %), gomme selon la durée
	const FString TyreOpt = UGameplayStatics::ParseOption(OptionsString, TEXT("pneus"));
	const float Km = T->WX.Num() * 2.f / 1000.f;
	for (AGPMCar* C : Grid)
	{
		C->Fuel = FMath::Min(110.f, (TotalLaps + 0.3f) * Km * 1.7f * 1.03f);
		C->Mass = 1.f + C->Fuel / 750.f;
		C->Tyre = FMath::FRand() < 0.4f ? 'S' : 'M';
		C->GripMul = AGPMCar::TyreGrip(C->Tyre);
	}
	Car->Tyre = TyreOpt.IsEmpty() ? 'M' : TCHAR(FChar::ToUpper(TyreOpt[0]));
	bTyreWarn = false; RadioT = 0.f;
	UE_LOG(LogGPM, Log, TEXT("Grille : %d voitures, joueur en P%d"), Grid.Num(), Slot + 1);
}

void AGPMGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Cars.Num() == 0) return;
	if (EngineSound && Car)
	{
		const float Skid = FMath::Clamp((FMath::Abs(Car->Beta) - 0.04f) * 6.f + (Car->bLock ? 0.6f : 0.f), 0.f, 1.f) * FMath::Min(1.f, Car->V / 15.f);
		EngineSound->SetState(Car->Rpm(), Car->Throttle, Skid, Car->V);
	}
	if (Phase == EPhase::Season)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->WasInputKeyJustPressed(EKeys::Enter)) Continue();
		return;
	}
	if (Phase == EPhase::Quali || Phase == EPhase::QualiDone)
	{
		RadioT -= DeltaSeconds;
		if (Phase == EPhase::Quali && Car && Car->Lap >= 2) EndQuali();
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->WasInputKeyJustPressed(EKeys::Enter)) StartRace();                                    // à la course
			else if (PC->WasInputKeyJustPressed(EKeys::BackSpace)) StartQuali();                           // refaire le tour
		}
		return;
	}
	if (Phase == EPhase::Lights)
	{
		LightsT += DeltaSeconds;
		if (LightsT >= GoAt) { Phase = EPhase::Race; for (AGPMCar* C : Cars) C->bHold = false; }
	}
	else if (Phase == EPhase::Race) Clock += DeltaSeconds;
	Aspiration(DeltaSeconds);
	Collide();
	if (Phase == EPhase::Race)
	{
		Strategy(DeltaSeconds);
		if (!bSCUsed && UGameplayStatics::HasOption(OptionsString, TEXT("sc")) && Clock > 20.f) RequestSafety(true);
		UpdateSafety(DeltaSeconds);
	}
	RadioT -= DeltaSeconds;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (PC->WasInputKeyJustPressed(EKeys::B)) ToggleBox();
	// arrivée : tour de plus que la distance de course
	for (AGPMCar* C : Cars)
		if (!C->bFinished && C->Lap > TotalLaps) { C->bFinished = true; C->FinishTime = Clock; }
	if (Phase == EPhase::Race && Car && Car->bFinished)
	{
		Car->bPlayerControlled = false;   // le pilote automatique ramène la voiture
		DoneT += DeltaSeconds;
		bool bAll = true;
		for (AGPMCar* C : Cars) bAll &= C->bFinished;
		if (DoneT > 4.f || bAll) { Phase = EPhase::Done; RecordResult(); }
	}
	// classement : arrivés par temps, puis tours et distance parcourue
	Order.Sort([](const AGPMCar& A, const AGPMCar& B)
	{
		if (A.bFinished != B.bFinished) return A.bFinished;
		if (A.bFinished) return A.FinishTime + A.Penalty < B.FinishTime + B.Penalty;
		return A.Progress() > B.Progress();
	});
	if (Phase == EPhase::Done)
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->WasInputKeyJustPressed(EKeys::Enter)) Continue();
}

void AGPMGameMode::RequestSafety(bool bForce)
{
	if (SC || bSCUsed || Phase != EPhase::Race || Order.Num() == 0) return;
	const AGPMCar* Lead = Order[0];
	const FGPMTrackData* T = Lead->GetTrack();
	if (!T) return;
	if (!bForce && (Lead->Progress() / float(T->WX.Num()) - 1.f) / TotalLaps > 0.8f) return;   // pas dans le dernier cinquième
	if (!bForce && FMath::FRand() > 0.35f) return;
	bSCUsed = true;
	SC = GetWorld()->SpawnActor<AGPMCar>();
	if (!SC) return;
	SC->SetLivery(FLinearColor(0.8f, 0.82f, 0.85f), FLinearColor(0.1f, 0.75f, 0.25f));
	SC->Perf = 0.5f;
	SC->DriverName = TEXT("Sécurité");
	SC->MakeAI(0.56f);
	SC->PlaceOnTrack(T, 0);
	// 220 m devant le premier, au milieu de la piste, déjà lancée
	const int32 N = T->WX.Num(), I = (Lead->Idx + 110) % N;
	SC->Idx = I; SC->X = T->WX[I]; SC->Z = T->WZ[I]; SC->Y = T->WY[I]; SC->Heading = T->H[I];
	SC->Lat = SC->Lane = 0.f; SC->V = 40.f; SC->Lap = 1000;   // jamais comptée à l'arrivée
	SCT = 0.f; SCEnding = 0.f;
	SCAhead.Reset();
	for (AGPMCar* C : Order) { if (C == Car) break; SCAhead.Add(C); }
	for (AGPMCar* C : Cars) C->Field.Add(SC);   // le peloton se cale derrière elle
	Say(TEXT("Muret : voiture de sécurité ! Ralentis, interdiction de dépasser."));
}

void AGPMGameMode::UpdateSafety(float Dt)
{
	for (AGPMCar* C : Cars) C->bSCMode = SafetyOut();
	if (!SC) return;
	SCT += Dt;
	// fin : peloton regroupé (moins de 22 m par voiture), ou au bout de 75 s
	const float Spread = Order.Num() > 1 ? (Order[0]->Progress() - Order.Last()->Progress()) * 2.f : 0.f;
	if (SCEnding <= 0.f && ((SCT > 25.f && Spread < Order.Num() * 22.f) || SCT > 75.f))
	{
		SCEnding = 4.f;
		Say(TEXT("Muret : la voiture de sécurité rentre. Prépare la relance !"));
	}
	if (SCEnding > 0.f)
	{
		SCEnding -= Dt;
		if (SCEnding <= 0.f)
		{
			for (AGPMCar* C : Cars) C->Field.Remove(SC);
			SC->Destroy(); SC = nullptr; SCAhead.Reset();
			Say(TEXT("Muret : fin de la voiture de sécurité, c'est reparti !"));
		}
		return;
	}
	// joueur : pénalité s'il passe une voiture qui était devant lui
	if (!Car || Car->PitPhase) { SCAhead.Reset(); return; }
	for (auto It = SCAhead.CreateIterator(); It; ++It)
	{
		AGPMCar* C = *It;
		if (C->PitPhase) { It.RemoveCurrent(); continue; }
		if (Car->Progress() - C->Progress() > 4.f)
		{
			It.RemoveCurrent();
			if (Car->bPlayerControlled) { Car->Penalty += 5.f; Say(TEXT("Muret : dépassement sous voiture de sécurité, pénalité de 5 s !")); }
		}
	}
}

// gomme la plus tendre qui tient jusqu'à l'arrivée sans passer sous 40 % (MAXW = 0,6)
TCHAR AGPMGameMode::PickTyre(int32 LapsLeft) const
{
	for (TCHAR T : { TCHAR('S'), TCHAR('M') }) if (LapsLeft * AGPMCar::TyreWpl(T) * 1.05f <= 0.6f) return T;
	return 'H';
}

void AGPMGameMode::ToggleBox()
{
	if (!Car || Car->PitPhase || Phase == EPhase::Done) return;
	Car->bBox = !Car->bBox;
	if (Car->bBox)
	{
		Car->NextTyre = PickTyre(TotalLaps - Car->Lap);
		Say(FString::Printf(TEXT("Box à la fin du tour : %s."), *TyreName(Car->NextTyre)));
	}
	else Say(TEXT("Arrêt annulé."));
}

// Usure des pneus, carburant, arrêts (updateStrategy de strategy.js, sans la météo)
void AGPMGameMode::Strategy(float Dt)
{
	for (AGPMCar* C : Cars)
	{
		const FGPMTrackData* T = C->GetTrack();
		if (!T) continue;
		const float Len = T->WX.Num() * 2.f, Dist = FMath::Abs(C->V) * Dt;
		if (!C->PitPhase)
		{
			// usure : distance, glissades (fortement), sorties de piste
			const float Slide = FMath::Abs(C->Beta) > 0.05f ? FMath::Min(1.f, FMath::Abs(C->Beta) * 4.f) : 0.f;
			const float K = (1.f + 3.5f * Slide + (C->bOffTrack ? 1.5f : 0.f)) * (C == Car ? 1.f : 0.92f);
			C->Wear += Dist / Len * AGPMCar::TyreWpl(C->Tyre) * K;
			if (!C->bFinished) C->Fuel = FMath::Max(0.f, C->Fuel - Dist / 1000.f * 1.7f);
		}
		C->Mass = 1.f + C->Fuel / 750.f;
		C->GripMul = AGPMCar::TyreGrip(C->Tyre) * AGPMCar::LifeGrip(1.f - C->Wear) * (C->Wear >= 1.f ? 0.7f : 1.f);
		if (C == Car && !bTyreWarn && C->Wear > 0.6f) { bTyreWarn = true; Say(TEXT("Muret : tes pneus passent sous 40 %, ils n'ont presque plus de grip.")); }

		// décision d'arrêt avant l'entrée des stands
		const int32 K0 = T->PitK(C->Idx), Left = TotalLaps - C->Lap;
		const bool bZone = K0 > T->PitE0 - 30 && K0 < T->PitE0 - 10;
		if (!bZone || C->PitPhase || C->Lap < 1 || Left < 1 || C->bFinished) continue;
		const bool bAuto = C != Car || !C->bPlayerControlled;
		if (bAuto && !C->bBox && (1.f - C->Wear) - Left * AGPMCar::TyreWpl(C->Tyre) * 1.05f < 0.38f)
		{
			C->bBox = true;
			C->NextTyre = PickTyre(Left);
		}
		if (C->bBox)
		{
			C->StartPit(FMath::Max(1.4f, 2.3f + FMath::FRand() * 0.6f));
			if (C == Car) { bTyreWarn = false; Say(FString::Printf(TEXT("Muret : box, box ! Pneus %s."), *TyreName(C->NextTyre))); }
		}
	}
}

// Saison : points du Grand Prix (une fois par course), puis écran de saison et GP suivant
void AGPMGameMode::RecordResult()
{
	if (!bCareer || bRecorded) return;
	bRecorded = true;
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!Season) return;
	TArray<FGPMFinish> Fin;
	for (const AGPMCar* C : Order) Fin.Add({ C->DriverName, C->TeamName, C->BestLap, C == Car });
	LastPts = Season->RecordRace(Fin);
}

void AGPMGameMode::Continue()
{
	if (Phase == EPhase::Done)
	{
		if (bCareer) Phase = EPhase::Season; else Restart();
		return;
	}
	if (Phase == EPhase::Season)
	{
		UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
		if (!Season) return;
		if (Season->IsOver()) Season->NewSeason(true);
		Phase = EPhase::Quali;   // évite un double appui pendant le chargement
		LoadCircuit(Season->CurrentCircuit());
	}
}

float AGPMGameMode::ResultTime(const AGPMCar* C) const
{
	if (C->bFinished) return C->FinishTime + C->Penalty;
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
			if (O == C || O->PitPhase) continue;
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
			if (A.PitPhase || B.PitPhase) continue;
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
					if (Close > 14) RequestSafety(false);
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

// Commande console : GPM.SC (voiture de sécurité tout de suite)
static FAutoConsoleCommandWithWorldAndArgs GSafetyCmd(
	TEXT("GPM.SC"),
	TEXT("Fait sortir la voiture de sécurité."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (AGPMGameMode* GM = World ? World->GetAuthGameMode<AGPMGameMode>() : nullptr) GM->RequestSafety(true);
	}));
