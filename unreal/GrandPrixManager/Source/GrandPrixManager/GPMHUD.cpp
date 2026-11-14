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
#include "CanvasItem.h"
#include "Engine/Texture.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	const FLinearColor HudPanel(0.f, 0.f, 0.f, 0.45f);
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
	for (int32 k = 0; k < 3; ++k) RadioBtn[k] = { FVector2D(M, H * 0.5f + k * 30.f * Scale), FVector2D(152.f * Scale, 27.f * Scale), nullptr };
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
		for (int32 k = 0; k < 3; ++k) bRadioDown[k] |= Inside(RadioBtn[k], X, Y);
	}
	// plus de bouton STAND : le joueur prend lui-même la voie des stands
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
	DrawRect(bDown ? FLinearColor(1, 1, 1, 0.32f) : FLinearColor(0.02f, 0.025f, 0.045f, 0.45f), B.Pos.X, B.Pos.Y, B.Size.X, B.Size.Y);
	DrawRect(FLinearColor(HudAcc.R, HudAcc.G, HudAcc.B, bDown ? 1.f : 0.6f), B.Pos.X, B.Pos.Y + B.Size.Y - 3.f * Scale, B.Size.X, 3.f * Scale);
	const FVector2D Sz = InkSize(B.Label, 0, 16.f);
	Ink(B.Label, B.Pos.X + (B.Size.X - Sz.X) / 2, B.Pos.Y + (B.Size.Y - Sz.Y) / 2, 0, 16.f, FLinearColor(1, 1, 1, 0.9f));
}

// ---------- charte des écrans dans le HUD ----------

void AGPMHUD::Slab(float X, float Y, float W, float H, FLinearColor Col)
{
	UTexture* Tex = GPMUIStyle::MaskTex(0);
	if (!Tex) { DrawRect(Col, X, Y, W, H); return; }
	// bouts penchés à leur proportion (la texture fait 64 × 24, 10 pixels de bout), milieu étiré
	const float Cap = FMath::Min(W * 0.5f, H * 10.f / 24.f), Cu = 10.f / 64.f;
	DrawTexture(Tex, X, Y, Cap, H, 0.f, 0.f, Cu, 1.f, Col);
	if (W > 2.f * Cap) DrawTexture(Tex, X + Cap, Y, W - 2.f * Cap, H, Cu, 0.f, 1.f - 2.f * Cu, 1.f, Col);
	DrawTexture(Tex, X + W - Cap, Y, Cap, H, 1.f - Cu, 0.f, Cu, 1.f, Col);
}

void AGPMHUD::Paint(int32 Which, float X, float Y, float W, float H, FLinearColor Col)
{
	if (UTexture* Tex = GPMUIStyle::MaskTex(Which)) DrawTexture(Tex, X, Y, W, H, 0.f, 0.f, 1.f, 1.f, Col);
	else if (Which != 3) DrawRect(Col, X, Y, W, H);
}

// Textes du HUD avec les polices du moteur (le chemin Canvas + police Slate n'affichait rien) :
// taille en points comme dans les écrans, graisse simulée par un second passage décalé, ombre légère pour la lisibilité
namespace GPMHudInk
{
	UFont* FontOf(int32 Kind) { return GEngine ? (Kind <= 1 ? GEngine->GetLargeFont() : GEngine->GetMediumFont()) : nullptr; }
	float ScaleOf(const UFont* F, float Px) { return Px / FMath::Max(1.f, F ? F->GetMaxCharHeight() : 16.f); }
}

FVector2D AGPMHUD::InkSize(const FString& Text, int32 Kind, float Size) const
{
	const float Px = Size * Scale * 1.5f;
	const UFont* F = GPMHudInk::FontOf(Kind);
	if (!F || Text.IsEmpty()) return FVector2D(Text.Len() * Px * 0.5f, Px);
	const float K = GPMHudInk::ScaleOf(F, Px);
	return FVector2D(F->GetStringSize(*Text) * K + (Kind == 0 ? Px * 0.06f : 0.f), F->GetStringHeightSize(*Text) * K);
}

