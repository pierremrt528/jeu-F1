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
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "GPMScenery.h"
#include "GPMSky.h"
#include "GPMDebris.h"
#include "GPMEngineSound.h"
#include "GPMSeason.h"
#include "GPMFactory.h"
#include "GPMShowroom.h"
#include "GPMCareer.h"
#include "Engine/Engine.h"
#include "RenderCore.h"
#include "RHI.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Dom/JsonValue.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "ShaderCompiler.h"

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
	LoadSettings();
	FString Id = UGameplayStatics::ParseOption(OptionsString, TEXT("circuit"));
	bCareer = Id.IsEmpty();   // sans ?circuit : la saison (22 GP, points, sauvegarde)
	// sans option de test : écran d'accueil (Carrière / Course solo / Réglages), le circuit de la manche en fond
	bBootMenu = bCareer && !UGameplayStatics::HasOption(OptionsString, TEXT("course"));
	Pending = bBootMenu ? 1 : 0;
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
	// Soleil (ombres dynamiques proches seulement), lumière d'ambiance capturée du ciel (dôme AGPMSky posé par ApplyTheme), brouillard, post-traitement léger
	UWorld* W = GetWorld();
	// 60 images par seconde au plus, sans « lissage » : le lissage du moteur restait bloqué vers 30 i/s après un à-coup
	if (GEngine) { GEngine->bSmoothFrameRate = false; GEngine->SetMaxFPS(60.f); }
	Sun = W->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-40.f, 35.f, 0.f));
	if (Sun)
	{
		UDirectionalLightComponent* D = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
		D->SetMobility(EComponentMobility::Movable);
		D->SetIntensity(8.f);
		D->SetLightColor(FLinearColor(1.f, 0.94f, 0.85f));   // soleil un peu chaud (0xfff0d8 du jeu web)
		D->SetCastShadows(true);
		D->SetDynamicShadowDistanceMovableLight(9000.f);   // 90 m : ombres des voitures proches
		D->SetDynamicShadowCascades(2);
	}
	SkyDome = W->SpawnActor<AGPMSky>();   // dégradé, soleil, nuages photo et montagnes (comme le jeu web)
	SkyLight = W->SpawnActor<ASkyLight>();
	if (SkyLight)
	{
		USkyLightComponent* L = SkyLight->GetLightComponent();
		L->SetMobility(EComponentMobility::Movable);
		L->bRealTimeCapture = false;   // capture unique du ciel (au chargement de chaque circuit) : beaucoup moins cher
		L->SetIntensity(1.f);
		L->bLowerHemisphereIsBlack = true;
		L->LowerHemisphereColor = FLinearColor(0.08f, 0.09f, 0.05f);   // lumière renvoyée par l'herbe (0x6a7448 de l'hémisphère du jeu web, atténué)
		L->RecaptureSky();
	}
	Debris = W->SpawnActor<AGPMDebris>();   // éclats de carbone et ailerons arrachés
	// atelier 3D des écrans de gestion : une pièce fermée 600 m sous le circuit, cachée pendant la course
	Showroom = W->SpawnActor<AGPMShowroom>(FVector(0.f, 0.f, -60000.f), FRotator::ZeroRotator);
	if (Showroom) { Showroom->Build(); Showroom->SetShown(false); }
	Fog = W->SpawnActor<AExponentialHeightFog>();
	if (Fog) Fog->GetComponent()->SetFogDensity(0.004f);
	if (APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>())
	{
		PP->bUnbound = true;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.35f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.32f;
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.3f;
		S.bOverride_LensFlareIntensity = true; S.LensFlareIntensity = 0.25f;
		// étalonnage « retransmission » : plus de contraste, couleurs moins criardes, lumière légèrement chaude,
		// ombres un peu bleutées
		S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.f);
		S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.1f, 1.1f, 1.1f, 1.f);
		S.bOverride_SceneColorTint = true; S.SceneColorTint = FLinearColor(1.f, 0.985f, 0.955f);
		S.bOverride_ColorGainShadows = true; S.ColorGainShadows = FVector4(0.96f, 0.99f, 1.05f, 1.f);
		S.bOverride_ColorSaturationHighlights = true; S.ColorSaturationHighlights = FVector4(0.92f, 0.92f, 0.92f, 1.f);
		// flou de vitesse léger : le décor file sur les côtés à haute vitesse
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = 0.3f;
		S.bOverride_MotionBlurMax = true; S.MotionBlurMax = 1.2f;
	}
	// image un peu plus nette (le lissage des bords adoucit les lignes fines : vibreurs, grillages, lignes blanches)
	if (IConsoleVariable* Sharpen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Tonemapper.Sharpen"))) Sharpen->Set(0.5f, ECVF_SetByCode);
}

