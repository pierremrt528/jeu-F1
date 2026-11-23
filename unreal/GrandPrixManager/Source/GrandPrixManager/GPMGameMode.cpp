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
#include "GPMRain.h"
#include "Camera/PlayerCameraManager.h"
#include "GPMEngineSound.h"
#include "GPMSeason.h"
#include "GPMFactory.h"
#include "GPMShowroom.h"
#include "GPMCareer.h"
#include "Engine/Engine.h"
#include "RenderCore.h"
#include "RHI.h"
#include "RHIStats.h"
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
	Rain3D = W->SpawnActor<AGPMRain>();     // gerbes d'eau derrière les voitures, gouttes autour de la caméra
	// atelier 3D des écrans de gestion : une pièce fermée 600 m sous le circuit, cachée pendant la course
	Showroom = W->SpawnActor<AGPMShowroom>(FVector(0.f, 0.f, -60000.f), FRotator::ZeroRotator);
	if (Showroom) { Showroom->Build(); Showroom->SetShown(false); }
	Fog = W->SpawnActor<AExponentialHeightFog>();
	if (Fog) Fog->GetComponent()->SetFogDensity(0.004f);
	if (APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>())
	{
		PP->bUnbound = true;
		PPVol = PP;
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
	// Fluidité (mesurée avec GPM.TestPerf sur un MacBook Air M3) : le lissage TSR des réglages « Épique » coûtait ~4 ms
	// et faisait tomber la résolution à 55 % (image floue). Lissage FXAA en pleine résolution, ombres sur 2 cascades.
	auto SetCV = [](const TCHAR* Name, const TCHAR* Value)
	{
		if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(Name)) CV->Set(Value, ECVF_SetByCode);
	};
	SetCV(TEXT("r.AntiAliasingMethod"), TEXT("1"));
	SetCV(TEXT("r.FXAA.Quality"), TEXT("4"));
	SetCV(TEXT("r.SSR.Quality"), TEXT("0"));
	SetCV(TEXT("r.LensFlareQuality"), TEXT("0"));
	SetCV(TEXT("r.ScreenPercentage"), TEXT("100"));
	ScreenPct = 100;
	ApplyGraphics();   // ombres, occlusion, lissage selon la qualité choisie dans les réglages
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
	PerfWorst = FMath::Max(PerfWorst, Dt);
	if (Dt > 0.025f) ++PerfHitch;
	if (PerfT >= 1.5f && !bPerfTest)
	{
		PerfT = 0.f;
		// résolution adaptative : seulement quand la carte graphique est le goulot (baisser la résolution n'aide pas sinon)
		const float GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		const bool bGpuKnown = GpuMs > 0.1f, bGpuBound = !bGpuKnown || GpuMs > 15.f;
		int32 Want = ScreenPct;
		if (Fps < 50.f && bGpuBound) Want -= 10;
		else if (Fps < 56.f && bGpuBound) Want -= 5;
		else if (Fps > 58.f || (bGpuKnown && GpuMs < 11.f)) Want += 5;
		Want = FMath::Clamp(Want, 75, 100);   // sous 75 %, l'image devient trop floue
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
		UE_LOG(LogGPM, Log, TEXT("Fluidité : %.0f i/s, résolution %d %% · jeu %.1f ms, rendu %.1f ms, GPU %.1f ms · %d à-coups, pire image %.0f ms · %d appels de dessin"), Fps, ScreenPct,
			FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),
			PerfHitch, PerfWorst * 1000.f, GNumDrawCallsRHI[0]);
		PerfHitch = 0; PerfWorst = 0.f;
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
	GConfig->GetInt(TEXT("GPM"), TEXT("NegoLevel"), UGPMCareerSubsystem::NegoLevel, GGameUserSettingsIni);
	GConfig->GetInt(TEXT("GPM"), TEXT("CareerLen"), UGPMCareerSubsystem::CareerLen, GGameUserSettingsIni);
	GConfig->GetInt(TEXT("GPM"), TEXT("Gfx"), GfxLevel, GGameUserSettingsIni);
	GfxLevel = FMath::Clamp(GfxLevel, 0, 2);
	UGPMCareerSubsystem::NegoLevel = FMath::Clamp(UGPMCareerSubsystem::NegoLevel, 0, 2);
	AGPMCar::LoadAIOptions();
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
		GConfig->SetInt(TEXT("GPM"), TEXT("NegoLevel"), UGPMCareerSubsystem::NegoLevel, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("GPM"), TEXT("CareerLen"), UGPMCareerSubsystem::CareerLen, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("GPM"), TEXT("Gfx"), GfxLevel, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	ApplyGraphics();
	ApplySettings();
}