void AGPMHUD::Ink(const FString& Text, float X, float Y, int32 Kind, float Size, FLinearColor Col, float Align)
{
	if (!Canvas || Text.IsEmpty()) return;
	UFont* F = GPMHudInk::FontOf(Kind);
	if (!F) return;
	const float Px = Size * Scale * 1.5f, K = GPMHudInk::ScaleOf(F, Px);
	const float Sx = Align > 0.f ? X - InkSize(Text, Kind, Size).X * Align : X;
	const float Bold = Kind == 0 ? Px * 0.06f : Kind == 1 ? Px * 0.035f : 0.f;
	FCanvasTextItem Item(FVector2D(Sx, Y), FText::FromString(Text), F, Col);
	Item.Scale = FVector2D(K, K);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.45f * Col.A), FVector2D(1.f, 1.f) * FMath::Max(1.f, Px * 0.05f));
	Canvas->DrawItem(Item);
	if (Bold > 0.f)
	{
		Item.DisableShadow();
		Item.Position = FVector2D(Sx + Bold, Y);
		Canvas->DrawItem(Item);
	}
}

void AGPMHUD::TyreIcon(TCHAR Tyre, float Cx, float Cy, float Rad)
{
	const FLinearColor Col = Tyre == 'S' ? FLinearColor(1.f, 0.2f, 0.2f) : Tyre == 'H' ? FLinearColor(0.92f, 0.92f, 0.92f) : Tyre == 'W' ? FLinearColor(0.2f, 0.55f, 1.f)
		: Tyre == 'I' ? FLinearColor(0.25f, 0.85f, 0.35f) : FLinearColor(1.f, 0.85f, 0.15f);
	Paint(4, Cx - Rad, Cy - Rad, Rad * 2.f, Rad * 2.f, FLinearColor(0.02f, 0.02f, 0.03f, 1.f));
	Paint(4, Cx - Rad * 0.86f, Cy - Rad * 0.86f, Rad * 1.72f, Rad * 1.72f, Col);
	Paint(4, Cx - Rad * 0.56f, Cy - Rad * 0.56f, Rad * 1.12f, Rad * 1.12f, FLinearColor(0.06f, 0.06f, 0.07f, 1.f));
	const FString L = Tyre == 'S' ? TEXT("S") : Tyre == 'H' ? TEXT("H") : Tyre == 'W' ? TEXT("W") : Tyre == 'I' ? TEXT("I") : TEXT("M");
	const FVector2D Sz = InkSize(L, 0, Rad / Scale * 0.95f);
	Ink(L, Cx - Sz.X * 0.5f, Cy - Sz.Y * 0.5f, 0, Rad / Scale * 0.95f, Col);
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
	// fond en verre fumé avec un filet de la couleur de l'écurie, halo sous le tracé
	DrawRect(HudPanel, O.X - 8 * Scale, O.Y - 8 * Scale, Size + 16 * Scale, Size + 16 * Scale);
	DrawRect(HudAcc, O.X - 8 * Scale, O.Y - 8 * Scale, Size + 16 * Scale, 2.f * Scale);
	const int32 N = T->WX.Num(), Step = 8;
	for (int32 i = 0; i < N; i += Step)
	{
		const int32 j = (i + Step) % N;
		const FVector2D A = P(T->WX[i], T->WZ[i]), B = P(T->WX[j], T->WZ[j]);
		DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(HudAcc.R, HudAcc.G, HudAcc.B, 0.35f), 6.f * Scale);
	}
	for (int32 i = 0; i < N; i += Step)
	{
		const int32 j = (i + Step) % N;
		const FVector2D A = P(T->WX[i], T->WZ[i]), B = P(T->WX[j], T->WZ[j]);
		DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(1, 1, 1, 0.92f), 2.f * Scale);
	}
	// ligne de départ, voitures (couleur de l'écurie), le joueur en grand avec un liseré blanc
	const FVector2D S0 = P(T->WX[0], T->WZ[0]);
	DrawRect(FLinearColor::White, S0.X - 1.5f * Scale, S0.Y - 4.f * Scale, 3.f * Scale, 8.f * Scale);
	for (const AGPMCar* Other : Car->Field)
		if (Other && Other != Car)
		{
			const FVector2D Q = P(Other->X, Other->Z);
			Paint(4, Q.X - 3.f * Scale, Q.Y - 3.f * Scale, 6.f * Scale, 6.f * Scale, Other->TeamColor);
		}
	const FVector2D C = P(Car->X, Car->Z);
	Paint(3, C.X - 9.f * Scale, C.Y - 9.f * Scale, 18.f * Scale, 18.f * Scale, FLinearColor(HudAcc.R, HudAcc.G, HudAcc.B, 0.7f));
	Paint(4, C.X - 5.f * Scale, C.Y - 5.f * Scale, 10.f * Scale, 10.f * Scale, FLinearColor::White);
	Paint(4, C.X - 3.5f * Scale, C.Y - 3.5f * Scale, 7.f * Scale, 7.f * Scale, HudAcc);
}

