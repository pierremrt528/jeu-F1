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
	Box = { FVector2D((W - B * 1.3f) / 2, H - M - B * 0.5f), FVector2D(B * 1.3f, B * 0.5f), TEXT("STAND") };
}

void AGPMHUD::ReadTouches(AGPMCar* Car)
{
	bLeft = bRight = bGas = bBrake = bBoxBtn = false;
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
	}
	if (bBoxBtn && !bBoxWas)
		if (AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>()) GM->ToggleBox();
	bBoxWas = bBoxBtn;
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

// Résultats (comme l'écran de résultats du jeu web) : place, pilote, écurie, écart, meilleur tour, points
void AGPMHUD::DrawResults(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0, 0, W, H);
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	DrawCentered(TEXT("RÉSULTATS"), FLinearColor::White, W, 10 * Scale, Big, Scale * 1.3f);
	const TArray<AGPMCar*>& Order = GM->Standings();
	const float Row = FMath::Min(15.5f * Scale, (H - 70 * Scale) / FMath::Max(1, Order.Num()));
	const float X0 = FMath::Max(10.f, W / 2 - 330 * Scale), Y0 = 46 * Scale, S = Row / 16.f;
	const float Lead = Order.Num() ? GM->ResultTime(Order[0]) : 0.f;
	for (int32 k = 0; k < Order.Num(); ++k)
	{
		const AGPMCar* C = Order[k];
		const float Y = Y0 + k * Row;
		const FLinearColor Ink = C == Car ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor::White;
		if (C == Car) DrawRect(FLinearColor(1, 1, 1, 0.1f), X0 - 6, Y, 660 * Scale, Row);
		DrawRect(C->TeamColor, X0, Y + Row * 0.15f, 4 * Scale, Row * 0.7f);
		DrawText(FString::Printf(TEXT("%d"), k + 1), Ink, X0 + 10 * Scale, Y, Mid, S);
		DrawText(C->DriverName, Ink, X0 + 40 * Scale, Y, Mid, S);
		DrawText(C->TeamName, FLinearColor(1, 1, 1, 0.65f), X0 + 220 * Scale, Y, Mid, S);
		const float Tm = GM->ResultTime(C);
		DrawText(k == 0 ? Chrono(Tm) : Gap(Tm - Lead) + (C->bFinished ? TEXT("") : TEXT(" (est.)")), Ink, X0 + 390 * Scale, Y, Mid, S);
		DrawText(Chrono(C->BestLap), FLinearColor(0.75f, 0.4f, 1.f), X0 + 510 * Scale, Y, Mid, S);
		if (k < 10) DrawText(FString::Printf(TEXT("%d pts"), AGPMGameMode::Points[k]), Ink, X0 + 600 * Scale, Y, Mid, S);
	}
	DrawCentered(TEXT("Entrée ou touche l'écran : recommencer"), FLinearColor(1, 1, 1, 0.7f), W, H - 22 * Scale, Mid, Scale * 0.9f);

	// toucher l'écran (ou clic) : recommencer
	APlayerController* PC = GetOwningPlayerController();
	float TX, TY; bool bDown = false;
	if (PC) PC->GetInputTouchState(ETouchIndex::Touch1, TX, TY, bDown);
	if (bDown && !bWasDown) GM->Restart();
	bWasDown = bDown;
}

