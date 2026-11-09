// Mini cinématiques (cine.js) : seule la caméra bouge, bandes noires et titres dessinés par le HUD.
// Survol du circuit avant les feux (drone, travelling le long de la grille, grue derrière la voiture),
// tour d'honneur autour de la voiture après un podium, bienvenue au QG, survol lent derrière les menus.
#include "GPMGameMode.h"
#include "GPMCar.h"
#include "GPMTrack.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

namespace
{
	constexpr float ShotLen = 3.4f;            // durée d'un plan du survol (s)
	constexpr float PodiumLen = 7.f, WelcomeLen = 5.5f;
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
			const float A = CineT * 0.3f;
			Eye = CarPos + FVector(FMath::Cos(A) * 620.f, FMath::Sin(A) * 620.f, 210.f);
			const FVector Dir = (CarPos - Eye).GetSafeNormal2D();
			Look = CarPos + FVector(0, 0, 40.f) - FVector::CrossProduct(FVector::UpVector, Dir) * 330.f;
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
