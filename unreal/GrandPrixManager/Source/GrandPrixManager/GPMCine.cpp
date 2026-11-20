// Mini cinématiques (cine.js) : seule la caméra bouge, bandes noires et titres dessinés par le HUD.
// Survol du circuit avant les feux (drone, travelling le long de la grille, grue derrière la voiture),
// tour d'honneur autour de la voiture après un podium, bienvenue au QG, survol lent derrière les menus.
#include "GPMGameMode.h"
#include "GPMCar.h"
#include "GPMTrack.h"
#include "GPMShowroom.h"
#include "GPMDataSubsystem.h"
#include "GPMCareer.h"
#include "GPMSeason.h"
#include "GPMFactory.h"
#include "GPMEngineSound.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

namespace
{
	// fins des plans (s) : survol (drone, grille, casque, aileron, portique des feux, grue), tour d'honneur et sacre (trois plans chacun)
	constexpr int32 IntroN = 6;
	constexpr float IntroCuts[IntroN] = { 3.4f, 6.8f, 9.2f, 11.4f, 14.f, 16.8f };
	constexpr float PodiumCuts[3] = { 2.8f, 5.6f, 8.6f };
	constexpr float CrownCuts[3] = { 3.f, 6.f, 9.f };
	constexpr float WelcomeLen = 5.5f, MeetLen = 4.6f, SignLen = 3.9f, FlashAt = 2.65f;
	constexpr float CutFade = 0.16f;           // fondu au noir de part et d'autre d'un changement de plan (s)

	float Ease(float U) { return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(U, 0.f, 1.f)); }

	// caméra tenue à l'épaule : léger flottement (degrés), lent et sans à-coups
	FRotator Handheld(const FRotator& R, float T, float Amp)
	{
		return R + FRotator(Amp * (0.6f * FMath::Sin(T * 1.13f) + 0.4f * FMath::Sin(T * 2.71f + 1.f)),
			Amp * (0.6f * FMath::Sin(T * 0.87f + 2.f) + 0.4f * FMath::Sin(T * 2.03f)), 0.f);
	}
}

void AGPMGameMode::CineLens(float Focus, float Fstop)
{
	if (!CineCam) return;
	// diaphragme ouvert sur les gros plans : le fond se fond, la voiture ressort (seulement pendant les cinématiques)
	FPostProcessSettings& P = CineCam->GetCameraComponent()->PostProcessSettings;
	CineCam->GetCameraComponent()->PostProcessBlendWeight = 1.f;
	P.bOverride_DepthOfFieldFocalDistance = true; P.DepthOfFieldFocalDistance = Focus;
	P.bOverride_DepthOfFieldFstop = true; P.DepthOfFieldFstop = Fstop;
	P.bOverride_VignetteIntensity = true; P.VignetteIntensity = Focus > 0.f ? 0.45f : 0.32f;
	// rendu « caméra de télé » : flou de mouvement, frange d'objectif, grain léger, contraste et couleurs un peu poussés
	P.bOverride_MotionBlurAmount = true; P.MotionBlurAmount = 0.55f;
	P.bOverride_MotionBlurMax = true; P.MotionBlurMax = 3.f;
	P.bOverride_SceneFringeIntensity = true; P.SceneFringeIntensity = Focus > 0.f ? 0.9f : 0.4f;
	P.bOverride_FilmGrainIntensity = true; P.FilmGrainIntensity = 0.1f;
	P.bOverride_ColorContrast = true; P.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.f);
	P.bOverride_ColorSaturation = true; P.ColorSaturation = FVector4(1.08f, 1.06f, 1.02f, 1.f);
	P.bOverride_BloomIntensity = true; P.BloomIntensity = 0.9f;
}

void AGPMGameMode::StartCine(ECine Mode)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return;
	if (!CineCam)
	{
		CineCam = GetWorld()->SpawnActor<ACameraActor>();
		if (!CineCam) return;
		CineCam->GetCameraComponent()->SetFieldOfView(62.f);
		CineCam->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	Cine = Mode;
	CineT = 0.f;
	if (Car) CineFwd = Car->GetActorForwardVector();
	UpdateRoom();   // l'atelier s'allume avant le premier plan (pas d'image du circuit entre deux)
	UpdateCine(0.f);
	PC->SetViewTargetWithBlend(CineCam, Mode == ECine::Podium ? 0.6f : 0.f);
	if (Mode == ECine::Menu) CineLens(0.f, 4.f);
}