void AGPMGameMode::ApplySettings()
{
	if (!Car) return;
	Car->Assist.bABS = bSetABS; Car->Assist.bTC = bSetTC; Car->Assist.bStab = bSetStab;
	// caméra au départ : la dernière choisie (touche C) ; la case « cockpit au départ » des réglages l'emporte quand on la change
	int32 Want = bSetCockpit ? 2 : 0, Saved = -1;
	if (GConfig) GConfig->GetInt(TEXT("GPM"), TEXT("CamMode"), Saved, GGameUserSettingsIni);
	if (Saved >= 0 && Saved <= 2 && (Saved == 2) == bSetCockpit) Want = Saved;
	if (Car->GetCamMode() != Want) Car->SetCamMode(Want);
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
	ClearGates(); MisReset();
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
	bQInvalid = false;
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
	const float Mine = bQInvalid ? 0.f : Car->LastLap;   // limites de piste franchies : le tour ne compte pas
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
	ClearGates();
	PStyleSum = PStyleDist = 0.f;
	RaceStyle.Reset();
	RaceLapSeen = WearMsgLap = 0; StintMsgStop = -1; LapWearR = LapFuelR = 0.f;
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
	RedState = 0; bRedUsed = bYellowPrev = bYellow = bBlueFlag = false; Yellows.Reset(); YellowAhead.Reset(); BlueT = 0.f; BlueWarn = CrashRun = 0; SCCall = -1; CrashAt = -100.f;
	bSCUsed = false; SCAhead.Reset(); SCState = 0; SCCount = 0; SCPassed = 0; GreenT = StewardT = 0.f; Inquiries.Reset(); bStewardInquiry = false;
	for (AGPMCar* C : Cars) C->SlowMul = 1.f;
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
	RollReliability();   // moteurs qui faibliront pendant cette course
	bTyreWarn = false; if (RadioT < 4.f) RadioT = 0.f;   // garde le message de pénalité de grille
	// conseil de l'ingénieur selon le style de pilotage de la dernière course
	if (RadioT <= 0.f && LastStyle > 1.08f) Say(FString::Printf(TEXT("Muret : la dernière fois, tes pneus se sont usés %.0f %% plus vite que ceux du plateau. Freine en ligne droite, ou prévois des relais plus courts."), (LastStyle - 1.f) * 100.f));
	else if (RadioT <= 0.f && LastStyle > 0.f && LastStyle < 0.94f) Say(FString::Printf(TEXT("Muret : la dernière fois, tes pneus ont tenu %.0f %% mieux que ceux du plateau : tu peux allonger les relais."), (1.f - LastStyle) * 100.f));
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
	// pluie en 3D : seulement en piste (pas dans les menus ni l'atelier)
	if (Rain3D)
	{
		APlayerController* PCam = GetWorld()->GetFirstPlayerController();
		if (InMenu() || !PCam || !PCam->PlayerCameraManager) Rain3D->Clear();
		else
		{
			// vue embarquée (T-cam, cockpit) : gouttes sur l'objectif ; pas pendant les cinématiques
			// voitures lointaines sans ombre (plus de 70 m de la caméra), feu arrière allumé sous la pluie
			const FVector CamAt = PCam->PlayerCameraManager->GetCameraLocation();
			const bool bLight = bRaining || TrackWet > 0.3f;
			for (AGPMCar* C : Order)
				if (C) { C->SetNear(FVector::DistSquared(C->GetActorLocation(), CamAt) < FMath::Square(7000.f)); C->SetRainLight(bLight); }
			USceneComponent* Onboard = Car && PCam->GetViewTarget() == Car.Get() ? Car->OnboardCam() : nullptr;
			Rain3D->UpdateRain(DeltaSeconds, PCam->PlayerCameraManager->GetCameraLocation(), PCam->PlayerCameraManager->GetCameraRotation(), Order, TrackWet, RainShow, Onboard, Car ? Car->V : 0.f);
		}
	}
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
		// après la révélation de la monoplace : la présentation devant la presse (fenêtre à choix)
		if (bRevealDialog && Cine != ECine::Welcome)
		{
			bRevealDialog = false; bReveal = false;
			if (UGPMCareerSubsystem* CR = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>()) if (CR->Current()) { bNegoFromMercato = false; Phase = EPhase::Press; }
		}
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
	if (bTesting && Phase == EPhase::Quali) { TrackLimits(DeltaSeconds); TestTick(DeltaSeconds); return; }
	if (Phase == EPhase::Quali || Phase == EPhase::QualiDone)
	{
		TrackLimits(DeltaSeconds);
		RadioT -= DeltaSeconds;
		if (Phase == EPhase::Quali) UpdateSectors();
		if (Phase == EPhase::Quali && bPractice && Car) { WearAndFuel(Car, DeltaSeconds); GateTick(); if (Car->Lap != MisLapSeen) MisLap(); }
		if (Phase == EPhase::Quali && Car && Car->Lap >= (bPractice && PracticeProg == 2 ? 3 : 2)) { if (bPractice) EndPractice(); else EndQuali(); }
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
	UpdateRed(DeltaSeconds);
	if (Scenery) Scenery->SetStartLights(LightsOn());
	// mécaniciens du carré : prêts quand une voiture de l'écurie arrive, au travail pendant l'arrêt (pas pendant un stop-and-go)
	if (Scenery && Car && (Phase == EPhase::Race || Phase == EPhase::Quali || Phase == EPhase::Lights))
	{
		int32 Crew = 0;
		const AGPMCar* At = nullptr;
		for (AGPMCar* C : Cars)
		{
			if (!C || C->TeamIdx != Car->TeamIdx || !C->PitPhase) continue;
			const float D = FGPMTrackData::PitBox(C->TeamIdx) - C->PitKf;
			const int32 S = C->PitPhase == 2 ? (C->bPenVisit ? 1 : 2) : (C->PitPhase == 1 && D < 50.f && D > -6.f) ? 1 : 0;
			if (S > Crew) { Crew = S; At = C; }
		}
		Scenery->SetCrew(Crew, Crew == 2 && At ? At->GetActorLocation() : FVector::ZeroVector, At ? At->GetActorForwardVector() : FVector::ForwardVector, Car->TeamColor, DeltaSeconds);
	}
	Aspiration(DeltaSeconds);
	Collide();
	TrackLimits(DeltaSeconds);
	if (Phase == EPhase::Race)
	{
		Strategy(DeltaSeconds);
		if (!bSCUsed && UGameplayStatics::HasOption(OptionsString, TEXT("sc")) && Clock > 20.f) RequestSafety(true);
		UpdateSafety(DeltaSeconds);
		Stewards(DeltaSeconds);
		UpdateFlags(DeltaSeconds);
	}
	GreenT -= DeltaSeconds; StewardT -= DeltaSeconds;
	RadioT -= DeltaSeconds;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		// radio du muret : 1 (ou I) point complet, 2 (ou M) rythme, 3 (ou N) pneus du prochain arrêt
		if (DigitPressed(PC, 1) || PC->WasInputKeyJustPressed(EKeys::I)) RadioCall(0);
		if (DigitPressed(PC, 2) || PC->WasInputKeyJustPressed(EKeys::M)) RadioCall(1);
		if (DigitPressed(PC, 3) || PC->WasInputKeyJustPressed(EKeys::N)) RadioCall(2);
	}
	// arrivée : tour de plus que la distance de course
	// (drapeau à damier : une fois le vainqueur arrivé, chaque voiture termine à son prochain passage sur la ligne, même à un tour)
	const bool bLive = Phase != EPhase::Done && Phase != EPhase::Outro;   // après l'arrivée, le classement ne bouge plus
	if (bLive)
	{
		bool bNew = false;
		for (AGPMCar* C : Cars)
		{
			if (C->bFinished || C->bOut) continue;
			if (C->Lap > TotalLaps || (C->FlagLap >= 0 && C->Lap > C->FlagLap))
			{
				C->bFinished = true; C->FinishTime = Clock; bNew = true;
				C->FinProgress = float(C->Lap) * (C->GetTrack() ? C->GetTrack()->WX.Num() : 1);
			}
		}
		if (bNew) for (AGPMCar* C : Cars) if (!C->bFinished && !C->bOut && C->FlagLap < 0) C->FlagLap = C->Lap;
	}
	if (Phase == EPhase::Race && Car && (Car->bFinished || Car->bOut))
	{
		if (Car->bFinished) Car->bPlayerControlled = false;   // le pilote automatique ramène la voiture
		DoneT += DeltaSeconds;
		bool bAll = true;
		for (AGPMCar* C : Cars) bAll &= C->bFinished || C->bOut;
		if (DoneT > 4.f || bAll)
		{
			Phase = EPhase::Done;
			// voitures encore en piste : elles finissent leur tour au rythme de leur dernier tour (temps calculé, classement figé)
			// (le joueur a abandonné avant le drapeau : le reste de la course est calculé jusqu'au bout, arrêts et casses compris)
			bool bFlagOut = false;
			for (const AGPMCar* C : Cars) bFlagOut |= C->bFinished;
			for (AGPMCar* C : Cars)
			{
				const FGPMTrackData* Tc = C->GetTrack();
				if (C->bFinished || C->bOut || !Tc) continue;
				const int32 Nc = FMath::Max(1, Tc->WX.Num());
				const float LapT = C->LastLap > 0.f ? C->LastLap : LapEstimate;
				if (bFlagOut)
				{
					C->FinishTime = Clock + (1.f - float(C->Idx) / Nc) * LapT + (C->PitPhase ? 12.f : 0.f);
					C->FinProgress = float(C->Lap + 1) * Nc;
					continue;
				}
				const float* FailAt = EngineBlown.Contains(C) ? EngineFailAt.Find(C) : nullptr;
				if (FailAt)
				{
					C->bOut = true; C->OutWhy = TEXT("moteur");
					C->FinProgress = FMath::Max(C->Progress(), (1.f + *FailAt * TotalLaps) * Nc);
					continue;
				}
				const float Pace = (C->BestLap > 0.f ? C->BestLap * 1.012f : LapT) * (1.f + 0.003f * (FMath::FRand() * 2.f - 1.f));
				const int32 StopsLeft = FMath::Max(0, C->Plan.Num() - 1 - C->Stops);
				C->FinishTime = Clock + FMath::Max(0.f, TotalLaps + 1 - C->Lap - float(C->Idx) / Nc) * Pace + StopsLeft * Tc->PitLoss + (C->PitPhase ? 12.f : 0.f);
				C->FinProgress = float(TotalLaps + 1) * Nc;
			}
			Order.Sort([](const AGPMCar& A, const AGPMCar& B)
			{
				if (A.bOut != B.bOut) return B.bOut;
				if (A.FinProgress != B.FinProgress) return A.FinProgress > B.FinProgress;
				return A.bOut ? false : A.FinishTime + A.Penalty < B.FinishTime + B.Penalty;
			});
			RecordResult();
			// tour d'honneur autour de la voiture après un podium (confettis pour une victoire)
			const int32 P = Car->bOut ? 0 : PositionOf(Car);
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
	// (abandons derniers, le plus loin d'abord ; à tours égaux, les arrivés par temps, pénalités comprises)
	if (Phase != EPhase::Done && Phase != EPhase::Outro) Order.Sort([](const AGPMCar& A, const AGPMCar& B)
	{
		if (A.bOut != B.bOut) return B.bOut;
		if (A.bOut) return A.FinProgress > B.FinProgress;
		const float Ka = A.bFinished ? A.FinProgress : A.Progress(), Kb = B.bFinished ? B.FinProgress : B.Progress();
		if (A.bFinished && B.bFinished && Ka == Kb) return A.FinishTime + A.Penalty < B.FinishTime + B.Penalty;
		if (Ka != Kb) return Ka > Kb;
		return A.bFinished && !B.bFinished;
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

void AGPMGameMode::RequestSafety(bool bForce, bool bVirtual)
{
	if (SCState || RedState || SCCount >= 2 || Phase != EPhase::Race || Order.Num() == 0) return;
	const AGPMCar* Lead = Order[0];
	const FGPMTrackData* T = Lead->GetTrack();
	if (!T) return;
	if (!bForce && (Lead->Progress() / float(T->WX.Num()) - 1.f) / TotalLaps > 0.85f) return;   // pas dans les derniers tours
	if (!bForce && FMath::FRand() > (bVirtual ? 0.6f : 0.45f)) return;
	bSCUsed = true; ++SCCount; SCT = 0.f; SCEnding = 0.f;
	SCAhead.Reset();
	for (AGPMCar* C : Order) { if (C == Car) break; if (!C->bOut) SCAhead.Add(C); }
	if (bVirtual)
	{
		// voiture de sécurité virtuelle : tout le monde à 40 % de moins, les écarts restent, pas de dépassement
		SCState = 4;
		for (AGPMCar* C : Cars) C->SlowMul = 0.6f;
		Say(TEXT("Muret : VSC ! Voiture de sécurité virtuelle : ralentis, garde ton écart, pas de dépassement."));
		return;
	}
	SC = GetWorld()->SpawnActor<AGPMCar>();
	if (!SC) return;
	SC->DriverName = TEXT("Sécurité");
	SC->TeamIdx = 12;   // garage au bout de la voie des stands, après ceux des écuries
	SC->MakeSafetyBody();
	SC->Perf = 0.5f;
	SC->MakeAI(0.6f);
	SC->PlaceOnTrack(T, 0);
	// elle sort de la voie des stands si le leader est encore loin, sinon elle s'insère 220 m devant lui
	const int32 N = T->WX.Num(), Exit = (T->PitX1 + N) % N;
	const int32 Ahead = ((Exit - Lead->Idx) % N + N) % N;
	const int32 I = Ahead > 150 && Ahead < N / 2 ? Exit : (Lead->Idx + 110) % N;
	SC->Idx = I; SC->X = T->WX[I]; SC->Z = T->WZ[I]; SC->Y = T->WY[I]; SC->Heading = T->H[I];
	SC->Lat = SC->Lane = 0.f; SC->V = 40.f; SC->Lap = 1000;   // jamais comptée à l'arrivée
	SC->SetBeacons(true);
	SCState = 1;
	for (AGPMCar* C : Cars) C->Field.Add(SC);   // le peloton se cale derrière elle
	Say(TEXT("Muret : voiture de sécurité ! Ralentis, file indienne, interdiction de dépasser."));
	UE_LOG(LogGPM, Log, TEXT("Voiture de sécurité : sortie (indice %d, leader %d)"), I, Lead->Idx);
}

void AGPMGameMode::UpdateSafety(float Dt)
{
	for (AGPMCar* C : Cars) C->bSCMode = SafetyOut() && !(SCState == 3 && Order.Num() && C == Order[0]);
	if (!SCState) return;
	SCT += Dt;
	auto Green = [this]()
	{
		SCState = 0; GreenT = 3.f; SCAhead.Reset();
		for (AGPMCar* C : Cars) { C->SlowMul = 1.f; C->bSCMode = false; }
		Say(TEXT("Muret : drapeau vert, c'est reparti, attaque !"));
	};
	if (SCState >= 4)
	{
		// VSC : 20 à 30 s, puis « fin de VSC » et piste verte 4 s plus tard
		if (SCState == 4 && SCT > 20.f + 10.f * FMath::Frac(SCCount * 0.37f)) { SCState = 5; SCEnding = 4.f; Say(TEXT("Muret : fin de la VSC, prépare-toi à relancer !")); }
		if (SCState == 5 && (SCEnding -= Dt) <= 0.f) { Green(); return; }
	}
	else
	{
		const FGPMTrackData* T = SC ? SC->GetTrack() : nullptr;
		// fin : peloton regroupé (moins de 22 m par voiture) au bout de 30 s, ou au bout de 80 s ; gyrophares éteints
		const AGPMCar* LastRun = nullptr;
		for (int32 k = Order.Num() - 1; k >= 0 && !LastRun; --k) if (!Order[k]->bOut) LastRun = Order[k];
		const float Spread = LastRun && Order.Num() > 1 ? (Order[0]->Progress() - LastRun->Progress()) * 2.f : 0.f;
		if (SCState == 1 && ((SCT > 30.f && Spread < Order.Num() * 22.f) || SCT > 80.f))
		{
			SCState = 2;
			if (SC) SC->SetBeacons(false);
			Say(TEXT("Muret : la voiture de sécurité rentre à la fin de ce tour. Reste collé, pas de dépassement avant la ligne !"));
		}
		// elle plonge dans la voie des stands et se gare au bout ; le leader mène alors le peloton jusqu'à la ligne
		if (SCState == 2 && SC && T)
		{
			const int32 K = T->PitK(SC->Idx);
			if (!SC->PitPhase && K > T->PitE0 - 30 && K < T->PitE0 - 10) SC->StartPit(1e6f);
			if (SC->PitPhase >= 2 || SCT > 160.f)
			{
				for (AGPMCar* C : Cars) C->Field.Remove(SC);
				SC->Destroy(); SC = nullptr;
				SCState = 3; SCLeader = Order.Num() ? Order[0] : nullptr; SCLeadLap = Order.Num() ? Order[0]->Lap : 0;
			}
		}
		// relance : drapeau vert quand le leader passe la ligne
		if (SCState == 3 && (!SCLeader.IsValid() || SCLeader->Lap > SCLeadLap || SCLeader->bOut)) { Green(); return; }
	}
	// dépassement d'une voiture qui était devant (voiture de sécurité, VSC, relance avant la ligne) : 5 s, puis drive-through
	if (!Car || Car->PitPhase || SCState == 5) { if (!Car || Car->PitPhase) SCAhead.Reset(); return; }
	for (auto It = SCAhead.CreateIterator(); It; ++It)
	{
		AGPMCar* C = *It;
		if (C->PitPhase || C->bOut) { It.RemoveCurrent(); continue; }
		if (Car->Progress() - C->Progress() > 4.f)
		{
			It.RemoveCurrent();
			if (Car->bPlayerControlled)
			{
				const bool bAgain = SCPassed > 0;
				++SCPassed;
				Penalize(Car, bAgain ? 1 : 0, 5.f, FString::Printf(TEXT("Dépassement de %s sous %s"), *C->DriverName, SCState >= 4 ? TEXT("VSC") : TEXT("voiture de sécurité")), 3.f);
			}
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
		// abandon : la voiture s'arrête en roue libre, puis les commissaires l'évacuent (celle du joueur reste sur place)
		if (C->bOut)
		{
			C->OutT += Dt;
			C->PowerMul = 0.05f; C->AccMul = 0.f; C->DmgNew = C->DmgNewR = 0.f;
			C->V = FMath::Max(0.f, C->V - 7.f * Dt);
			if (C != Car && !C->bParked && (C->V < 2.f || C->OutT > 12.f))
			{
				Park(C, true);
				for (AGPMCar* O : Cars) O->Field.Remove(C);
			}
			continue;
		}
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
		if (!C->PitPhase)
		{
			WearAndFuel(C, Dt);   // usure selon le style de pilotage, essence selon l'accélérateur (GPMMissions.cpp)
			if (C == Car && Phase == EPhase::Race) PlayerLapLog();
			// pneu à 0 % : il peut éclater à tout moment, retour au stand au ralenti
			if (!C->bFlat && C->Wear >= 1.f && FMath::FRand() < Dt * (0.03f + 0.4f * (C->Wear - 1.f)))
			{
				C->bFlat = true;
				if (Left >= 1 && !C->bFinished) { C->bBox = true; C->NextTyre = NextStopTyre(C); }
				if (C == Car) { bHadFlat = true; Say(TEXT("Muret : crevaison ! Rentre au stand doucement, on change les pneus.")); }
				else Say(FString::Printf(TEXT("Muret : crevaison pour %s !"), *C->DriverName));
			}
		}
		C->Mass = 1.f + C->Fuel / 750.f;
		// panne sèche : le moteur ne tourne plus qu'au ralenti jusqu'à l'arrivée (ou au stand)
		const bool bDry = C->Fuel <= 0.f && !C->bFinished && Phase == EPhase::Race;
		C->PowerMul = AGPMCar::ModeV(C->Mode) * (C->bFlat ? 0.8f : 1.f) * (bDry ? 0.35f : 1.f);
		C->AccMul = AGPMCar::ModeA(C->Mode) * (bDry ? 0.3f : 1.f);
		if (EngineTrouble(C)) { C->PowerMul *= 0.93f; C->AccMul *= 0.8f; }   // ennui moteur : moins de puissance jusqu'à l'arrivée
		if (C == Car && !C->bFinished && Phase == EPhase::Race)
		{
			if (bDry && FuelWarn < 2) { FuelWarn = 2; Say(TEXT("Muret : panne sèche ! Plus d'essence, la voiture n'avance plus qu'au ralenti.")); }
			else if (FuelWarn == 0 && C->Mode != 2 && C->Lap >= 1 && FuelLapsLeft < LapsToRefuel(C) - 0.05f)
			{
				FuelWarn = 1;
				const float NeedL = FMath::Max(0.5f, LapsToRefuel(C));
				const float Short = FMath::Max(0.f, PerLapFuel * NeedL - C->Fuel);
				Say(FString::Printf(TEXT("Muret : l'essence ne suffira pas, il manque %.1f kg : économise %.1f kg par tour. Passe en économie (touche 2) et lève le pied avant les freinages."), Short, Short / NeedL));
			}
		}
		C->GripMul = AGPMCar::WetGrip(C->Tyre, TrackWet) * AGPMCar::LifeGrip(1.f - C->Wear) * (C->bFlat ? 0.55f : 1.f) * (1.f - 0.04f * C->Dmg - 0.03f * C->DmgR);
		// appui perdu : aileron avant → sous-virage, aileron arrière → survirage à haute vitesse
		C->DmgKd = C->bFwOff ? 0.3f : 1.f - 0.45f * C->Dmg;
		C->RwKd = C->bRwOff ? 0.3f : 1.f - 0.45f * C->DmgR;
		if (C == Car && !bTyreWarn && C->Wear > 0.6f && !C->PitPhase) { bTyreWarn = true; Say(TEXT("Muret : tes pneus passent sous 40 %, ils n'ont presque plus de grip.")); }

		// joueur dans la voie des stands : messages du muret, puis durée de l'arrêt et ravitaillement quand il est dans son carré
		if (C->PitMsg)
		{
			const int32 Msg = C->PitMsg;
			C->PitMsg = 0;
			if (Msg == 1)
			{
				const float Fix = (C->Dmg > 0.15f ? 3.f : 0.f) + (C->DmgR > 0.15f ? 4.f : 0.f) + (C->bFwOff || C->bRwOff ? 2.f : 0.f);
				const float Before = C->Fuel;
				int32 Upto = TotalLaps;
				if (Stints.IsValidIndex(C->Stops + 2))
				{
					int32 Planned = 0;
					for (int32 s = 0; s <= C->Stops + 1; ++s) Planned += Stints[s].Value;
					Upto = FMath::Min(TotalLaps, Planned);
				}
				C->Fuel = FMath::Max(C->Fuel, FMath::Min(Tank, FuelFor(C, FMath::Max(1, Upto - C->Lap) + 0.3f) * 1.04f));
				FuelWarn = 0; bTyreWarn = false; ManualTyre = 0; bTyrePicked = false;
				C->PitStopT = FMath::Max(1.4f, 2.3f + FMath::FRand() * 0.6f + C->UpPit) + Fix;
				const float Served = ServeAtStop(C);
				if (C->bPenVisit) { C->PitStopT = Served; continue; }   // stop-and-go ou drive-through : pas de pneus
				C->PitStopT += Served;
				// l'équipier est encore dans le carré : les mécaniciens finissent sa voiture d'abord
				float Wait = 0.f;
				for (const AGPMCar* O : Cars) if (O != C && O->TeamIdx == C->TeamIdx && O->PitPhase == 2) Wait = FMath::Max(Wait, FMath::Min(O->PitStopT, 8.f) + 1.f);
				C->PitStopT += Wait;
				const int32 Kg = FMath::RoundToInt(C->Fuel - Before);
				if (Wait > 0.f) Say(TEXT("Muret : ton équipier est encore dans le carré, on s'occupe de toi juste après…"));
				else Say(FString::Printf(TEXT("Muret : parfait ! Pneus %s%s%s…"), *TyreName(C->NextTyre), Kg > 0 ? *FString::Printf(TEXT(", +%d kg"), Kg) : TEXT(""), Fix > 0.f ? TEXT(" et réparation") : TEXT("")));
			}
			else if (Msg == 2) Say(TEXT("Muret : go, go, go ! Garde le limiteur jusqu'à la sortie."));
			else if (Msg == 3) Say(TEXT("Muret : tu as dépassé ton carré ! Arrête-toi et recule (frein à l'arrêt), ou ressors sans changer."));
			else if (Msg == 4)
			{
				if (C->PenKind == 1) { C->PenKind = 0; Say(TEXT("Muret : drive-through purgé, c'est bon !")); }
				else Say(TEXT("Muret : pas d'arrêt cette fois, on reste sur ces pneus."));
			}
			else if (Msg == 5) Penalize(C, 0, 5.f, TEXT("Excès de vitesse dans la voie des stands"), 4.f);
		}
		// décision d'arrêt avant l'entrée des stands
		const int32 K0 = T->PitK(C->Idx);
		const bool bZone = K0 > T->PitE0 - 30 && K0 < T->PitE0 - 10;
		if (K0 > 200 && K0 < 210) C->bAdvised = false;   // nouveau tour : le muret peut reconseiller
		// le joueur prend lui-même la voie des stands (à droite, à l'entrée de la voie)
		if (!bAuto && !C->PitPhase && C->Lap >= 1 && !C->bFinished && Phase == EPhase::Race && K0 > T->PitE0 + 12 && K0 < T->PitE1
			&& C->Lat > T->PitHalf + 1.5f && C->Lat > T->PitLatAt(K0) - 2.5f)
		{
			if (!C->bBox) C->NextTyre = NextStopTyre(C);
			C->StartManualPit();
			Say(FString::Printf(TEXT("Muret : voie des stands, limiteur ! Arrête-toi dans le carré de ton écurie (cadre jaune) : pneus %s prêts."), *TyreName(C->NextTyre)));
			continue;
		}
		if (!bZone || C->PitPhase || C->Lap < 1 || Left < 1 || C->bFinished) continue;
		// arrêt prévu par la stratégie (le joueur : celle de l'écran de stratégie, le muret l'appelle au bon tour)
		const TArray<TPair<TCHAR, int32>>& Pl = C == Car ? Stints : C->Plan;
		if (!C->bBox && Pl.IsValidIndex(C->Stops + 1))
		{
			int32 Planned = 0;
			for (int32 s = 0; s <= C->Stops; ++s) Planned += Pl[s].Value;
			if (C->Lap >= Planned && bAuto) { C->bBox = true; C->NextTyre = NextStopTyre(C); }
			// joueur : le muret l'appelle au tour prévu, c'est à lui d'entrer dans la voie
			else if (C->Lap >= Planned && !C->bAdvised)
			{
				C->bAdvised = true;
				C->NextTyre = NextStopTyre(C);
				Say(FString::Printf(TEXT("Muret : box, box ! Prends la voie des stands à droite : pneus %s."), *TyreName(C->NextTyre)));
			}
		}
		// neutralisation : le peloton roule au ralenti, un arrêt coûte bien moins cher → on avance l'arrêt prévu dans les 6 tours
		if ((SafetyOut() || SCState == 4) && !C->bBox && Left >= 3)
		{
			int32 NextStop = 999;
			if (Pl.IsValidIndex(C->Stops + 1)) { NextStop = 0; for (int32 s = 0; s <= C->Stops; ++s) NextStop += Pl[s].Value; }
			if (NextStop - C->Lap <= 6 || C->Wear > 0.4f)
			{
				if (bAuto) { C->bBox = true; C->NextTyre = NextStopTyre(C); }
				else if (SCCall != SCCount)
				{
					SCCall = SCCount; C->bAdvised = true; C->NextTyre = NextStopTyre(C);
					Say(FString::Printf(TEXT("Muret : box, box sous %s ! L'arrêt coûte deux fois moins de temps : pneus %s prêts."), SCState == 4 ? TEXT("la VSC") : TEXT("la voiture de sécurité"), *TyreName(C->NextTyre)));
				}
			}
		}
		// IA : pluie, piste sèche, crevaison, pneus à bout, aileron cassé
		if (bAuto && !C->bBox && WantBox(C)) { C->bBox = true; C->NextTyre = NextStopTyre(C); }
		// joueur : conseil du muret, une fois par tour
		if (!bAuto && !C->bBox && !C->bAdvised && WantBox(C))
		{
			C->bAdvised = true;
			Say(FString::Printf(TEXT("Muret : rentre ce tour, prends la voie des stands à droite : pneus %s%s."), *TyreName(NextStopTyre(C)), C->Dmg > 0.5f || C->DmgR > 0.5f ? TEXT(" et nouvel aileron") : TEXT("")));
		}
		// un seul carré par écurie : si l'équipier y est déjà (ou s'y rend, ou si le joueur a demandé son arrêt), on attend le tour
		// suivant ; sur crevaison on rentre quand même et les mécaniciens font attendre la voiture
		if (C->bBox && bAuto && C->PenKind == 1)
		{
			// drive-through : on traverse la voie au limiteur sans s'arrêter
			C->StartPit(0.f); C->PitPhase = 3; C->bPenVisit = C->bDriveThru = true; C->PenKind = 0;
			continue;
		}
		float Queue = 0.f;
		if (C->bBox && bAuto)
			for (const AGPMCar* O : Cars)
				if (O != C && O->TeamIdx == C->TeamIdx && !O->bOut && (O->PitPhase == 1 || O->PitPhase == 2 || (O == Car && O->bBox && O->bPlayerControlled)))
					Queue = FMath::Max(Queue, 3.f + (O->PitPhase == 2 ? FMath::Min(O->PitStopT, 8.f) : 4.f));
		if (Queue > 0.f && !C->bFlat) continue;
		if (C->bBox && bAuto)
		{
			const float Fix = Queue + (C->Dmg > 0.15f ? 3.f : 0.f) + (C->DmgR > 0.15f ? 4.f : 0.f) + (C->bFwOff || C->bRwOff ? 2.f : 0.f);
			// pièces Stands et Centre des mécaniciens : l'équipe du joueur gagne du temps à chaque arrêt
			const float Crew = Car && C->TeamIdx == Car->TeamIdx ? Car->UpPit : 0.f;
			const float Served = ServeAtStop(C);
			C->StartPit(C->bPenVisit ? Served : FMath::Max(1.4f, 2.3f + FMath::FRand() * 0.6f + Crew) + Fix + Served);
			if (C->bPenVisit) continue;
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
	for (const AGPMCar* C : Order) Fin.Add({ C->DriverName, C->TeamName, C->BestLap, C == Car, C->bOut });
	const int32 Pos = Order.IndexOfByKey(Car) + 1;
	LastPts = Season->RecordRace(Fin);
	UGPMSaveGame* G = Season->State();
	UGPMFactorySubsystem* Factory = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
	if (!G || !Factory) return;
	// gains (racePrize de team.js) + part des sponsors ; moral et réputation suivent les résultats
	static const float Prize[10] = { 5.f, 4.f, 3.4f, 2.8f, 2.4f, 2.1f, 1.8f, 1.6f, 1.4f, 1.2f };
	LastPartner = Factory->PartnerIncome();
	LastPrize = (Pos >= 1 && Pos <= 10 ? Prize[Pos - 1] : 0.8f) * (0.8f + G->Rep / 250.f) * Factory->PrizeMul() + LastPartner;
	if (G->FinStart < 0.f) G->FinStart = G->Budget;   // bilan financier : budget au départ de la saison
	G->FinPrize += LastPrize - LastPartner; G->FinPartner += LastPartner;
	Factory->AddMoney(LastPrize);
	// moral : un podium le fait monter, les points le tiennent, une course sans point le fait baisser ; l'euphorie retombe
	// un peu d'une course à l'autre (sinon la jauge restait bloquée à 100). Réputation : elle peut aussi redescendre.
	// (le moral se juge sur la place attendue par la direction : mieux que prévu il monte, moins bien il baisse, et il revient
	// peu à peu vers 60 ; un podium fait toujours plaisir)
	{
		const UGPMCareerSubsystem* CareerM = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
		const int32 Exp = CareerM ? CareerM->Expected() : 9;
		const int32 Swing = FMath::Clamp(FMath::RoundToInt((Exp - Pos) * 0.8f), -4, 4) + (Pos == 1 ? 2 : Pos <= 3 ? 1 : 0);
		// l'euphorie retombe plus vite (vers 55), et les gains sont plus durs près du sommet (Bump)
		G->Moral = UGPMCareerSubsystem::Bump(G->Moral, float(Swing)) - FMath::RoundToInt((G->Moral - 55) / 8.f);
		G->Moral = FMath::Clamp(G->Moral, 0, 100);
	}
	// réputation : une victoire compte plus qu'un podium ; au-dessus de 70 elle s'érode si on ne la nourrit pas
	G->Rep = UGPMCareerSubsystem::Bump(G->Rep, Pos == 1 ? 2.f : Pos <= 3 ? 1.f : Pos <= 6 ? 0.4f : Pos <= 12 ? 0.f : -1.f);
	if (G->Rep > 70) G->Rep = FMath::Max(70, G->Rep - FMath::RoundToInt((G->Rep - 70) / 15.f));
	// Les écuries rivales développent leur voiture à chaque course, comme le joueur : vite au début d'un règlement, de moins en
	// moins quand leur voiture approche de son plafond (+0,15), plus vite quand elles ont du retard sur la meilleure voiture du
	// plateau (celle du joueur comprise), et selon leurs moyens (les grandes écuries investissent plus).
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
	{
		// les rivaux réagissent quand tu domines : plus tu enchaînes les podiums (6 derniers GP), plus ils poussent leur développement
		float React = 1.f;
		{
			const TArray<FGPMRaceResult>& Res = Season->Results();
			int32 Pods = 0, Seen = 0;
			for (int32 k = Res.Num() - 1; k >= 0 && Seen < 6; --k, ++Seen) if (Res[k].Pos >= 1 && Res[k].Pos <= 3) ++Pods;
			const float Dom = Seen >= 4 ? FMath::Clamp((Pods - 2.f) / 4.f, 0.f, 1.f) : 0.f;
			React = 1.f + 0.8f * Dom;
			const FString Flag = FString::Printf(TEXT("react%d"), G->Year);
			if (Dom >= 0.5f && !G->Flags.Contains(Flag))
			{
				G->Flags.Add(Flag);
				if (UGPMCareerSubsystem* CS = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>())
					CS->SendMail(TEXT("presse"), TEXT("Paddock"), TEXT("Tes rivaux contre-attaquent"),
						TEXT("Ta domination agace le paddock : les grandes écuries rivales débloquent des budgets d'urgence et accélèrent leurs évolutions. Attends-toi à les voir revenir dans les prochaines courses : garde ton usine en mouvement."));
			}
		}
		float BestCar = Factory->PlayerPerf();
		for (int32 t = 1; t < Data->Teams.Num(); ++t) BestCar = FMath::Max(BestCar, Data->Teams[t].Perf + G->RivalDev.FindRef(Data->Teams[t].Name));
		for (int32 t = 1; t < Data->Teams.Num(); ++t)
		{
			const float Dev = G->RivalDev.FindRef(Data->Teams[t].Name), Mine = Data->Teams[t].Perf + Dev;
			const float Room = FMath::Clamp(1.f - Dev / 0.15f, 0.15f, 1.3f);
			const float Catch = 1.f + 0.6f * FMath::Clamp((BestCar - Mine) / 0.05f, 0.f, 1.f);
			const float Means = 0.85f + 0.25f * FMath::Clamp((Data->Teams[t].Perf - 0.95f) / 0.04f, 0.f, 1.f);
			Factory->RivalGain(Data->Teams[t].Name, (0.0022f + FMath::FRand() * 0.0028f) * Room * Catch * Means * React * Factory->RivalDevMul(Data->Teams[t].Name));
		}
	}
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
		R.Pos = Pos; R.Grid = GridSlot + 1; R.Stops = Car->Stops; R.Pen = Car->PenSum; R.bSC = bSCUsed;
		R.bWet = bRainStart || WxChangeAt >= 0.f;   // il a plu pendant la course (questions de presse et flash info sur la pluie)
		R.bFlat = bHadFlat;
		R.bDnf = Car->bOut;
		R.Circuit = CurTrack() ? CurTrack()->Name : FString();
		const FString Dotd = RecordHonours(Pos);
		R.Winner = Order.Num() ? (Order[0] == Car ? FString(TEXT("Toi")) : Order[0]->DriverName) : FString();
		R.OutWhy = Car->bOut ? Car->OutWhy : FString();
		R.bRed = bRedUsed;
		R.Style = PStyleDist > 2000.f ? (PStyleSum / PStyleDist - 1.f) * 100.f : 0.f;
		if (Pos >= 1 && !Car->bOut)
		{
			R.WinGap = Pos > 1 ? ResultGap(Car) : FString();
			if (Order.IsValidIndex(Pos - 2) && !Order[Pos - 2]->bOut)
			{
				R.Ahead = Order[Pos - 2]->DriverName;
				const float Gp = ResultTime(Car) - ResultTime(Order[Pos - 2]);
				if (Gp > 0.f && Gp < 30.f) R.GapAhead = Gp;
			}
			if (Order.IsValidIndex(Pos) && !Order[Pos]->bOut)
			{
				R.Behind = Order[Pos]->DriverName;
				const float Gp = ResultTime(Order[Pos]) - ResultTime(Car);
				if (Gp > 0.f && Gp < 30.f) R.GapBehind = Gp;
			}
		}
		const int32 RivalIdx = Pos >= 2 ? Pos - 2 : 1;
		if (Order.IsValidIndex(RivalIdx)) R.Rival = Order[RivalIdx]->DriverName;
		float Best = 1e9f;
		for (const AGPMCar* C : Order) if (C->BestLap > 0.f) Best = FMath::Min(Best, C->BestLap);
		R.bFastest = Car->BestLap > 0.f && Car->BestLap <= Best;
		for (int32 k = 0; k < Order.Num(); ++k) if (Order[k] != Car && Order[k]->TeamIdx == Car->TeamIdx) R.MatePos = k + 1;
		for (int32 k = 0; k < Order.Num(); ++k) if (!G->MisWho.IsEmpty() && Order[k]->DriverName == G->MisWho) R.RivalPos = k + 1;
		const float BeforeLife = G->Budget;
		LastLines = Career->AfterRace(R);
		if (!Dotd.IsEmpty()) LastLines.Insert(Dotd, 0);
		G->FinPartner += G->Budget - BeforeLife;   // sponsor principal, contrats ponctuels, missions, primes de l'équipier
		if (!Built.IsEmpty()) LastLines.Add(Built);
		if (Car->bOut && Car->OutWhy == TEXT("moteur")) LastLines.Add(TEXT("Casse moteur : abandon. Le pôle Fiabilité de l'usine et un motoriste plus fiable réduisent ce risque."));
		else if (Car->bOut) LastLines.Add(TEXT("Accident : abandon, la voiture sera réparée pour le prochain Grand Prix."));
		else if (EngineFailAt.Contains(Car.Get())) LastLines.Add(TEXT("Ennui moteur pendant la course : de la puissance perdue. Le pôle Fiabilité de l'usine réduit ce risque."));
		if (PStyleDist > 2000.f)
		{
			const float St = PStyleSum / PStyleDist, Pc = (St - 1.f) * 100.f;
			LastLines.Add(Pc > 4.f ? FString::Printf(TEXT("Pilotage : tes pneus se sont usés %.0f %% plus vite que la moyenne du plateau (freinages tardifs, virages attaqués)."), Pc)
				: Pc < -4.f ? FString::Printf(TEXT("Pilotage : tes pneus se sont usés %.0f %% moins vite que la moyenne du plateau, belle gestion."), -Pc)
				: FString(TEXT("Pilotage : usure des pneus dans la moyenne du plateau.")));
			UE_LOG(LogGPM, Log, TEXT("Style de pilotage : %.2f (référence IA %.3f)"), St, AGPMCar::StressRef);
			LastStyle = St;
			// palmarès : courses bien gérées, style moyen de la carrière (en centièmes)
			if (bCareer && G)
			{
				if (St <= 0.96f) G->Rec.FindOrAdd(TEXT("saver"))++;
				G->Rec.FindOrAdd(TEXT("styleSum")) += FMath::RoundToInt((St - 1.f) * 100.f);
				G->Rec.FindOrAdd(TEXT("styleN"))++;
			}
		}
		// le plus doux et le plus dur avec ses pneus
		{
			const AGPMCar* Gentle = nullptr;
			const AGPMCar* Rough = nullptr;
			float GentleV = 99.f, RoughV = 0.f;
			for (const TPair<const AGPMCar*, FVector2f>& Pr : RaceStyle)
			{
				if (!Pr.Key || Pr.Value.Y < 3000.f || Pr.Key->bOut) continue;
				const float Sv = Pr.Value.X / Pr.Value.Y;
				if (Sv < GentleV) { GentleV = Sv; Gentle = Pr.Key; }
				if (Sv > RoughV) { RoughV = Sv; Rough = Pr.Key; }
			}
			auto Who = [this](const AGPMCar* Cx) { return Cx == Car.Get() ? FString(TEXT("toi")) : Cx->DriverName; };
			if (Gentle && Rough && Gentle != Rough)
				LastLines.Add(FString::Printf(TEXT("Gestion des pneus : le plus doux, %s ; le plus dur, %s."), *Who(Gentle), *Who(Rough)));
		}
		// règlement : rumeurs de refonte, règlement de l'an prochain
		TArray<TPair<FString, FString>> RegMails;
		Factory->EraAfterRace(G->Round, Season->NumRounds(), Season->IsOver(), RegMails, LastLines);
		Career->RaceMails(R, LastLines);
		// compte rendu de l'ingénieur : indispensable après une course simulée, utile après une course jouée
		{
			const bool bSim = Car->RaceTime < 1.f;
			FString Body = FString::Printf(TEXT("%s · %d tours · %s\n\nTa course : parti P%d, %s."), bSim ? TEXT("Course simulée") : TEXT("Course jouée"), TotalLaps,
				bRainStart || WxChangeAt >= 0.f ? TEXT("pluie pendant la course") : TEXT("piste sèche"), GridSlot + 1,
				Car->bOut ? (Car->OutWhy == TEXT("moteur") ? TEXT("abandon sur casse moteur") : Car->OutWhy == TEXT("drapeau noir") ? TEXT("disqualifié (drapeau noir)") : TEXT("abandon sur accident")) : *FString::Printf(TEXT("arrivé P%d (%s)"), Pos, Pos == 1 ? TEXT("victoire") : *ResultGap(Car)));
			FString Plan;
			for (const TPair<TCHAR, int32>& S : Stints) Plan += FString::Printf(TEXT("%s%s %d t."), Plan.IsEmpty() ? TEXT("") : TEXT(" → "), *TyreName(S.Key), S.Value);
			if (!Plan.IsEmpty()) Body += FString::Printf(TEXT("\nStratégie prévue : %s · %d arrêt%s."), *Plan, Car->Stops, Car->Stops > 1 ? TEXT("s") : TEXT(""));
			if (EngineFailAt.Contains(Car.Get()) && !Car->bOut) Body += TEXT("\nEnnui moteur : de la puissance perdue en fin de course.");
			if (Car->PenSum > 0.f) Body += FString::Printf(TEXT("\nPénalités : %.0f s%s."), Car->PenSum, Car->Penalty < Car->PenSum ? *FString::Printf(TEXT(" (dont %.0f s purgées au stand)"), Car->PenSum - Car->Penalty) : TEXT(""));
			if (R.MatePos > 0 && Order.IsValidIndex(R.MatePos - 1))
				Body += FString::Printf(TEXT("\nTon équipier %s : %s."), *Order[R.MatePos - 1]->DriverName, Order[R.MatePos - 1]->bOut ? TEXT("abandon") : *FString::Printf(TEXT("P%d"), R.MatePos));
			Body += TEXT("\n\nPodium : ");
			for (int32 k = 0; k < 3 && k < Order.Num(); ++k) Body += FString::Printf(TEXT("%s%d. %s (%s)"), k ? TEXT(" · ") : TEXT(""), k + 1, Order[k] == Car ? TEXT("toi") : *Order[k]->DriverName, *Order[k]->TeamName);
			const AGPMCar* FastC = nullptr;
			for (const AGPMCar* C : Order) if (!C->bOut && C->BestLap > 0.f && (!FastC || C->BestLap < FastC->BestLap)) FastC = C;
			if (FastC) Body += FString::Printf(TEXT("\nMeilleur tour : %s, %d:%06.3f."), FastC == Car ? TEXT("toi") : *FastC->DriverName, int32(FastC->BestLap) / 60, FMath::Fmod(FastC->BestLap, 60.f));
			FString Outs;
			for (const AGPMCar* C : Order) if (C->bOut && C != Car) Outs += FString::Printf(TEXT("%s%s (%s)"), Outs.IsEmpty() ? TEXT("") : TEXT(", "), *C->DriverName, C->OutWhy == TEXT("moteur") ? TEXT("moteur") : C->OutWhy == TEXT("drapeau noir") ? TEXT("disqualifié") : TEXT("accident"));
			if (!Outs.IsEmpty()) Body += TEXT("\nAbandons : ") + Outs + TEXT(".");
			if (bSCUsed) Body += TEXT("\nLa voiture de sécurité est sortie.");
			Career->SendMail(TEXT("equipe"), TEXT("Ingénieur de course"), FString::Printf(TEXT("Compte rendu : GP %s"), *R.Circuit), Body);
		}
		// fin de saison : bilan financier et palmarès
		if (Season->IsOver())
		{
			int32 Wins = 0, Pods = 0, MyD = 0, MyT = 0, MyPts = 0;
			for (const FGPMRaceResult& Rr : G->Results) { Wins += Rr.Pos == 1; Pods += Rr.Pos >= 1 && Rr.Pos <= 3; }
			const TArray<FGPMStanding> Dr = Season->Drivers(), Tm = Season->Teams();
			for (int32 k = 0; k < Dr.Num(); ++k) if (Dr[k].bPlayer) { MyD = k + 1; MyPts = Dr[k].Pts; }
			for (int32 k = 0; k < Tm.Num(); ++k) if (Tm[k].bPlayer) MyT = k + 1;
			// vitrine : titres et records de saison
			if (MyD == 1) { G->Rec.FindOrAdd(TEXT("dtitles"))++; G->Trophies.Add(FString::Printf(TEXT("%d|Championnat des pilotes|0"), G->Year)); }
			if (MyT == 1) { G->Rec.FindOrAdd(TEXT("ttitles"))++; G->Trophies.Add(FString::Printf(TEXT("%d|Championnat des écuries|0"), G->Year)); }
			G->Rec.Add(TEXT("bestpts"), FMath::Max(G->Rec.FindRef(TEXT("bestpts")), MyPts));
			G->Rec.Add(TEXT("bestwins"), FMath::Max(G->Rec.FindRef(TEXT("bestwins")), Wins));
			G->Honours.Add(FString::Printf(TEXT("%d : pilote P%d (%d pts), écurie P%d · %d victoire%s, %d podium%s · champion : %s (%s)%s"), G->Year, MyD, MyPts, MyT, Wins, Wins > 1 ? TEXT("s") : TEXT(""),
				Pods, Pods > 1 ? TEXT("s") : TEXT(""), Dr.Num() ? *Dr[0].Name : TEXT("?"), Tm.Num() ? *Tm[0].Name : TEXT("?"), MyD == 1 && MyT == 1 ? TEXT(" · DOUBLÉ pilotes et écuries") : MyD == 1 ? TEXT(" · TITRE PILOTES") : MyT == 1 ? TEXT(" · TITRE ÉCURIES") : TEXT("")));
			FString Pal;
			for (int32 k = G->Honours.Num() - 1; k >= 0; --k) Pal += (Pal.IsEmpty() ? TEXT("• ") : TEXT("\n• ")) + G->Honours[k];
			Career->SendMail(TEXT("equipe"), TEXT("Service de presse"), FString::Printf(TEXT("Palmarès de %s"), *G->TeamName), Pal);
			// carrière à durée fixée : bilan de fin de mandat (on peut continuer ensuite, sans limite)
			if (UGPMCareerSubsystem::CareerLen > 0 && G->Year - 2026 == UGPMCareerSubsystem::CareerLen)
			{
				const int32 Ti = G->Rec.FindRef(TEXT("dtitles")) + G->Rec.FindRef(TEXT("ttitles")), Wi = G->Rec.FindRef(TEXT("wins"));
				const TCHAR* Grade = Ti >= 2 ? TEXT("Légende du sport") : Ti == 1 ? TEXT("Champion") : Wi >= 5 ? TEXT("Grand directeur") : Wi >= 1 ? TEXT("Vainqueur de Grands Prix") : TEXT("Bâtisseur");
				Career->SendMail(TEXT("direction"), TEXT("Conseil d'administration"), TEXT("Fin de ton mandat"), FString::Printf(
					TEXT("Ton mandat de %d saisons est terminé. Verdict du conseil : %s.\n\n%d Grands Prix, %d victoires, %d podiums, %d poles, %d titre(s).\n\nLe conseil te propose de rester : la carrière continue sans limite (onglet Palmarès pour tout revoir)."),
					UGPMCareerSubsystem::CareerLen, Grade, G->Rec.FindRef(TEXT("starts")), Wi, G->Rec.FindRef(TEXT("pods")), G->Rec.FindRef(TEXT("poles")), Ti));
			}
			const float Start = G->FinStart < 0.f ? G->Budget : G->FinStart, Spent = Start + G->FinPrize + G->FinPartner - G->Budget;
			int32 Levels = 0;
			for (const FGPMBuilding& B : Factory->Buildings()) Levels += Factory->Level(B.Id);
			Career->SendMail(TEXT("direction"), TEXT("Direction financière"), FString::Printf(TEXT("Bilan financier %d"), G->Year),
				FString::Printf(TEXT("Budget au début de la saison : %.1f M€\nPrimes de course : +%.1f M€\nPartenaires, sponsors et contrats : %+.1f M€\nDépenses (pièces, chantiers, chefs, transferts, événements) : %+.1f M€\nBudget en fin de saison : %.1f M€\n\nÀ payer à l'intersaison : le motoriste, le salaire de ton équipier et des chefs de pôle, et l'entretien des installations (%.1f M€)."),
					Start, G->FinPrize, G->FinPartner, -Spent, G->Budget, 1.5f * Levels));
		}
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
			// l'équipier roule avec la même voiture que le joueur : il profite aussi des pièces de l'usine et du moral
			C->Perf = C->TeamIdx == Car->TeamIdx ? Factory->PlayerPerf() * (1.f + (Season->State()->Moral - 60) / 4000.f)
				: Data->Teams[C->TeamIdx].Perf + Season->State()->RivalDev.FindRef(Data->Teams[C->TeamIdx].Name);
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
		if (!Season->IsOver() && PreStage() < 3) return;   // la pré-saison d'abord (boutons de l'écran de saison)
		if (Season->IsOver())
		{
			// intersaison : nouvelle saison, puis l'écran de saison sur le premier circuit
			if (Career) Career->Intersaison();
			// entretien des installations pour la nouvelle saison : 1,5 M€ par niveau de bâtiment
			if (UGPMFactorySubsystem* FacU = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>())
			{
				int32 Levels = 0;
				for (const FGPMBuilding& B : FacU->Buildings()) Levels += FacU->Level(B.Id);
				if (Levels > 0)
				{
					FacU->AddMoney(-1.5f * Levels);
					if (Career) Career->SendMail(TEXT("usine"), TEXT("Direction financière"), TEXT("Entretien des installations"),
						FString::Printf(TEXT("L'entretien de nos installations (%d niveaux de bâtiments) coûte %.1f M€ pour la nouvelle saison."), Levels, 1.5f * Levels));
				}
			}
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

void AGPMGameMode::RespawnCars()
{
	for (AGPMCar* C : Cars) if (C) C->Destroy();
	if (SC) { SC->Destroy(); SC = nullptr; }
	Cars.Reset(); Car = nullptr; Order.Reset(); QOrder.Reset(); QTimes.Reset(); SCAhead.Reset();
	EngineSound = nullptr;
	EngineFailAt.Reset(); EngineTold.Reset(); EngineBlown.Reset();
}

void AGPMGameMode::FinishSetup()
{
	UGPMCareerSubsystem* Career = GetGameInstance()->GetSubsystem<UGPMCareerSubsystem>();
	UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>();
	if (!Career || !Season) return;
	Career->CreateCareer();
	RespawnCars();   // les écuries ont changé de place, de couleurs ou de nombre
	bCareer = true; bChampShown = false;
	// carrière recommencée : plus rien de l'ancienne (bilan de course, rencontres déjà faites, sacre déjà joué)
	LastLines.Reset(); MetDrivers.Reset(); CrownYear = -1; LastPts = 0; LastPrize = LastPartner = 0.f; MercatoPage = 0; bNegoFromMercato = false;
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
	if (C->Classified()) return C->FinishTime + C->Penalty;
	const FGPMTrackData* T = C->GetTrack();
	const float Total = (TotalLaps + 1) * (T ? T->WX.Num() : 1);
	return Clock * Total / FMath::Max(1.f, C->bOut ? C->FinProgress : C->Progress());
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
			if (O == C || O->PitPhase || O->bParked) continue;
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
			if (A.PitPhase || B.PitPhase || A.bParked || B.bParked) continue;
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
					if (Close > 14) RequestSafety(false, FMath::FRand() < 0.3f);
					if (Close > 5)
					{
						// dégâts sur l'aileron touché (avant ou arrière), seulement sur un nouveau contact franc
						// commissaires : la voiture qui percute l'arrière d'une autre avec son nez est en tort
						if (Pa > 0 && Pb < 0) ContactPenalty(&A, &B, Close); else if (Pb > 0 && Pa < 0) ContactPenalty(&B, &A, Close);
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

// Commande console : GPM.VSC (voiture de sécurité virtuelle tout de suite)
static FAutoConsoleCommandWithWorldAndArgs GVscCmd(
	TEXT("GPM.VSC"),
	TEXT("Déclenche une voiture de sécurité virtuelle."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
	{
		if (AGPMGameMode* GM = World ? World->GetAuthGameMode<AGPMGameMode>() : nullptr) GM->RequestSafety(true, true);
	}));