// Ambiance du circuit (thème de data.js) : direction du soleil, brouillard, puis le décor
void AGPMGameMode::ApplyTheme(const FGPMTrackData* T)
{
	const FGPMTheme& Th = T->Theme;
	if (Sun) Sun->SetActorRotation((-Th.Sun).Rotation());
	if (Fog)
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		// fog [début, fin] du jeu web : voile qui commence à FogStart, ~60 % à FogEnd, total vers 4 km ; couleur de l'horizon
		// du ciel (raccord invisible). Au-delà de 12 km (dôme, nuages, montagnes) : pas de brouillard.
		FogBase = 17.4f / FMath::Max(400.f, Th.FogEnd - Th.FogStart);
		FogTint = Th.Horizon;
		F->SetFogDensity(FogBase);
		F->SetFogHeightFalloff(0.02f);
		F->SetStartDistance(Th.FogStart * 100.f);
		F->SetFogCutoffDistance(GPMSkyCfg::FogCutoff);
		F->SetFogInscatteringColor(FogTint);
		SkyGloomSet = -1.f;   // la grisaille de la pluie est réappliquée sur le nouveau ciel
		Fog->SetActorLocation(FVector(0, 0, T->Points.Num() ? T->Points[0].Z : 0.f));
	}
	// capture du ciel une fois le soleil placé (à l'image suivante, quand le ciel est à jour)
	if (SkyLight) GetWorldTimerManager().SetTimerForNextTick([this]() { if (SkyLight) SkyLight->GetLightComponent()->RecaptureSky(); });
	if (!SkyDome) SkyDome = GetWorld()->SpawnActor<AGPMSky>();
	if (SkyDome) SkyDome->Build(T);
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
				else C->MakeAI(Team.Drivers[d].Skill * AIDiff(AILevel));   // niveau choisi dans les réglages (DIFFS.k : 1 en « Normal »)
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
	RefreshRoster();
	ApplySettings();
	QOrder.Reset(); QTimes.Reset();
	bGridReady = false; SetupK = -1; Stints.Reset();
	QPB[0] = QPB[1] = QPB[2] = 0.f;
	WetSet = -1.f;
	Forecast();   // météo du nouveau week-end
	const int32 Then = Pending;
	Pending = 0;
	if (Then == 1) { GoHome(); return; }
	if (Then == 4 || Then == 5)
	{
		// week-end : réglages de la voiture (qualifs ensuite), ou directement la stratégie (course solo sans qualifs)
		StageGrid(); BaseUps();
		if (Then == 4) Phase = EPhase::Tuning; else OpenPrerace();
		return;
	}
	if (Then == 3) { StageGrid(); return; }   // écran de gestion : la grille en fond
	if (Then == 2)
	{
		// nouvelle écurie : bienvenue au QG (la voiture aux nouvelles couleurs), puis l'écran de saison
		StageGrid();
		Phase = EPhase::Season;
		if (UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>())
			CineSub = Career->Save() ? Career->Save()->TeamName : FString();
		StartCine(ECine::Welcome);
		return;
	}
	// Week-end : qualifications jouées (un tour lancé), puis la course ; ?course ou course solo sans qualifs : directement la course
	if (UGameplayStatics::HasOption(OptionsString, TEXT("course")) || (!bCareer && !bSoloQuali)) StartRace(); else StartQuali();
}

// noms, niveaux et couleurs des pilotes après la création de l'écurie ou un transfert
void AGPMGameMode::RefreshRoster()
{
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!Data) return;
	int32 k = 0;
	for (int32 t = 0; t < Data->Teams.Num(); ++t)
		for (int32 d = 0; d < Data->Teams[t].Drivers.Num(); ++d, ++k)
		{
			if (!Cars.IsValidIndex(k)) return;
			AGPMCar* C = Cars[k];
			const FGPMTeam& Tm = Data->Teams[t];
			C->DriverName = Tm.Drivers[d].Name;
			C->TeamName = Tm.Name;
			if (t == 0 && !C->TeamColor.Equals(Tm.Color)) C->SetLivery(Tm.Color, Tm.Accent);
			C->TeamColor = Tm.Color;
			C->Perf = Tm.Perf;
			if (C != Car) C->Skill = Tm.Drivers[d].Skill * AIDiff(AILevel);
		}
}

// la grille au complet, voitures arrêtées : fond des menus
void AGPMGameMode::StageGrid()
{
	const FGPMTrackData* T = CurTrack();
	if (!T) return;
	TArray<AGPMCar*> All;
	if (QOrder.Num() == Cars.Num()) All = QOrder;   // après les qualifications : l'ordre de la grille
	else for (AGPMCar* C : Cars) All.Add(C);
	for (int32 k = 0; k < All.Num(); ++k)
	{
		Park(All[k], false);
		All[k]->PlaceOnTrack(T, k);
		All[k]->bHold = true;
		All[k]->Field = All;
	}
	Order = All;
}

// Fluidité : résolution adaptative (comme le jeu web) pour tenir 60 i/s, et i/s notés dans le journal
void AGPMGameMode::UpdatePerf(float Dt)
{
	if (Dt <= 0.f || Dt > 0.5f) return;
	FrameAvg = FrameAvg * 0.95f + Dt * 0.05f;
	Fps = 1.f / FMath::Max(0.001f, FrameAvg);
	PerfT += Dt; PerfLogT += Dt;
	if (PerfT >= 1.5f)
	{
		PerfT = 0.f;
		// résolution adaptative : seulement quand la carte graphique est le goulot (baisser la résolution n'aide pas sinon)
		const float GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		const bool bGpuKnown = GpuMs > 0.1f, bGpuBound = !bGpuKnown || GpuMs > 15.f;
		int32 Want = ScreenPct;
		if (Fps < 50.f && bGpuBound) Want -= 10;
		else if (Fps < 56.f && bGpuBound) Want -= 5;
		else if (Fps > 58.f || (bGpuKnown && GpuMs < 11.f)) Want += 5;
		Want = FMath::Clamp(Want, 55, 100);
		if (Want != ScreenPct)
		{
			ScreenPct = Want;
			if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) CV->Set(float(ScreenPct), ECVF_SetByCode);
		}
	}
	if (PerfLogT >= 15.f)
	{
		PerfLogT = 0.f;
		// temps de l'image par fil (jeu, rendu, carte graphique) pour savoir ce qui ralentit
		UE_LOG(LogGPM, Log, TEXT("Fluidité : %.0f i/s, résolution %d %% · jeu %.1f ms, rendu %.1f ms, GPU %.1f ms"), Fps, ScreenPct,
			FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	}
}

void AGPMGameMode::LoadSettings()
{
	if (!GConfig) return;
	GConfig->GetBool(TEXT("GPM"), TEXT("ABS"), bSetABS, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("GPM"), TEXT("TC"), bSetTC, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("GPM"), TEXT("Stab"), bSetStab, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("GPM"), TEXT("Cockpit"), bSetCockpit, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("GPM"), TEXT("Long"), bLongRace, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("GPM"), TEXT("Sound"), bSetSound, GGameUserSettingsIni);
	GConfig->GetInt(TEXT("GPM"), TEXT("AILevel"), AILevel, GGameUserSettingsIni);
	GConfig->GetInt(TEXT("GPM"), TEXT("Fuel"), FuelK, GGameUserSettingsIni);
	AILevel = FMath::Clamp(AILevel, 0, 4);
	FuelK = FMath::Clamp(FuelK, 0, 4);
}

// niveau des IA : le rythme de chaque pilote IA suit le nouveau coefficient tout de suite
void AGPMGameMode::SetAILevel(int32 L)
{
	const int32 Was = AILevel;
	AILevel = FMath::Clamp(L, 0, 4);
	if (Was == AILevel) return;
	for (AGPMCar* C : Cars) if (C && C != Car) C->Skill *= AIDiff(AILevel) / AIDiff(Was);
	SaveSettings();
}