void AGPMGameMode::EndCine()
{
	const ECine Was = Cine;
	Cine = ECine::None;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		if (Car) PC->SetViewTargetWithBlend(Car, Was == ECine::Menu ? 0.f : 0.7f);
	if (Was == ECine::Intro && Phase == EPhase::Intro) { Phase = EPhase::Lights; LightsT = -1.f; }
	if (Was == ECine::Podium && Phase == EPhase::Outro) Phase = EPhase::Done;
}

int32 AGPMGameMode::CineShot() const
{
	const float* Cuts = Cine == ECine::Intro ? IntroCuts : Cine == ECine::Podium ? PodiumCuts : Cine == ECine::Crown ? CrownCuts : nullptr;
	if (!Cuts) return 0;
	const int32 N = Cine == ECine::Intro ? IntroN : 3;
	int32 Shot = 0;
	while (Shot < N - 1 && CineT >= Cuts[Shot]) ++Shot;
	return Shot;
}

float AGPMGameMode::CineCut() const
{
	const float* Cuts = Cine == ECine::Intro ? IntroCuts : Cine == ECine::Podium ? PodiumCuts : Cine == ECine::Crown ? CrownCuts : nullptr;
	if (!Cuts) return 0.f;
	const int32 N = Cine == ECine::Intro ? IntroN : 3;
	float D = 1e3f;
	for (int32 k = 0; k < N - 1; ++k) D = FMath::Min(D, FMath::Abs(CineT - Cuts[k]));
	return FMath::Clamp(1.f - D / CutFade, 0.f, 1.f);
}

