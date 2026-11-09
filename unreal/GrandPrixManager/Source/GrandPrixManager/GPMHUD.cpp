#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
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
}

void AGPMHUD::ReadTouches(AGPMCar* Car)
{
	bLeft = bRight = bGas = bBrake = false;
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
	for (const AGPMCar* O : Car->Field)
		if (O && O != Car) { const FVector2D Q = P(O->X, O->Z); DrawRect(FLinearColor(1, 1, 1, 0.9f), Q.X - 2 * Scale, Q.Y - 2 * Scale, 4 * Scale, 4 * Scale); }
	const FVector2D C = P(Car->X, Car->Z);
	DrawRect(HudAccent, C.X - 4 * Scale, C.Y - 4 * Scale, 8 * Scale, 8 * Scale);
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

	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float M = 16.f * Scale;

	// Vitesse et rapport (en haut à gauche)
	DrawRect(HudPanel, M, M, 150 * Scale, 70 * Scale);
	DrawText(FString::Printf(TEXT("%3.0f"), Car->SpeedKmh()), FLinearColor::White, M + 10 * Scale, M + 4 * Scale, Big, Scale * 1.6f);
	DrawText(TEXT("km/h"), FLinearColor(1, 1, 1, 0.7f), M + 100 * Scale, M + 12 * Scale, Mid, Scale);
	const int32 Gr = Car->Gear();
	DrawText(Gr == 0 ? FString(TEXT("N")) : FString::FromInt(Gr), HudAccent, M + 104 * Scale, M + 34 * Scale, Big, Scale * 1.2f);
	if (Car->bAbsOn || Car->bTcOn)
		DrawText(Car->bAbsOn ? TEXT("ABS") : TEXT("TC"), FLinearColor(1.f, 0.8f, 0.1f), M + 10 * Scale, M + 74 * Scale, Mid, Scale);

	// Tour et chronos (en haut au centre)
	const float CW = 230 * Scale, CX = (W - CW) / 2;
	DrawRect(HudPanel, CX, M, CW, 58 * Scale);
	DrawText(FString::Printf(TEXT("TOUR %d"), FMath::Max(1, Car->Lap)), FLinearColor::White, CX + 10 * Scale, M + 4 * Scale, Mid, Scale * 1.1f);
	DrawText(Chrono(Car->LapTime()), FLinearColor::White, CX + 100 * Scale, M + 4 * Scale, Mid, Scale * 1.1f);
	DrawText(TEXT("Meilleur"), FLinearColor(1, 1, 1, 0.7f), CX + 10 * Scale, M + 30 * Scale, Mid, Scale);
	DrawText(Chrono(Car->BestLap), FLinearColor(0.75f, 0.4f, 1.f), CX + 100 * Scale, M + 30 * Scale, Mid, Scale);

	// Place et compte à rebours
	if (const AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>())
	{
		if (GM->NumCars() > 0)
		{
			DrawRect(HudPanel, M, M + 96 * Scale, 110 * Scale, 44 * Scale);
			DrawText(FString::Printf(TEXT("P%d/%d"), GM->PositionOf(Car), GM->NumCars()), FLinearColor::White, M + 10 * Scale, M + 98 * Scale, Big, Scale * 1.2f);
		}
		if (GM->StartIn > 0.f)
		{
			const FString Cd = GM->StartIn > 3.f ? FString(TEXT("Prêts…")) : FString::FromInt(FMath::CeilToInt(GM->StartIn));
			float TW, TH;
			GetTextSize(Cd, TW, TH, Big, Scale * 3.f);
			DrawText(Cd, HudAccent, (W - TW) / 2, H * 0.32f, Big, Scale * 3.f);
		}
	}

	// Mini-carte (en haut à droite)
	const float MS = 120 * Scale;
	DrawMiniMap(Car, FVector2D(W - M - MS, M + 6 * Scale), MS);

	// Commandes tactiles
	if (bShowTouch)
	{
		DrawButton(Left, bLeft);
		DrawButton(Right, bRight);
		DrawButton(Brake, bBrake);
		DrawButton(Gas, bGas);
	}
	else
	{
		DrawText(TEXT("Flèches/ZQSD : conduire   C : caméra   R : replacer   T : boutons tactiles"), FLinearColor(1, 1, 1, 0.6f), M, H - 28 * Scale, Mid, Scale * 0.9f);
	}
}