// ---------- carburant ----------

float AGPMGameMode::FuelFor(const AGPMCar* C, float Laps) const
{
	const FGPMTrackData* T = CurTrack();
	const float Km = T ? T->WX.Num() * 2.f / 1000.f : 5.f;
	return FMath::Max(0.f, Laps) * Km * 1.7f * (C ? C->UpFuel : 1.f);
}

float AGPMGameMode::PlayerStartFuel(int32 K) const
{
	static const float Margin[5] = { -0.08f, -0.04f, 0.03f, 0.08f, 0.f };
	const int32 Kc = FMath::Clamp(K, 0, 4);
	if (Kc == 4) return Tank;
	const int32 First = Stints.Num() > 1 ? Stints[0].Value : LapsFor();
	return FMath::Min(Tank, FuelFor(Car.Get(), First + 0.3f) * (1.f + Margin[Kc]));
}

float AGPMGameMode::LapsToRefuel(const AGPMCar* C) const
{
	if (!C) return 0.f;
	const FGPMTrackData* T = C->GetTrack();
	const int32 N = T ? FMath::Max(1, T->WX.Num()) : 1;
	const TArray<TPair<TCHAR, int32>>& Pl = C == Car.Get() ? Stints : C->Plan;
	int32 Upto = TotalLaps;
	if (Pl.IsValidIndex(C->Stops + 1))
	{
		int32 Planned = 0;
		for (int32 s = 0; s <= C->Stops; ++s) Planned += Pl[s].Value;
		Upto = FMath::Min(TotalLaps, Planned);
	}
	return FMath::Max(0.f, Upto - C->Lap + 1 - float(C->Idx) / N);
}