void AGPMGameMode::UpdateCine(float Dt)
{
	if (Cine == ECine::None || !CineCam) return;
	// un écran de gestion ouvert pendant un plan de course : on repasse au survol lent
	if ((Cine == ECine::Intro && Phase != EPhase::Intro) || (Cine == ECine::Podium && Phase != EPhase::Outro)) { EndCine(); return; }
	CineT += Dt;
	// fin de saison avec un titre : le sacre au QG avant l'écran « Champion du monde »
	if (Cine == ECine::Menu && Phase == EPhase::Season && !bChampShown && Showroom)
		if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>())
			if (Season->IsOver() && CrownYear != Season->Year())
			{
				const TArray<FGPMStanding> Drv = Season->Drivers(), Tms = Season->Teams();
				const bool bDrv = Drv.Num() && Drv[0].bPlayer, bTeam = Tms.Num() && Tms[0].bPlayer;
				if (bDrv || bTeam)
				{
					CrownYear = Season->Year();
					CineTitle = bDrv ? TEXT("CHAMPION DU MONDE !") : TEXT("CHAMPIONS DES ÉCURIES !");
					CineSub = bDrv ? FString::Printf(TEXT("Saison %d · %d points"), Season->Year(), Drv[0].Pts)
						: FString::Printf(TEXT("%s · saison %d"), *Tms[0].Name, Season->Year());
					EndCine();
					StartCine(ECine::Crown);
					return;
				}
			}
	if (bRoomOn && Showroom)
	{
		// atelier : plans calculés dans le repère de la pièce ; les plans des écrans glissent doucement de l'un à l'autre
		const int32 Shot = Cine == ECine::Welcome ? 2 : Cine == ECine::Meet ? 3 : Cine == ECine::Sign ? 4 : Cine == ECine::Crown ? 6 : Phase == EPhase::Home ? 5
			: (Phase == EPhase::Setup || Phase == EPhase::Tuning) ? 0 : 1;
		FVector Eye, Look;
		float Fov = 62.f;
		Showroom->Camera(Shot, CineT, Eye, Look, Fov);
		if ((Shot >= 2 && Shot <= 4) || Shot == 6 || RoomShot < 0 || Dt <= 0.f) { RoomEye = Eye; RoomLook = Look; RoomFov = Fov; }
		else
		{
			RoomEye = FMath::VInterpTo(RoomEye, Eye, Dt, 2.2f);
			RoomLook = FMath::VInterpTo(RoomLook, Look, Dt, 2.2f);
			RoomFov = FMath::FInterpTo(RoomFov, Fov, Dt, 2.2f);
		}
		RoomShot = Shot;
		// photo de la signature : déclic de l'appareil (le flash blanc est dessiné par le HUD)
		if (Cine == ECine::Sign && CineT - Dt < FlashAt && CineT >= FlashAt && EngineSound && bSetSound)
		{
			EngineSound->Blip(2400.f, 0.04f, 0.07f, true, 0.f, true);
			EngineSound->Blip(1700.f, 0.06f, 0.05f, true, 0.06f, true);
		}
		const float Len = Cine == ECine::Welcome ? WelcomeLen : Cine == ECine::Meet ? MeetLen : Cine == ECine::Sign ? SignLen : Cine == ECine::Crown ? CrownCuts[2] : -1.f;
		if (Len > 0.f && CineT >= Len) { EndCine(); StartCine(ECine::Menu); return; }
		CineCam->GetCameraComponent()->SetFieldOfView(RoomFov);
		const bool bScene = Shot >= 2 && Shot != 5;
		// scènes : mise au point sur la voiture ou les personnages, fond adouci ; écrans de gestion : net partout (plus léger)
		CineLens(bScene ? float((RoomLook - RoomEye).Size()) : 0.f, Cine == ECine::Crown ? 2.2f : 2.8f);
		const FRotator Rot = (RoomLook - RoomEye).Rotation();
		CineCam->SetActorLocationAndRotation(RoomEye, bScene ? Handheld(Rot, CineT, 0.18f) : Rot);
		return;
	}
	const FGPMTrackData* T = CurTrack();
	if (!T || T->WX.Num() == 0) return;
	const int32 N = T->WX.Num();

	// centre et taille du circuit
	FVector Lo(1e9), Hi(-1e9);
	for (int32 i = 0; i < N; i += 8)
	{
		const FVector P = FGPMTrackData::ToWorld(T->WX[i], T->WY[i], T->WZ[i]);
		Lo = Lo.ComponentMin(P); Hi = Hi.ComponentMax(P);
	}
	const FVector Center = (Lo + Hi) * 0.5;
	const float Rad = FMath::Max(15000.f, float((Hi - Lo).Size2D()) * 0.5f);
	const FVector CarPos = Car ? Car->GetActorLocation() : Center;
	const FVector Fwd = Car ? Car->GetActorForwardVector() : FVector::ForwardVector;
	if (Dt > 0.f) CineFwd = FMath::VInterpTo(CineFwd, Fwd, Dt, 2.5f).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(CineFwd, FVector::UpVector).GetSafeNormal();
	FVector Eye = Center, Look = Center;
	float Fov = 62.f, Focus = 0.f, Fstop = 4.f, Shake = 0.f;

	switch (Cine)
	{
	case ECine::Menu:
	{
		if (Phase == EPhase::Setup || Phase == EPhase::Tuning)
		{
			// création de l'écurie, réglages : la voiture tourne sous la caméra, comme à l'atelier,
			// décalée à droite de l'écran (le panneau occupe la gauche)
			// assez loin pour que toute la voiture tienne dans la moitié droite, même vue de profil
			const float A = CineT * 0.3f, Orbit = 1150.f;
			Eye = CarPos + FVector(FMath::Cos(A) * Orbit, FMath::Sin(A) * Orbit, 300.f);
			const FVector Dir = (CarPos - Eye).GetSafeNormal2D();
			Look = CarPos + FVector(0, 0, 40.f) - FVector::CrossProduct(FVector::UpVector, Dir) * (Orbit * 0.6f);
			break;
		}
		// survol lent du circuit derrière les menus
		const float A = CineT * 0.035f;
		Eye = Center + FVector(FMath::Cos(A) * Rad * 0.9f, FMath::Sin(A) * Rad * 0.9f, Rad * 0.32f + 4000.f);
		Look = Center;
		break;
	}
	case ECine::Intro:
	{
		const int32 Shot = CineShot();
		const float S0 = Shot == 0 ? 0.f : IntroCuts[Shot - 1];
		const float U = Ease((CineT - S0) / (IntroCuts[Shot] - S0));
		const FVector CarSide = FVector::CrossProduct(Fwd, FVector::UpVector).GetSafeNormal();
		if (Shot == 0)
		{
			// drone : descend en tournant au-dessus du circuit jusqu'à la grille
			const float A = 0.9f + U * 0.8f;
			const float R = FMath::Lerp(Rad * 0.8f, Rad * 0.3f, U);
			Eye = CarPos + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, FMath::Lerp(Rad * 0.45f, 5000.f, U));
			Look = FMath::Lerp(Center, CarPos, 0.35f + 0.65f * U);
			Fov = FMath::Lerp(70.f, 58.f, U);
			Shake = 0.12f;
		}
		else if (Shot == 1)
		{
			// travelling au téléobjectif le long de la grille, à hauteur de casque, de la pole jusqu'au fond
			const TArray<AGPMCar*>& G = Order;
			if (G.Num() > 0)
			{
				const float F = U * (G.Num() - 1);
				const int32 a = FMath::Clamp(FMath::FloorToInt(F), 0, G.Num() - 1), b = FMath::Min(a + 1, G.Num() - 1);
				const FVector P = FMath::Lerp(G[a]->GetActorLocation(), G[b]->GetActorLocation(), F - a);
				// côté extérieur de la grille (jamais entre les deux files de voitures)
				FVector GSide = FVector::CrossProduct(G[a]->GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
				FVector Mid = FVector::ZeroVector;
				for (const AGPMCar* Gc : G) Mid += Gc->GetActorLocation();
				Mid /= G.Num();
				const float Off = float(FVector::DotProduct(P - Mid, GSide));
				if (Off < 0.f) GSide = -GSide;
				Eye = P + GSide * (FMath::Abs(Off) + 1100.f) + FVector(0, 0, 230.f) + G[a]->GetActorForwardVector() * 450.f;
				Look = P + FVector(0, 0, 55.f);
				Fov = 44.f;
				Focus = float((Look - Eye).Size()); Fstop = 2.8f;
				Shake = 0.2f;
			}
		}
		else if (Shot == 2)
		{
			// casque du pilote dans le cockpit, au téléobjectif : la mise au point passe du volant au casque
			Eye = CarPos + CarSide * FMath::Lerp(190.f, 160.f, U) + Fwd * 140.f + FVector(0, 0, 135.f);
			Look = CarPos + Fwd * 30.f + FVector(0, 0, 88.f);
			Fov = FMath::Lerp(24.f, 21.f, U);
			Focus = float((Look - Eye).Size()) * FMath::Lerp(0.8f, 1.f, Ease(U * 2.5f)); Fstop = 1.4f;
			Shake = 0.22f;
		}
		else if (Shot == 3)
		{
			// gros plan au ras du sol sur l'aileron avant de ta voiture, la caméra glisse lentement vers le cockpit
			Eye = CarPos + Fwd * FMath::Lerp(720.f, 520.f, U) + CarSide * FMath::Lerp(330.f, 250.f, U) + FVector(0, 0, FMath::Lerp(40.f, 85.f, U));
			Look = CarPos + Fwd * FMath::Lerp(110.f, 0.f, U) + FVector(0, 0, 50.f);
			Fov = FMath::Lerp(36.f, 31.f, U);
			Focus = float((Look - Eye).Size()); Fstop = 1.8f;
			Shake = 0.25f;
		}
		else if (Shot == 4 && Order.Num() > 0)
		{
			// plan de départ de la télé : depuis le portique des feux, au long foyer, toute la grille écrasée par la perspective
			const AGPMCar* Pole = Order[0];
			const FVector PF = Pole->GetActorForwardVector();
			const FVector PS = FVector::CrossProduct(PF, FVector::UpVector).GetSafeNormal();
			Eye = Pole->GetActorLocation() + PF * FMath::Lerp(2900.f, 2750.f, U) + PS * 260.f + FVector(0, 0, 560.f);
			Look = Pole->GetActorLocation() - PF * 3000.f + FVector(0, 0, 60.f);
			Fov = FMath::Lerp(15.f, 13.f, U);
			Focus = float((Car ? CarPos - Eye : Look - Eye).Size()); Fstop = 5.6f;
			Shake = 0.06f;
		}
		else
		{
			// grue : de haut derrière la voiture jusqu'à la caméra de poursuite
			Eye = CarPos - Fwd * FMath::Lerp(3500.f, 900.f, U) + FVector(0, 0, FMath::Lerp(2600.f, 260.f, U));
			Look = CarPos + Fwd * 1200.f;
			Fov = FMath::Lerp(55.f, 62.f, U);
		}
		if (CineT >= IntroCuts[IntroN - 1]) { EndCine(); return; }
		break;
	}
	case ECine::Podium:
	{
		// tour d'honneur en trois plans : la voiture vient vers la caméra, tour de la voiture, grue qui s'élève
		const int32 Shot = CineShot();
		const float S0 = Shot == 0 ? 0.f : PodiumCuts[Shot - 1];
		const float U = Ease((CineT - S0) / (PodiumCuts[Shot] - S0));
		if (Shot == 0)
		{
			Eye = CarPos + CineFwd * FMath::Lerp(820.f, 1100.f, U) + Side * 140.f + FVector(0, 0, FMath::Lerp(55.f, 85.f, U));
			Look = CarPos + FVector(0, 0, 60.f);
			Fov = 36.f;
			Focus = float((Look - Eye).Size()); Fstop = 2.f;
			Shake = 0.3f;
		}
		else if (Shot == 1)
		{
			const float A = FMath::Lerp(0.6f, 2.4f, U);
			Eye = CarPos + (CineFwd * FMath::Cos(A) + Side * FMath::Sin(A)) * 680.f + FVector(0, 0, 140.f);
			Look = CarPos + FVector(0, 0, 55.f);
			Fov = 50.f;
			Focus = float((Look - Eye).Size()); Fstop = 2.8f;
			Shake = 0.15f;
		}
		else
		{
			Eye = CarPos - CineFwd * FMath::Lerp(600.f, 1500.f, U) + Side * 250.f + FVector(0, 0, FMath::Lerp(160.f, 950.f, U));
			Look = CarPos + CineFwd * 400.f;
			Fov = 60.f;
		}
		if (CineT >= PodiumCuts[2]) { EndCine(); return; }
		break;
	}
	case ECine::Welcome:
	{
		// bienvenue (sans atelier) : la caméra tourne autour de la voiture
		const float A = CineT * 0.4f + 0.6f;
		Eye = CarPos + FVector(FMath::Cos(A) * 700.f, FMath::Sin(A) * 700.f, 180.f);
		Look = CarPos + FVector(0, 0, 60.f);
		Focus = float((Look - Eye).Size()); Fstop = 2.8f;
		if (CineT >= WelcomeLen) { EndCine(); StartCine(ECine::Menu); return; }
		break;
	}
	default: break;
	}
	// cadreur : le point visé suit avec un léger retard (remis à zéro à chaque changement de plan)
	const int32 ShotNow = CineShot();
	if (ShotNow != CineShotSeen || Dt <= 0.f || Cine == ECine::Menu) { CineShotSeen = ShotNow; CineAim = Look; }
	else CineAim = FMath::VInterpTo(CineAim, Look, Dt, 5.f);
	Look = CineAim;
	CineCam->GetCameraComponent()->SetFieldOfView(Fov);
	CineLens(Focus, Fstop);
	const FRotator Rot = (Look - Eye).Rotation();
	CineCam->SetActorLocationAndRotation(Eye, Shake > 0.f ? Handheld(Rot, CineT, Shake) : Rot);
}

