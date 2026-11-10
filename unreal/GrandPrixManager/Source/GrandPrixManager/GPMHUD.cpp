#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
#include "GPMSeason.h"
#include "GPMFactory.h"
#include "GPMDataSubsystem.h"
#include "GPMCareer.h"
#include "GPMUI.h"
#include "Engine/GameInstance.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor HudPanel(0.f, 0.f, 0.f, 0.45f);
	const FLinearColor HudAccent(0.85f, 0.12f, 0.12f, 1.f);
}

AGPMHUD::AGPMHUD()
{
#if PLATFORM_IOS || PLATFORM_ANDROID
	bShowTouch = true;
#endif
}

void AGPMHUD::Layout(float W, float H)
{
	// Tailles pensées pour 844 × 390 (paysage), mises à l'échelle selon la hauteur réelle
	Scale = H / 390.f;
	const float B = 86.f * Scale, M = 18.f * Scale, Y = H - B - M;
	Left = { FVector2D(M, Y), FVector2D(B, B), TEXT("<") };
	Right = { FVector2D(M * 2 + B, Y), FVector2D(B, B), TEXT(">") };
	Gas = { FVector2D(W - M - B, Y - B * 0.35f), FVector2D(B, B * 1.35f), TEXT("GAZ") };
	Brake = { FVector2D(W - M * 2 - B * 2, Y), FVector2D(B, B), TEXT("FREIN") };
	Box = { FVector2D((W - B * 1.3f) / 2, H - M - B * 0.5f), FVector2D(B * 1.3f, B * 0.5f), TEXT("STAND") };
	// radio du muret : 3 boutons à gauche, au-dessus des flèches
	for (int32 k = 0; k < 3; ++k) RadioBtn[k] = { FVector2D(M, H * 0.47f + k * 32.f * Scale), FVector2D(112.f * Scale, 28.f * Scale), nullptr };
}

void AGPMHUD::ReadTouches(AGPMCar* Car)
{
	bLeft = bRight = bGas = bBrake = bBoxBtn = false;
	for (bool& bDown : bRadioDown) bDown = false;
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !bShowTouch) { Car->TouchSteer = 0.f; Car->TouchGas = Car->TouchBrake = 0.f; Car->bTouchActive = false; return; }
	auto Inside = [](const FButton& B, float X, float Y) { return X >= B.Pos.X && X <= B.Pos.X + B.Size.X && Y >= B.Pos.Y && Y <= B.Pos.Y + B.Size.Y; };
	bool bAny = false;
	for (int32 i = 0; i < 10; ++i)
	{
		float X, Y; bool bPressed;
		PC->GetInputTouchState(ETouchIndex::Type(i), X, Y, bPressed);
		if (!bPressed) continue;
		bAny = true;
		bLeft |= Inside(Left, X, Y);
		bRight |= Inside(Right, X, Y);
		bGas |= Inside(Gas, X, Y);
		bBrake |= Inside(Brake, X, Y);
		bBoxBtn |= Inside(Box, X, Y);
		for (int32 k = 0; k < 3; ++k) bRadioDown[k] |= Inside(RadioBtn[k], X, Y);
	}
	if (bBoxBtn && !bBoxWas)
		if (AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>()) GM->ToggleBox();
	bBoxWas = bBoxBtn;
	for (int32 k = 0; k < 3; ++k)
	{
		if (bRadioDown[k] && !bRadioWas[k])
			if (AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>()) GM->RadioCall(k);
		bRadioWas[k] = bRadioDown[k];
	}
	Car->bTouchActive = bAny;
	Car->TouchSteer = (bRight ? 1.f : 0.f) - (bLeft ? 1.f : 0.f);
	Car->TouchGas = bGas ? 1.f : 0.f;
	Car->TouchBrake = bBrake ? 1.f : 0.f;
}

void AGPMHUD::DrawButton(const FButton& B, bool bDown)
{
	DrawRect(bDown ? FLinearColor(1, 1, 1, 0.35f) : FLinearColor(1, 1, 1, 0.12f), B.Pos.X, B.Pos.Y, B.Size.X, B.Size.Y);
	float TW, TH;
	GetTextSize(B.Label, TW, TH, GEngine->GetMediumFont(), Scale * 1.2f);
	DrawText(B.Label, FLinearColor(1, 1, 1, 0.85f), B.Pos.X + (B.Size.X - TW) / 2, B.Pos.Y + (B.Size.Y - TH) / 2, GEngine->GetMediumFont(), Scale * 1.2f);
}

FString AGPMHUD::Chrono(float S) const
{
	if (S <= 0.f) return TEXT("--:--.---");
	const int32 M = FMath::FloorToInt(S / 60.f);
	return FString::Printf(TEXT("%d:%06.3f"), M, S - M * 60.f);
}