void AGPMGameMode::SaveSettings()
{
	if (GConfig)
	{
		GConfig->SetBool(TEXT("GPM"), TEXT("ABS"), bSetABS, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("GPM"), TEXT("TC"), bSetTC, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("GPM"), TEXT("Stab"), bSetStab, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("GPM"), TEXT("Cockpit"), bSetCockpit, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("GPM"), TEXT("Long"), bLongRace, GGameUserSettingsIni);
		GConfig->SetBool(TEXT("GPM"), TEXT("Sound"), bSetSound, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("GPM"), TEXT("AILevel"), AILevel, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("GPM"), TEXT("Fuel"), FuelK, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	ApplySettings();
}

void AGPMGameMode::ApplySettings()
{
	if (!Car) return;
	Car->Assist.bABS = bSetABS; Car->Assist.bTC = bSetTC; Car->Assist.bStab = bSetStab;
	if (Car->IsCockpit() != bSetCockpit) Car->ToggleCamera();
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
	BaseUps(); ApplySetup();
	if (SC) { for (AGPMCar* C : Cars) C->Field.Remove(SC); SC->Destroy(); SC = nullptr; }
	for (AGPMCar* C : Cars) if (C != Car) Park(C, true);
	Park(Car, false);
	Car->bPlayerControlled = !UGameplayStatics::HasOption(OptionsString, TEXT("demo"));
	const int32 N = T->WX.Num(), I = (N - 150) % N;
	Car->PlaceOnTrack(T, 0);
	Car->Idx = I; Car->X = T->WX[I]; Car->Z = T->WZ[I]; Car->Y = T->WY[I]; Car->Heading = T->H[I];
	Car->Lat = Car->Lane = 0.f; Car->V = 45.f; Car->bHold = false;
	Car->Field = { Car.Get() };
	ResetWeather();
	if (Debris) Debris->Clear();
	// tendres pour le tour lancé, intermédiaires ou pneus pluie sur piste mouillée
	const TCHAR QWet = WetTyreFor(Car);
	Car->Tyre = QWet ? QWet : TCHAR('S'); Car->Wear = 0.f; Car->Fuel = 10.f; Car->Mass = 1.f + Car->Fuel / 750.f;
	Car->GripMul = AGPMCar::WetGrip(Car->Tyre, TrackWet);
	Order = { Car.Get() };
	// secteurs : records perso du week-end (gardés quand on refait le tour), temps de référence le long du tour
	if (QTimes.Num() == 0) QPB[0] = QPB[1] = QPB[2] = 0.f;
	for (int32 k = 0; k < 3; ++k) { QSec[k] = 0.f; QCol[k] = 0; QPB0[k] = QPB[k]; }
	QSplit[0] = QSplit[1] = 0.f;
	QCum.SetNumZeroed(N + 1);
	for (int32 i = 0; i < N; ++i) QCum[i + 1] = QCum[i] + 2.f / FMath::Max(5.f, T->AISpeed.IsValidIndex(i) ? T->AISpeed[i] : 40.f);
	if (QTimes.Num() == 0)
		for (AGPMCar* C : Cars)
			if (C != Car) QTimes.Add(C, LapTimeFor(T, C->Skill, C->Perf) * (1.f + 0.006f * (FMath::FRand() - 0.3f)) * 0.985f * WetLapK());
	RadioT = 0.f;
	Say(bRaining ? TEXT("Qualifications sous la pluie : un tour lancé, pneus pluie. Doucement sur les freins !") : TEXT("Qualifications : un tour lancé. Donne tout !"));
}

void AGPMGameMode::EndQuali()
{
	Phase = EPhase::QualiDone;
	bGridReady = true;
	const float Mine = Car->LastLap;
	// 3e secteur et records perso
	if (QSplit[1] > 0.f && Mine > QSplit[1]) { QSec[2] = Mine - QSplit[1]; QCol[2] = SectorColour(2); }
	for (int32 k = 0; k < 3; ++k) if (QSec[k] > 0.f && (QPB[k] <= 0.f || QSec[k] < QPB[k])) QPB[k] = QSec[k];
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
	BaseUps(); ApplySetup();
	Car->bPlayerControlled = !UGameplayStatics::HasOption(OptionsString, TEXT("demo"));   // ?demo : pilote automatique

	// Grille : ordre des qualifications ; sinon IA rangées par niveau et joueur en milieu de grille (?grille=N)
	TArray<AGPMCar*> Grid;
	int32 Slot = 0;
	if (QOrder.Num() == Cars.Num())
	{
		Grid = QOrder; Slot = Grid.IndexOfByKey(Car);
		// pénalité de grille (événement « moteur changé »)
		UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
		if (bCareer && Career && Career->Save() && Career->Save()->GridPen > 0)
		{
			const int32 To = FMath::Min(Grid.Num() - 1, Slot + Career->Save()->GridPen);
			Grid.Remove(Car); Grid.Insert(Car, To); Slot = To;
			Say(FString::Printf(TEXT("Pénalité : %d places sur la grille"), Career->Save()->GridPen));
			Career->Save()->GridPen = 0;
		}
	}
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
	TotalLaps = LapsFor();
	GridSlot = Slot;
	// pneus et carburant au départ : assez d'essence pour la course (+3 %), gomme selon la durée
	const FString TyreOpt = UGameplayStatics::ParseOption(OptionsString, TEXT("pneus"));
	const float Km = T->WX.Num() * 2.f / 1000.f;
	// météo remise au départ, débris effacés ; chaque IA prend une des stratégies du muret (pneus pluie sur piste mouillée)
	ResetWeather();
	if (Debris) Debris->Clear();
	ManualTyre = 0; bTyrePicked = false;
	const TArray<FPlan> AIPlans = MakePlans();
	for (AGPMCar* C : Grid)
	{
		C->Plan.Reset();
		if (AIPlans.Num()) C->Plan = AIPlans[FMath::RandRange(0, AIPlans.Num() - 1)].Stints;
		// essence pour le premier relais (+4 %), ravitaillement à chaque arrêt (comme le jeu web)
		C->Fuel = FMath::Min(Tank, FuelFor(C, (C->Plan.Num() > 1 ? C->Plan[0].Value : TotalLaps) + 0.3f) * 1.04f);
		C->Mass = 1.f + C->Fuel / 750.f;
		C->Tyre = C->Plan.Num() ? C->Plan[0].Key : bRainStart ? RainTyre() : TCHAR('M');
		C->WetCall = 0.25f + 0.08f * (FMath::FRand() - 0.5f);
		C->GripMul = AGPMCar::WetGrip(C->Tyre, TrackWet);
	}
	Car->Plan.Reset();
	Car->Tyre = !TyreOpt.IsEmpty() ? TCHAR(FChar::ToUpper(TyreOpt[0])) : Stints.Num() ? Stints[0].Key : bRainStart ? RainTyre() : TCHAR('M');
	Car->WetCall = 0.25f;
	Car->GripMul = AGPMCar::WetGrip(Car->Tyre, TrackWet);
	// carburant choisi par le joueur à l'écran de stratégie
	Car->Fuel = PlayerStartFuel(FuelK);
	Car->Mass = 1.f + Car->Fuel / 750.f;
	FuelWarn = 0;
	bTyreWarn = false; if (RadioT < 4.f) RadioT = 0.f;   // garde le message de pénalité de grille
	UE_LOG(LogGPM, Log, TEXT("Grille : %d voitures, joueur en P%d"), Grid.Num(), Slot + 1);
	ApplySettings();
	// survol du circuit avant les feux (une fois par Grand Prix, pas quand on recommence)
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	const FString Key = T->Id + (bCareer && Season ? FString::FromInt(Season->Round()) : FString(TEXT("solo")));
	if (!bNoIntro && !IntroSeen.Contains(Key) && !UGameplayStatics::HasOption(OptionsString, TEXT("course")))
	{
		IntroSeen.Add(Key);
		Phase = EPhase::Intro;
		CineTitle = T->Name;
		CineSub = bCareer && Season ? FString::Printf(TEXT("Grand Prix %d / %d · %d tours"), Season->Round() + 1, Season->NumRounds(), TotalLaps) : FString::Printf(TEXT("Course solo · %d tours"), TotalLaps);
		StartCine(ECine::Intro);
	}
}

void AGPMGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePerf(DeltaSeconds);
	if (Cars.Num() == 0) return;
	UpdateAudio(DeltaSeconds);
	// grande refonte du règlement : monoplaces de la nouvelle ère
	if (UGPMFactorySubsystem* Fac = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>()) Fac->Trees();
	if (const UGPMDataSubsystem* DataE = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		if (DataE->CarEra != CarsEra) { CarsEra = DataE->CarEra; for (AGPMCar* C : Cars) if (C) C->SyncModel(); }
	UpdateRoom();
	UpdateCine(DeltaSeconds);
	UpdateSky(DeltaSeconds);
	AutoCapture(DeltaSeconds);
	// pointeur de la souris visible dans les menus et les écrans de résultats (sur ordinateur), caché en course
	if (APlayerController* PCur = GetWorld()->GetFirstPlayerController())
	{
		const bool bCursor = InMenu() || Phase == EPhase::QualiDone || Phase == EPhase::Done || Phase == EPhase::Intro || Phase == EPhase::Outro;
		if (bool(PCur->bShowMouseCursor) != bCursor)
		{
			PCur->bShowMouseCursor = bCursor;
			PCur->bEnableClickEvents = bCursor;
			if (bCursor)
			{
				FInputModeGameAndUI Mode;
				Mode.SetHideCursorDuringCapture(false);
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PCur->SetInputMode(Mode);
			}
			else PCur->SetInputMode(FInputModeGameOnly());
		}
	}
	if (InMenu())
	{
		if (Cine == ECine::None) StartCine(ECine::Menu);
		// contrat signé pendant une négociation : poignée de main et photo dans l'atelier
		if (UGPMCareerSubsystem* CS = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>())
			if (CS->bJustSigned) { CS->bJustSigned = false; StartSign(CS->Nego.D.Name); }
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			// scènes de l'atelier : Entrée, Espace ou Échap pour passer
			if (RoomCine() && (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Escape)))
			{
				EndCine();
				StartCine(ECine::Menu);
			}
			else if (Phase == EPhase::Season && PC->WasInputKeyJustPressed(EKeys::Enter)) Continue();
			else if ((Phase == EPhase::Season || Phase == EPhase::Factory) && PC->WasInputKeyJustPressed(EKeys::U)) Phase = Phase == EPhase::Season ? EPhase::Factory : EPhase::Season;
			else if ((Phase == EPhase::Factory || Phase == EPhase::Mercato) && PC->WasInputKeyJustPressed(EKeys::Escape)) Phase = EPhase::Season;
			else if ((Phase == EPhase::Solo || Phase == EPhase::Settings || Phase == EPhase::Setup) && PC->WasInputKeyJustPressed(EKeys::Escape)) Phase = EPhase::Home;
			else if ((Phase == EPhase::Tuning || Phase == EPhase::Prerace) && PC->WasInputKeyJustPressed(EKeys::Escape)) Phase = bCareer ? EPhase::Season : EPhase::Solo;
		}
		return;
	}
	if (Phase == EPhase::Intro || Phase == EPhase::Outro)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::SpaceBar)) EndCine();
		if (Phase == EPhase::Intro) return;   // la course attend la fin du survol
	}
	if (Phase == EPhase::Quali || Phase == EPhase::QualiDone)
	{
		RadioT -= DeltaSeconds;
		if (Phase == EPhase::Quali) UpdateSectors();
		if (Phase == EPhase::Quali && Car && Car->Lap >= 2) EndQuali();
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->WasInputKeyJustPressed(EKeys::Enter)) { if (Phase == EPhase::QualiDone) OpenPrerace(); }   // stratégie de course
			else if (Phase == EPhase::QualiDone && PC->WasInputKeyJustPressed(EKeys::BackSpace)) StartQuali();   // refaire le tour
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
	{
		if (PC->WasInputKeyJustPressed(EKeys::B)) ToggleBox();
		// radio du muret : 1 (ou I) point complet, 2 (ou M) rythme, 3 (ou N) pneus du prochain arrêt
		if (DigitPressed(PC, 1) || PC->WasInputKeyJustPressed(EKeys::I)) RadioCall(0);
		if (DigitPressed(PC, 2) || PC->WasInputKeyJustPressed(EKeys::M)) RadioCall(1);
		if (DigitPressed(PC, 3) || PC->WasInputKeyJustPressed(EKeys::N)) RadioCall(2);
	}
	// arrivée : tour de plus que la distance de course
	for (AGPMCar* C : Cars)
		if (!C->bFinished && C->Lap > TotalLaps) { C->bFinished = true; C->FinishTime = Clock; }
	if (Phase == EPhase::Race && Car && Car->bFinished)
	{
		Car->bPlayerControlled = false;   // le pilote automatique ramène la voiture
		DoneT += DeltaSeconds;
		bool bAll = true;
		for (AGPMCar* C : Cars) bAll &= C->bFinished;
		if (DoneT > 4.f || bAll)
		{
			Phase = EPhase::Done;
			RecordResult();
			// tour d'honneur autour de la voiture après un podium (confettis pour une victoire)
			const int32 P = PositionOf(Car);
			if (P >= 1 && P <= 3)
			{
				CineTitle = P == 1 ? TEXT("VICTOIRE !") : FString::Printf(TEXT("PODIUM : P%d"), P);
				CineSub = CurTrack() ? CurTrack()->Name : FString();
				Phase = EPhase::Outro;
				StartCine(ECine::Podium);
			}
		}
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

// Une capture d'écran par écran et par partie (interface comprise), écrasée à la partie suivante : Saved/Captures/<écran>.png
void AGPMGameMode::AutoCapture(float Dt)
{
	static const TCHAR* Names[] = { TEXT("Qualifs"), TEXT("QualifsFin"), TEXT("Feux"), TEXT("Course"), TEXT("Arrivee"), TEXT("Saison"), TEXT("Usine"),
		TEXT("Accueil"), TEXT("Creation"), TEXT("Solo"), TEXT("Reglages"), TEXT("Presse"), TEXT("Mercato"), TEXT("ReglagesVoiture"), TEXT("Strategie"),
		TEXT("Licencie"), TEXT("Survol"), TEXT("Podium") };
	static_assert(int32(UE_ARRAY_COUNT(Names)) == int32(EPhase::Outro) + 1, "un nom par phase");
	if (Phase != CapPhase) { CapPhase = Phase; CapT = 0.f; }
	CapT += Dt;
	// premier lancement : les matériaux se compilent quelques secondes (damier gris) ; on attend, puis on recapture le ciel
	const bool bBusy = GShaderCompilingManager && GShaderCompilingManager->IsCompiling();
	if (bShadersBusy && !bBusy && SkyLight) SkyLight->GetLightComponent()->RecaptureSky();
	bShadersBusy = bBusy;
	if (bBusy) return;
	FString Name = Names[int32(Phase)];
	float Wait = 1.5f;
	if (Phase == EPhase::Race) { Wait = 12.f; if (CapT > 45.f) { Name += TEXT("2"); Wait = 45.f; } }
	else if (Phase == EPhase::Quali) Wait = 8.f;
	else if (Phase == EPhase::Intro || Phase == EPhase::Outro) Wait = 3.f;
	if (Phase == EPhase::Setup) Name += FString::FromInt(SetupStep + 1);
	float At = CapT;
	if (RoomCine())
	{
		// scènes de l'atelier : bienvenue, arrivée du pilote et de l'agent, poignée de main puis photo
		At = CineT;
		Name = Cine == ECine::Welcome ? TEXT("Bienvenue") : Cine == ECine::Meet ? TEXT("RendezVous") : TEXT("Signature");
		Wait = Cine == ECine::Welcome ? 2.5f : Cine == ECine::Meet ? 4.1f : 1.3f;
		if (Cine == ECine::Sign && CineT > 3.3f) { Name += TEXT("2"); Wait = 3.3f; }
	}
	if (At < Wait || Captured.Contains(Name)) return;
	Captured.Add(Name);
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") / Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(File, true, false);
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
		Car->NextTyre = NextStopTyre(Car);
		Say(FString::Printf(TEXT("Box à la fin du tour : %s."), *TyreName(Car->NextTyre)));
	}
	else Say(TEXT("Arrêt annulé."));
}