// ---------- atelier 3D derrière les écrans de gestion ----------

bool AGPMGameMode::RoomWanted() const
{
	if (!Showroom) return false;
	if (RoomCine()) return true;
	if (Cine != ECine::Menu) return false;
	return Phase == EPhase::Home || Phase == EPhase::Setup || Phase == EPhase::Tuning || Phase == EPhase::Factory || Phase == EPhase::Mercato || (Phase == EPhase::Press && bNegoFromMercato);
}

void AGPMGameMode::UpdateRoom()
{
	if (!Showroom) return;
	const bool bWant = RoomWanted();
	if (bWant != bRoomOn)
	{
		// la pièce est fermée : ni soleil ni brouillard dedans, un peu de lumière d'ambiance seulement
		bRoomOn = bWant;
		RoomShot = -1;
		Showroom->SetShown(bWant);
		if (Sun) Sun->GetLightComponent()->SetVisibility(!bWant);
		if (Fog) Fog->GetComponent()->SetVisibility(!bWant);
		if (SkyLight) SkyLight->GetLightComponent()->SetIntensity(bWant ? 0.15f : 1.f);
	}
	Showroom->SetGala(bWant && Cine == ECine::Crown);
	if (!bWant) return;
	// couleurs et nom de l'écurie du joueur (changent pendant la création de l'écurie)
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		if (Data->Teams.Num() > 0)
		{
			FString Name = Data->Teams[0].Name;
			if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>())
				if (const UGPMSaveGame* G = Season->State())
					if (G->bCreated && !G->TeamName.IsEmpty()) Name = G->TeamName;
			Showroom->SetTeam(Data->Teams[0].Color, Data->Teams[0].Accent, Name);
		}
	// usine : la voiture de la prochaine ère sur le plateau
	if (AGPMCar* D = Showroom->DisplayCar())
	{
		const UGPMFactorySubsystem* Fac = GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>();
		D->SyncModel(bFutCar && Phase == EPhase::Factory && Fac ? Fac->CarEraId(true) : FString());
	}
	const int32 Kind = Cine == ECine::Meet ? 1 : Cine == ECine::Sign ? 2 : 0;
	if (Kind != 0 || RoomScene != 0) Showroom->PlayScene(Kind, CineT);
	RoomScene = Kind;
}