void AGPMHUD::DrawCentered(const FString& Text, FLinearColor Color, float W, float Y, UFont* Font, float S)
{
	float TW, TH;
	GetTextSize(Text, TW, TH, Font, S);
	DrawText(Text, Color, (W - TW) / 2, Y, Font, S);
}

// Feux de départ : portique sombre de 5 blocs, deux feux ronds par bloc qui s'allument en rouge un bloc après l'autre
// (halo autour des feux allumés), puis s'éteignent tous au départ
void AGPMHUD::DrawLights(int32 On, float W)
{
	const float R = 15.f * Scale, PodW = R * 2.f + 12.f * Scale, PodH = R * 4.f + 20.f * Scale, Sp = 10.f * Scale;
	const float Total = 5 * PodW + 4 * Sp, X0 = (W - Total) / 2, Y0 = 70.f * Scale;
	// poutre du portique et ses bandes jaunes et noires
	DrawRect(FLinearColor(0.03f, 0.03f, 0.035f, 0.95f), X0 - 26.f * Scale, Y0 - 16.f * Scale, Total + 52.f * Scale, 12.f * Scale);
	for (int32 k = 0; k < 14; ++k)
		Paint(1, X0 - 26.f * Scale + k * (Total + 52.f * Scale) / 14.f, Y0 - 16.f * Scale, 7.f * Scale, 12.f * Scale, FLinearColor(1.f, 0.78f, 0.05f, 0.9f));
	for (int32 k = 0; k < 5; ++k)
	{
		const float PX = X0 + k * (PodW + Sp);
		DrawRect(FLinearColor(0.015f, 0.015f, 0.02f, 0.96f), PX, Y0, PodW, PodH);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.08f), PX, Y0, PodW, 2.f * Scale);
		for (int32 Row = 0; Row < 2; ++Row)
		{
			const float Cx = PX + PodW / 2, Cy = Y0 + 10.f * Scale + R + Row * (R * 2.f + 4.f * Scale);
			const bool bLit = k < On;
			if (bLit) Paint(3, Cx - R * 2.6f, Cy - R * 2.6f, R * 5.2f, R * 5.2f, FLinearColor(1.f, 0.08f, 0.04f, 0.55f));
			Paint(4, Cx - R, Cy - R, R * 2.f, R * 2.f, bLit ? FLinearColor(1.f, 0.12f, 0.08f) : FLinearColor(0.16f, 0.05f, 0.05f));
			if (bLit) Paint(4, Cx - R * 0.45f, Cy - R * 0.6f, R * 0.7f, R * 0.7f, FLinearColor(1.f, 0.75f, 0.65f, 0.8f));
		}
	}
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
	// bouton penché, comme dans les écrans de gestion
	Slab(X, Y, BW, BH, Col);
	const FVector2D Sz = InkSize(Label, 1, 12.f);
	Ink(Label, X + (BW - Sz.X) / 2, Y + (BH - Sz.Y) / 2, 1, 12.f, Col.GetLuminance() > 0.55f ? FLinearColor(0.04f, 0.05f, 0.08f) : FLinearColor::White);
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