// Usure des pneus, carburant, météo, dégâts, arrêts (updateStrategy de strategy.js)
void AGPMGameMode::Strategy(float Dt)
{
	UpdateWeather(Dt);
	for (AGPMCar* C : Cars)
	{
		const FGPMTrackData* T = C->GetTrack();
		if (!T) continue;
		// chocs de l'image (murs, contacts) : dégâts sur l'aileron touché
		if (C->DmgNew > 0.f) { AddDamage(C, C->DmgNew, false); C->DmgNew = 0.f; }
		if (C->DmgNewR > 0.f) { AddDamage(C, C->DmgNewR, true); C->DmgNewR = 0.f; }
		const bool bAuto = C != Car || !C->bPlayerControlled;
		const int32 Left = TotalLaps - C->Lap, NPts = FMath::Max(1, T->WX.Num());
		// IA : économie si l'essence ne suffit pas jusqu'au prochain ravitaillement, sinon rythme normal
		const float PerLapFuel = NPts * 2.f / 1000.f * 1.7f * C->UpFuel;
		const float FuelLapsLeft = PerLapFuel > 0.f ? C->Fuel / PerLapFuel : 99.f;
		if (bAuto && !C->bFinished)
		{
			C->Mode = FuelLapsLeft < LapsToRefuel(C) + 0.15f ? 2 : 1;
		}
		const float Len = NPts * 2.f, Dist = FMath::Abs(C->V) * Dt;
		if (!C->PitPhase)
		{
			// usure : distance, glissades (fortement), sorties de piste, mode ; les pneus pluie fondent sur le sec
			const float Slide = FMath::Abs(C->Beta) > 0.05f ? FMath::Min(1.f, FMath::Abs(C->Beta) * 4.f) : 0.f;
			float K = (1.f + 3.5f * Slide + (C->bOffTrack ? 1.5f : 0.f)) * (C == Car ? 1.f : 0.92f) * AGPMCar::ModeWear(C->Mode) * C->UpTyre;
			K *= AGPMCar::WetWear(C->Tyre, TrackWet);
			C->Wear += Dist / Len * AGPMCar::TyreWpl(C->Tyre) * K;
			if (!C->bFinished) C->Fuel = FMath::Max(0.f, C->Fuel - Dist / 1000.f * 1.7f * C->UpFuel * AGPMCar::ModeFuel(C->Mode));
			// pneu à 0 % : il peut éclater à tout moment, retour au stand au ralenti
			if (!C->bFlat && C->Wear >= 1.f && FMath::FRand() < Dt * (0.03f + 0.4f * (C->Wear - 1.f)))
			{
				C->bFlat = true;
				if (Left >= 1 && !C->bFinished) { C->bBox = true; C->NextTyre = NextStopTyre(C); }
				if (C == Car) Say(TEXT("Muret : crevaison ! Rentre au stand doucement, on change les pneus."));
				else Say(FString::Printf(TEXT("Muret : crevaison pour %s !"), *C->DriverName));
			}
		}
		C->Mass = 1.f + C->Fuel / 750.f;
		// panne sèche : le moteur ne tourne plus qu'au ralenti jusqu'à l'arrivée (ou au stand)
		const bool bDry = C->Fuel <= 0.f && !C->bFinished && Phase == EPhase::Race;
		C->PowerMul = AGPMCar::ModeV(C->Mode) * (C->bFlat ? 0.8f : 1.f) * (bDry ? 0.35f : 1.f);
		C->AccMul = AGPMCar::ModeA(C->Mode) * (bDry ? 0.3f : 1.f);
		if (C == Car && !C->bFinished && Phase == EPhase::Race)
		{
			if (bDry && FuelWarn < 2) { FuelWarn = 2; Say(TEXT("Muret : panne sèche ! Plus d'essence, la voiture n'avance plus qu'au ralenti.")); }
			else if (FuelWarn == 0 && C->Mode != 2 && C->Lap >= 1 && FuelLapsLeft < LapsToRefuel(C) - 0.05f)
			{
				FuelWarn = 1;
				Say(TEXT("Muret : l'essence ne suffira pas, passe en économie (radio, touche 2)."));
			}
		}
		C->GripMul = AGPMCar::WetGrip(C->Tyre, TrackWet) * AGPMCar::LifeGrip(1.f - C->Wear) * (C->bFlat ? 0.55f : 1.f) * (1.f - 0.04f * C->Dmg - 0.03f * C->DmgR);
		// appui perdu : aileron avant → sous-virage, aileron arrière → survirage à haute vitesse
		C->DmgKd = C->bFwOff ? 0.3f : 1.f - 0.45f * C->Dmg;
		C->RwKd = C->bRwOff ? 0.3f : 1.f - 0.45f * C->DmgR;
		if (C == Car && !bTyreWarn && C->Wear > 0.6f && !C->PitPhase) { bTyreWarn = true; Say(TEXT("Muret : tes pneus passent sous 40 %, ils n'ont presque plus de grip.")); }

		// décision d'arrêt avant l'entrée des stands
		const int32 K0 = T->PitK(C->Idx);
		const bool bZone = K0 > T->PitE0 - 30 && K0 < T->PitE0 - 10;
		if (K0 > 200 && K0 < 210) C->bAdvised = false;   // nouveau tour : le muret peut reconseiller
		if (!bZone || C->PitPhase || C->Lap < 1 || Left < 1 || C->bFinished) continue;
		// arrêt prévu par la stratégie (le joueur : celle de l'écran de stratégie, le muret l'appelle au bon tour)
		const TArray<TPair<TCHAR, int32>>& Pl = C == Car ? Stints : C->Plan;
		if (!C->bBox && Pl.IsValidIndex(C->Stops + 1))
		{
			int32 Planned = 0;
			for (int32 s = 0; s <= C->Stops; ++s) Planned += Pl[s].Value;
			if (C->Lap >= Planned) { C->bBox = true; C->NextTyre = NextStopTyre(C); }
		}
		// IA : pluie, piste sèche, crevaison, pneus à bout, aileron cassé
		if (bAuto && !C->bBox && WantBox(C)) { C->bBox = true; C->NextTyre = NextStopTyre(C); }
		// joueur : conseil du muret, une fois par tour
		if (!bAuto && !C->bBox && !C->bAdvised && WantBox(C))
		{
			C->bAdvised = true;
			Say(FString::Printf(TEXT("Muret : rentre ce tour (touche B) : pneus %s%s."), *TyreName(NextStopTyre(C)), C->Dmg > 0.5f || C->DmgR > 0.5f ? TEXT(" et nouvel aileron") : TEXT("")));
		}
		if (C->bBox)
		{
			const float Fix = (C->Dmg > 0.15f ? 3.f : 0.f) + (C->DmgR > 0.15f ? 4.f : 0.f) + (C->bFwOff || C->bRwOff ? 2.f : 0.f);
			// pièces Stands et Centre des mécaniciens : l'équipe du joueur gagne du temps à chaque arrêt
			const float Crew = Car && C->TeamIdx == Car->TeamIdx ? Car->UpPit : 0.f;
			C->StartPit(FMath::Max(1.4f, 2.3f + FMath::FRand() * 0.6f + Crew) + Fix);
			// ravitaillement : de quoi aller jusqu'à l'arrêt suivant de la stratégie, ou jusqu'à l'arrivée
			{
				const TArray<TPair<TCHAR, int32>>& Next = C == Car ? Stints : C->Plan;
				int32 Upto = TotalLaps;
				if (Next.IsValidIndex(C->Stops + 2))
				{
					int32 Planned = 0;
					for (int32 s = 0; s <= C->Stops + 1; ++s) Planned += Next[s].Value;
					Upto = FMath::Min(TotalLaps, Planned);
				}
				C->Fuel = FMath::Max(C->Fuel, FMath::Min(Tank, FuelFor(C, FMath::Max(1, Upto - C->Lap) + 0.3f) * 1.04f));
				if (C == Car) FuelWarn = 0;
			}
			if (C == Car)
			{
				bTyreWarn = false;
				ManualTyre = 0; bTyrePicked = false;
				Say(FString::Printf(TEXT("Muret : box, box ! Pneus %s%s."), *TyreName(C->NextTyre), Fix > 0.f ? TEXT(" et réparation") : TEXT("")));
			}
		}
	}
}