void AGPMGameMode::StartMeet(const FString& Who, const FString& FromTeam)
{
	if (!Showroom || MetDrivers.Contains(Who)) return;
	MetDrivers.Add(Who);
	// le pilote dans la combinaison de son écurie actuelle (blanche s'il est libre), l'agent en costume sombre
	FLinearColor Main(0.86f, 0.87f, 0.9f), Accent(0.12f, 0.13f, 0.16f);
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		for (const FGPMTeam& Tm : Data->Teams)
			if (!FromTeam.IsEmpty() && Tm.Name == FromTeam) { Main = Tm.Color; Accent = Tm.Accent; }
	Showroom->SetCast(AGPMShowroom::Outfit(Main, Accent, Who, false),
		AGPMShowroom::Outfit(FLinearColor(0.07f, 0.075f, 0.09f), FLinearColor(0.45f, 0.06f, 0.08f), Who + TEXT(" agent"), true));
	CineTitle = TEXT("RENDEZ-VOUS AU QG");
	CineSub = FString::Printf(TEXT("%s et son agent"), *Who);
	StartCine(ECine::Meet);
}

void AGPMGameMode::StartSign(const FString& Who)
{
	if (!Showroom) return;
	// le nouveau pilote et toi, tous deux aux couleurs de l'écurie
	FLinearColor Main = FLinearColor::Red, Accent = FLinearColor::White;
	FString Team;
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
		if (Data->Teams.Num() > 0) { Main = Data->Teams[0].Color; Accent = Data->Teams[0].Accent; Team = Data->Teams[0].Name; }
	if (UGPMSeasonSubsystem* Season = GetGameInstance()->GetSubsystem<UGPMSeasonSubsystem>())
		if (const UGPMSaveGame* G = Season->State())
			if (!G->TeamName.IsEmpty()) Team = G->TeamName;
	Showroom->SetCast(AGPMShowroom::Outfit(Main, Accent, Who, false), AGPMShowroom::Outfit(Main, Accent, TEXT("Toi"), false));
	CineTitle = TEXT("SIGNATURE !");
	CineSub = FString::Printf(TEXT("%s rejoint %s"), *Who, *Team);
	StartCine(ECine::Sign);
}