void AGPMHUD::DrawMiniMap(AGPMCar* Car, FVector2D O, float Size)
{
	const FGPMTrackData* T = Car->GetTrack();
	if (!T || T->WX.Num() < 4) return;
	// Cadre de la carte (repère du jeu web : x vers la droite, z vers le bas de l'écran)
	float MinX = 1e9f, MaxX = -1e9f, MinZ = 1e9f, MaxZ = -1e9f;
	for (int32 i = 0; i < T->WX.Num(); i += 8) { MinX = FMath::Min(MinX, T->WX[i]); MaxX = FMath::Max(MaxX, T->WX[i]); MinZ = FMath::Min(MinZ, T->WZ[i]); MaxZ = FMath::Max(MaxZ, T->WZ[i]); }
	const float K = Size / FMath::Max(MaxX - MinX, MaxZ - MinZ);
	auto P = [&](float X, float Z) { return FVector2D(O.X + (X - MinX) * K + (Size - (MaxX - MinX) * K) / 2, O.Y + (Z - MinZ) * K + (Size - (MaxZ - MinZ) * K) / 2); };
	DrawRect(HudPanel, O.X - 6 * Scale, O.Y - 6 * Scale, Size + 12 * Scale, Size + 12 * Scale);
	const int32 N = T->WX.Num(), Step = 8;
	for (int32 i = 0; i < N; i += Step)
	{
		const int32 j = (i + Step) % N;
		const FVector2D A = P(T->WX[i], T->WZ[i]), B = P(T->WX[j], T->WZ[j]);
		DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(1, 1, 1, 0.8f), 2.f * Scale);
	}
	for (const AGPMCar* Other : Car->Field)
		if (Other && Other != Car) { const FVector2D Q = P(Other->X, Other->Z); DrawRect(FLinearColor(1, 1, 1, 0.9f), Q.X - 2 * Scale, Q.Y - 2 * Scale, 4 * Scale, 4 * Scale); }
	const FVector2D C = P(Car->X, Car->Z);
	DrawRect(HudAccent, C.X - 4 * Scale, C.Y - 4 * Scale, 8 * Scale, 8 * Scale);
}

void AGPMHUD::DrawCentered(const FString& Text, FLinearColor Color, float W, float Y, UFont* Font, float S)
{
	float TW, TH;
	GetTextSize(Text, TW, TH, Font, S);
	DrawText(Text, Color, (W - TW) / 2, Y, Font, S);
}

// Feux de départ : 5 colonnes qui s'allument en rouge une par une, puis s'éteignent toutes au départ
void AGPMHUD::DrawLights(int32 On, float W)
{
	const float R = 26.f * Scale, Sp = 14.f * Scale, Total = 5 * R * 2 + 4 * Sp, X0 = (W - Total) / 2, Y0 = 92.f * Scale;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f), X0 - Sp, Y0 - Sp, Total + 2 * Sp, R * 2 + 2 * Sp);
	for (int32 k = 0; k < 5; ++k)
		DrawRect(k < On ? FLinearColor(1.f, 0.05f, 0.05f) : FLinearColor(0.15f, 0.04f, 0.04f), X0 + k * (R * 2 + Sp), Y0, R * 2, R * 2);
}

FString AGPMHUD::Gap(float S) const
{
	return FString::Printf(TEXT("+%.3f"), S);
}

// Bouton tactile/souris simple : renvoie vrai au moment où on appuie dessus
bool AGPMHUD::Clicked(float X, float Y, float BW, float BH)
{
	return bTap && TapPos.X >= X && TapPos.X <= X + BW && TapPos.Y >= Y && TapPos.Y <= Y + BH;
}

void AGPMHUD::DrawBtn(const FString& Label, float X, float Y, float BW, float BH, FLinearColor Col)
{
	DrawRect(Col, X, Y, BW, BH);
	float TW, TH;
	GetTextSize(Label, TW, TH, GEngine->GetMediumFont(), Scale);
	DrawText(Label, FLinearColor::White, X + (BW - TW) / 2, Y + (BH - TH) / 2, GEngine->GetMediumFont(), Scale);
}

void AGPMHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UI.IsValid()) { UI->Shutdown(); UI.Reset(); }
	Super::EndPlay(EndPlayReason);
}

void AGPMHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	AGPMCar* Car = Cast<AGPMCar>(GetOwningPawn());
	if (!Car) return;

	const float W = Canvas->ClipX, H = Canvas->ClipY;
	Layout(W, H);
	APlayerController* PC = GetOwningPlayerController();
	if (PC && PC->WasInputKeyJustPressed(EKeys::T)) bShowTouch = !bShowTouch;
	ReadTouches(Car);

	AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>();
	{
		// appui (doigt ou clic) : position au moment où l'on touche l'écran
		float TX = 0.f, TY = 0.f; bool bDown = false;
		if (PC) PC->GetInputTouchState(ETouchIndex::Touch1, TX, TY, bDown);
		// clic de souris (si le clic n'est pas déjà converti en toucher)
		if (PC && !bDown && PC->IsInputKeyDown(EKeys::LeftMouseButton) && PC->GetMousePosition(TX, TY)) bDown = true;
		bTap = bDown && !bTapWas; bTapWas = bDown; TapPos = FVector2D(TX, TY);
	}
	// écrans de gestion (accueil, saison, usine, mercato, presse, qualifs, résultats) : interface Slate
	if (!UI.IsValid()) UI = MakeShared<FGPMUI>(this);
	if (GM && UI->Update(GM)) return;
	// cinématiques (survol, tour d'honneur, bienvenue) : bandes noires et titres
	if (GM && GM->Cine != AGPMGameMode::ECine::None && GM->Cine != AGPMGameMode::ECine::Menu) { DrawCine(GM, W, H); return; }
	if (GM && GM->InMenu()) return;

	DrawRace(GM, Car, W, H);
}