// Saison : points du Grand Prix (une fois par course), puis écran de saison et GP suivant
void AGPMGameMode::RecordResult()
{
	if (!bCareer || bRecorded) return;
	bRecorded = true;
	bGridReady = false; SetupK = -1; Stints.Reset();   // prochain Grand Prix : nouveau week-end
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!Season) return;
	TArray<FGPMFinish> Fin;
	for (const AGPMCar* C : Order) Fin.Add({ C->DriverName, C->TeamName, C->BestLap, C == Car });
	const int32 Pos = Order.IndexOfByKey(Car) + 1;
	LastPts = Season->RecordRace(Fin);
	UGPMSaveGame* G = Season->State();
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	if (!G || !Factory) return;
	// gains (racePrize de team.js) + part des sponsors ; moral et réputation suivent les résultats
	static const float Prize[10] = { 5.f, 4.f, 3.4f, 2.8f, 2.4f, 2.1f, 1.8f, 1.6f, 1.4f, 1.2f };
	LastPartner = Factory->PartnerIncome();
	LastPrize = (Pos >= 1 && Pos <= 10 ? Prize[Pos - 1] : 0.8f) * (0.8f + G->Rep / 250.f) * Factory->PrizeMul() + LastPartner;
	Factory->AddMoney(LastPrize);
	G->Moral = FMath::Clamp(G->Moral + (Pos <= 3 ? 6 : Pos <= 10 ? 2 : -3), 0, 100);
	G->Rep = FMath::Clamp(G->Rep + (Pos <= 3 ? 2 : Pos <= 10 ? 1 : 0), 0, 100);
	// les écuries rivales progressent un peu à chaque course
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		for (int32 t = 1; t < Data->Teams.Num(); ++t)
			Factory->RivalGain(Data->Teams[t].Name, (0.0012f + FMath::FRand() * 0.0018f) * 0.67f * Factory->RivalDevMul(Data->Teams[t].Name));
	// l'usine avance jusqu'au GP suivant (GAPS de season.js, en jours)
	G->News.Reset();
	int32 Days = 30;
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		if (const TSharedPtr<FJsonValue> Gaps = Data->GetRaw(TEXT("season"), TEXT("GAPS")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = Gaps->AsArray();
			if (A.IsValidIndex(G->Round - 1)) Days = int32(A[G->Round - 1]->AsNumber());
		}
	Factory->Tick(Days);
	const FString Built = Factory->TickInfra(Days);   // chantier des installations
	Factory->RotateStaff();                            // chefs libres renouvelés
	Factory->RecordPerf();                             // courbes de performance
	// vie de l'écurie : direction, sponsors, puis conférence de presse et flash info
	LastLines.Reset();
	if (UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>())
	{
		Career->ClearBoost();
		FGPMRaceCtx R;
		R.Pos = Pos; R.Grid = GridSlot + 1; R.Stops = Car->Stops; R.Pen = Car->Penalty; R.bSC = bSCUsed;
		R.Circuit = CurTrack() ? CurTrack()->Name : FString();
		R.Winner = Order.Num() ? (Order[0] == Car ? FString(TEXT("Toi")) : Order[0]->DriverName) : FString();
		const int32 RivalIdx = Pos >= 2 ? Pos - 2 : 1;
		if (Order.IsValidIndex(RivalIdx)) R.Rival = Order[RivalIdx]->DriverName;
		float Best = 1e9f;
		for (const AGPMCar* C : Order) if (C->BestLap > 0.f) Best = FMath::Min(Best, C->BestLap);
		R.bFastest = Car->BestLap > 0.f && Car->BestLap <= Best;
		for (int32 k = 0; k < Order.Num(); ++k) if (Order[k] != Car && Order[k]->TeamIdx == Car->TeamIdx) R.MatePos = k + 1;
		for (int32 k = 0; k < Order.Num(); ++k) if (!G->MisWho.IsEmpty() && Order[k]->DriverName == G->MisWho) R.RivalPos = k + 1;
		LastLines = Career->AfterRace(R);
		if (!Built.IsEmpty()) LastLines.Add(Built);
		// règlement : rumeurs de refonte, règlement de l'an prochain
		TArray<TPair<FString, FString>> RegMails;
		Factory->EraAfterRace(G->Round, Season->NumRounds(), Season->IsOver(), RegMails, LastLines);
		Career->RaceMails(R, LastLines);
		for (const TPair<FString, FString>& M : RegMails) Career->SendMail(TEXT("reglement"), TEXT("Instance dirigeante du championnat"), M.Key, M.Value);
	}
	Season->Save();
}

