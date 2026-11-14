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
	constexpr float ShotLen = 3.4f;            // durée d'un plan du survol (s)
	constexpr float PodiumLen = 7.f, WelcomeLen = 5.5f, MeetLen = 4.6f, SignLen = 3.9f, FlashAt = 2.65f;
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
	UpdateRoom();   // l'atelier s'allume avant le premier plan (pas d'image du circuit entre deux)
	UpdateCine(0.f);
	PC->SetViewTargetWithBlend(CineCam, Mode == ECine::Podium ? 0.6f : 0.f);
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
	return Cine == ECine::Intro ? FMath::Clamp(FMath::FloorToInt(CineT / ShotLen), 0, 2) : 0;
}

void AGPMGameMode::UpdateCine(float Dt)
{
	if (Cine == ECine::None || !CineCam) return;
	// un écran de gestion ouvert pendant un plan de course : on repasse au survol lent
	if ((Cine == ECine::Intro && Phase != EPhase::Intro) || (Cine == ECine::Podium && Phase != EPhase::Outro)) { EndCine(); return; }
	CineT += Dt;
	if (bRoomOn && Showroom)
	{
		// atelier : plans calculés dans le repère de la pièce ; les plans des écrans glissent doucement de l'un à l'autre
		const int32 Shot = Cine == ECine::Welcome ? 2 : Cine == ECine::Meet ? 3 : Cine == ECine::Sign ? 4 : Phase == EPhase::Home ? 5
			: (Phase == EPhase::Setup || Phase == EPhase::Tuning) ? 0 : 1;
		FVector Eye, Look;
		float Fov = 62.f;
		Showroom->Camera(Shot, CineT, Eye, Look, Fov);
		if ((Shot >= 2 && Shot <= 4) || RoomShot < 0 || Dt <= 0.f) { RoomEye = Eye; RoomLook = Look; RoomFov = Fov; }
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
		const float Len = Cine == ECine::Welcome ? WelcomeLen : Cine == ECine::Meet ? MeetLen : Cine == ECine::Sign ? SignLen : -1.f;
		if (Len > 0.f && CineT >= Len) { EndCine(); StartCine(ECine::Menu); return; }
		CineCam->GetCameraComponent()->SetFieldOfView(RoomFov);
		CineCam->SetActorLocationAndRotation(RoomEye, (RoomLook - RoomEye).Rotation());
		return;
	}
	CineCam->GetCameraComponent()->SetFieldOfView(62.f);
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
	FVector Eye = Center, Look = Center;

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
		const float U = FMath::Clamp((CineT - Shot * ShotLen) / ShotLen, 0.f, 1.f);
		if (Shot == 0)
		{
			// drone : descend en tournant au-dessus de la grille
			const float A = 0.9f + U * 0.7f;
			const float R = FMath::Lerp(Rad * 0.8f, Rad * 0.35f, U);
			Eye = CarPos + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, FMath::Lerp(Rad * 0.45f, 6000.f, U));
			Look = FMath::Lerp(Center, CarPos, U);
		}
		else if (Shot == 1)
		{
			// travelling le long de la grille, à hauteur de casque, depuis la pole jusqu'au fond
			const TArray<AGPMCar*>& G = Order;
			if (G.Num() > 0)
			{
				const float F = U * (G.Num() - 1);
				const int32 a = FMath::Clamp(FMath::FloorToInt(F), 0, G.Num() - 1), b = FMath::Min(a + 1, G.Num() - 1);
				const FVector P = FMath::Lerp(G[a]->GetActorLocation(), G[b]->GetActorLocation(), F - a);
				const FVector Side = FVector::CrossProduct(G[a]->GetActorForwardVector(), FVector::UpVector).GetSafeNormal();
				Eye = P + Side * 650.f + FVector(0, 0, 170.f) + G[a]->GetActorForwardVector() * 300.f;
				Look = P + FVector(0, 0, 60.f);
			}
		}
		else
		{
			// grue : de haut derrière la voiture jusqu'à la caméra de poursuite
			Eye = CarPos - Fwd * FMath::Lerp(3500.f, 900.f, U) + FVector(0, 0, FMath::Lerp(2600.f, 260.f, U));
			Look = CarPos + Fwd * 1200.f;
		}
		if (CineT >= ShotLen * 3.f) { EndCine(); return; }
		break;
	}
	case ECine::Podium:
	case ECine::Welcome:
	{
		// tour d'honneur : la caméra tourne autour de la voiture
		const float A = CineT * (Cine == ECine::Podium ? 0.55f : 0.4f) + 0.6f;
		const float R = Cine == ECine::Podium ? 900.f : 700.f;
		Eye = CarPos + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Cine == ECine::Podium ? 230.f : 180.f);
		Look = CarPos + FVector(0, 0, 60.f);
		if (CineT >= (Cine == ECine::Podium ? PodiumLen : WelcomeLen)) { const ECine Was = Cine; EndCine(); if (Was == ECine::Welcome) StartCine(ECine::Menu); return; }
		break;
	}
	default: break;
	}
	CineCam->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
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