// Classement des qualifications : temps, écart avec la pole ; Entrée = course, Retour = refaire le tour
void AGPMHUD::DrawQuali(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), 0, 0, W, H);
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	DrawCentered(TEXT("QUALIFICATIONS"), FLinearColor::White, W, 10 * Scale, Big, Scale * 1.3f);
	const TArray<AGPMCar*>& Q = GM->QOrder;
	const float Row = FMath::Min(15.5f * Scale, (H - 70 * Scale) / FMath::Max(1, Q.Num()));
	const float X0 = FMath::Max(10.f, W / 2 - 280 * Scale), Y0 = 46 * Scale, S = Row / 16.f;
	const float Pole = Q.Num() ? GM->QTimes.FindRef(Q[0]) : 0.f;
	for (int32 k = 0; k < Q.Num(); ++k)
	{
		const AGPMCar* C = Q[k];
		const float Y = Y0 + k * Row, Tm = GM->QTimes.FindRef(Q[k]);
		const FLinearColor Ink = C == Car ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor::White;
		if (C == Car) DrawRect(FLinearColor(1, 1, 1, 0.1f), X0 - 6, Y, 560 * Scale, Row);
		DrawRect(C->TeamColor, X0, Y + Row * 0.15f, 4 * Scale, Row * 0.7f);
		DrawText(FString::Printf(TEXT("%d"), k + 1), Ink, X0 + 10 * Scale, Y, Mid, S);
		DrawText(C->DriverName, Ink, X0 + 40 * Scale, Y, Mid, S);
		DrawText(C->TeamName, FLinearColor(1, 1, 1, 0.65f), X0 + 220 * Scale, Y, Mid, S);
		DrawText(Tm >= 999.f ? FString(TEXT("pas de temps")) : Chrono(Tm), k == 0 ? FLinearColor(0.75f, 0.4f, 1.f) : Ink, X0 + 390 * Scale, Y, Mid, S);
		if (k > 0 && Tm < 999.f) DrawText(Gap(Tm - Pole), FLinearColor(1, 1, 1, 0.65f), X0 + 480 * Scale, Y, Mid, S);
	}
	DrawCentered(TEXT("Entrée ou touche l'écran : course    Retour arrière : refaire le tour"), FLinearColor(1, 1, 1, 0.7f), W, H - 22 * Scale, Mid, Scale * 0.9f);
	APlayerController* PC = GetOwningPlayerController();
	float TX, TY; bool bDown = false;
	if (PC) PC->GetInputTouchState(ETouchIndex::Touch1, TX, TY, bDown);
	if (bDown && !bWasDown) GM->StartRace();
	bWasDown = bDown;
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

	AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>();

	// Tour et chronos (en haut au centre)
	const float CW = 230 * Scale, CX = (W - CW) / 2;
	DrawRect(HudPanel, CX, M, CW, 58 * Scale);
	DrawText(FString::Printf(TEXT("TOUR %d/%d"), FMath::Clamp(Car->Lap, 1, GM ? GM->TotalLaps : 99), GM ? GM->TotalLaps : 0), FLinearColor::White, CX + 10 * Scale, M + 4 * Scale, Mid, Scale * 1.1f);
	DrawText(Chrono(Car->LapTime()), FLinearColor::White, CX + 115 * Scale, M + 4 * Scale, Mid, Scale * 1.1f);
	DrawText(TEXT("Meilleur"), FLinearColor(1, 1, 1, 0.7f), CX + 10 * Scale, M + 30 * Scale, Mid, Scale);
	DrawText(Chrono(Car->BestLap), FLinearColor(0.75f, 0.4f, 1.f), CX + 115 * Scale, M + 30 * Scale, Mid, Scale);

	// Place, feux de départ, résultats
	if (GM && (GM->Phase == AGPMGameMode::EPhase::Quali || GM->Phase == AGPMGameMode::EPhase::QualiDone))
	{
		if (GM->Phase == AGPMGameMode::EPhase::QualiDone) { DrawQuali(GM, Car, W, H); return; }
		DrawCentered(TEXT("QUALIFICATIONS · TOUR LANCÉ"), FLinearColor(1.f, 0.85f, 0.2f), W, M + 66 * Scale, Mid, Scale * 1.1f);
		float Pole = 1e9f;
		for (const TPair<AGPMCar*, float>& Q : GM->QTimes) if (Q.Key != Car) Pole = FMath::Min(Pole, Q.Value);
		if (Pole < 1e8f) DrawText(FString::Printf(TEXT("Pole à battre : %s"), *Chrono(Pole)), FLinearColor(0.75f, 0.4f, 1.f), M, M + 96 * Scale, Mid, Scale);
	}
	else if (GM && GM->NumCars() > 0)
	{
		DrawRect(HudPanel, M, M + 96 * Scale, 110 * Scale, 44 * Scale);
		DrawText(FString::Printf(TEXT("P%d/%d"), GM->PositionOf(Car), GM->NumCars()), FLinearColor::White, M + 10 * Scale, M + 98 * Scale, Big, Scale * 1.2f);
		// pneus : gomme et vie restante (vert > 60 %, jaune > 40 %, rouge dessous)
		const float Life = FMath::Max(0.f, 1.f - Car->Wear);
		const FLinearColor TyreCol = Life > 0.6f ? FLinearColor(0.3f, 1.f, 0.4f) : Life > 0.4f ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor(1.f, 0.25f, 0.2f);
		DrawRect(HudPanel, M, M + 146 * Scale, 110 * Scale, 30 * Scale);
		DrawText(FString::Printf(TEXT("%c  %d %%%s"), Car->Tyre, FMath::RoundToInt(Life * 100), Car->bBox ? TEXT("  BOX") : TEXT("")), TyreCol, M + 10 * Scale, M + 150 * Scale, Mid, Scale);
		if (Car->PitPhase) DrawCentered(Car->PitPhase == 2 ? TEXT("ARRÊT AU STAND") : TEXT("LIMITEUR 80 km/h"), FLinearColor(1.f, 0.85f, 0.2f), W, H * 0.24f, Mid, Scale * 1.4f);
		if (GM->SafetyOut()) DrawCentered(TEXT("VOITURE DE SÉCURITÉ"), FLinearColor(1.f, 0.8f, 0.1f), W, H * 0.18f, Big, Scale * 1.3f);
		if (Car->Penalty > 0.f) DrawText(FString::Printf(TEXT("Pénalité +%.0f s"), Car->Penalty), FLinearColor(1.f, 0.3f, 0.2f), M + 10 * Scale, M + 180 * Scale, Mid, Scale);
		if (GM->RadioT > 0.f) DrawCentered(GM->Radio, FLinearColor(0.6f, 0.9f, 1.f), W, M + 66 * Scale, Mid, Scale * 1.05f);
		if (GM->Phase == AGPMGameMode::EPhase::Lights) DrawLights(GM->LightsOn(), W);
		else if (GM->Clock < 1.5f) DrawCentered(TEXT("GO !"), FLinearColor(0.2f, 1.f, 0.3f), W, H * 0.3f, Big, Scale * 3.f);
		if (Car->bFinished && GM->Phase != AGPMGameMode::EPhase::Done)
			DrawCentered(FString::Printf(TEXT("Arrivée : P%d"), GM->PositionOf(Car)), FLinearColor::White, W, H * 0.3f, Big, Scale * 2.f);
		if (GM->Phase == AGPMGameMode::EPhase::Done) { DrawResults(GM, Car, W, H); return; }
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
		DrawButton(Box, bBoxBtn || Car->bBox);
	}
	else
	{
		DrawText(TEXT("Flèches/ZQSD : conduire   C : caméra   B : stand   R : replacer   T : boutons tactiles"), FLinearColor(1, 1, 1, 0.6f), M, H - 28 * Scale, Mid, Scale * 0.9f);
	}
}