// Carrière : pièces de l'usine sur la voiture du joueur, progrès des rivaux sur la leur
void AGPMGameMode::ApplyCareer()
{
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	if (!bCareer || !Data || !Season || !Factory || !Season->State()) return;
	const FGPMUpgrades U = Factory->Upgrades();
	Car->UpVTop = U.VTop; Car->UpAcc = U.Acc; Car->UpMu = U.Mu; Car->UpBrake = U.Brake; Car->UpKd = U.Kd; Car->UpSteer = U.Steer; Car->UpFuel = U.Fuel;
	Car->UpTyre = U.Tyre; Car->UpPit = U.Pit;
	// bonus de la presse et des événements pour cette course, moral (±1 %), consommation du motoriste
	if (UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>())
	{
		Car->UpVTop *= 1.f + Career->Boost(TEXT("vtop"));
		Car->UpAcc *= 1.f + Career->Boost(TEXT("acc"));
		Car->UpMu *= 1.f + Career->Boost(TEXT("mu"));
		Car->UpBrake *= 1.f + Career->Boost(TEXT("brake"));
		Car->UpKd *= 1.f + Career->Boost(TEXT("kd"));
		Car->UpSteer *= 1.f + Career->Boost(TEXT("steer"));
		if (const UGPMSaveGame* G = Career->Save()) { Car->UpFuel *= G->Fuel; Car->Perf = Data->Teams[0].Perf * (1.f + (G->Moral - 60) / 4000.f); }
	}
	for (AGPMCar* C : Cars)
		if (C != Car && Data->Teams.IsValidIndex(C->TeamIdx))
			C->Perf = Data->Teams[C->TeamIdx].Perf + Season->State()->RivalDev.FindRef(Data->Teams[C->TeamIdx].Name);
}

void AGPMGameMode::Continue()
{
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (Phase == EPhase::Done)
	{
		if (!bCareer) { Restart(); return; }
		// après la course : conférence de presse et flash info, puis l'écran de saison
		bNegoFromMercato = false;
		if (Career && Career->Current()) { Phase = EPhase::Press; return; }
		EndDialogs();
		return;
	}
	if (Phase == EPhase::Season && Season)
	{
		if (Season->IsOver())
		{
			// intersaison : nouvelle saison, puis l'écran de saison sur le premier circuit
			if (Career) Career->Intersaison();
			// grande refonte du règlement (nouvelle ère) ou règlement annuel, puis la nouvelle saison
			FString RegSubj, RegBody, PubSubj, PubBody;
			if (UGPMFactorySubsystem* Fac = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>())
			{
				Fac->Intersaison(Season->Year() + 1, RegSubj, RegBody);
				Season->NewSeason(true);
				Fac->EraPublished(PubSubj, PubBody);
			}
			else Season->NewSeason(true);
			if (Career && !RegSubj.IsEmpty()) Career->SendMail(TEXT("reglement"), TEXT("Instance dirigeante du championnat"), RegSubj, RegBody);
			if (Career && !PubSubj.IsEmpty()) Career->SendMail(TEXT("reglement"), TEXT("Instance dirigeante du championnat"), PubSubj, PubBody);
			Season->Save();
			bChampShown = false;
			bGridReady = false; SetupK = -1; Stints.Reset();
			IntroSeen.Reset();
			Pending = 3;
			LoadCircuit(Season->CurrentCircuit());
			return;
		}
		if (bGridReady) OpenPrerace(); else OpenTuning();
	}
}

void AGPMGameMode::EndDialogs()
{
	if (bNegoFromMercato) { Phase = EPhase::Mercato; return; }
	Phase = EPhase::Season;
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (Career && Season && Season->IsOver()) LastLines.Append(Career->SeasonReview());
	if (Career && Career->IsFired()) { Phase = EPhase::Fired; return; }
	// le circuit du prochain Grand Prix en fond de l'écran de saison
	if (Season && Track && Track->CircuitId != Season->CurrentCircuit()) { Pending = 3; LoadCircuit(Season->CurrentCircuit()); }
}

// ---------- menus ----------
void AGPMGameMode::GoHome()
{
	StageGrid();
	Phase = EPhase::Home;
	StartCine(ECine::Menu);
}

void AGPMGameMode::NewCareer()
{
	if (UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>()) Career->Pick = FGPMPick();
	SetupStep = 0;
	Phase = EPhase::Setup;
}

void AGPMGameMode::FinishSetup()
{
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!Career || !Season) return;
	Career->CreateCareer();
	bCareer = true; bChampShown = false;
	IntroSeen.Reset();
	Pending = 2;
	Phase = EPhase::Season;
	LoadCircuit(Season->CurrentCircuit());
}

void AGPMGameMode::ContinueCareer()
{
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!Career || !Season || !Career->IsCreated()) { NewCareer(); return; }
	bCareer = true;
	LastLines.Reset();
	Phase = Career->IsFired() ? EPhase::Fired : EPhase::Season;
	if (Track && Track->CircuitId != Season->CurrentCircuit()) { Pending = 3; LoadCircuit(Season->CurrentCircuit()); }
}

void AGPMGameMode::StartSolo()
{
	UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>();
	if (!Data) return;
	const TArray<FString> Ids = Data->GetCircuitIds();
	if (!Ids.IsValidIndex(SoloIdx)) return;
	bCareer = false;
	EndCine();
	Phase = EPhase::Quali;
	Pending = bSoloQuali ? 4 : 5;
	LoadCircuit(Ids[SoloIdx]);
}

void AGPMGameMode::OpenMercato()
{
	MercatoPage = 0;
	bNegoFromMercato = true;
	Phase = EPhase::Mercato;
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
						// dégâts sur l'aileron touché (avant ou arrière), seulement sur un nouveau contact franc
						if (A.ContactT <= 0.f) (Pa > 0 ? A.DmgNew : A.DmgNewR) += Close * 0.01f;
						if (B.ContactT <= 0.f) (Pb > 0 ? B.DmgNew : B.DmgNewR) += Close * 0.01f;
						const float Kick = FMath::Min(0.5f, (Close - 5) * 0.03f) * (FMath::FRand() < 0.5f ? -1.f : 1.f);
						A.Beta += Kick * (Pa < 0 && !A.Assist.bStab ? 1.f : 0.3f);
						B.Beta += Kick * (Pb < 0 && !B.Assist.bStab ? 1.f : 0.3f);
					}
					A.ContactT = B.ContactT = 0.6f;
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
