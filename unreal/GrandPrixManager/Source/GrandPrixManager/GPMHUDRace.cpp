// HUD de course : tour des positions façon télé, compte-tours au centre en bas, tour et chronos, messages,
// mini-carte, commandes tactiles et menu pause (Échap / P, ou le bouton II sous la mini-carte).
#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
#include "GPMCareer.h"
#include "GPMTrack.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor RacePanel(0.02f, 0.02f, 0.04f, 0.62f);
	const FLinearColor RaceDim(1.f, 1.f, 1.f, 0.65f);
	const FLinearColor RacePurple(0.75f, 0.4f, 1.f);
	const FLinearColor RaceYellow(1.f, 0.85f, 0.2f);
	FLinearColor TyreColour(TCHAR T)
	{
		return T == 'S' ? FLinearColor(1.f, 0.2f, 0.2f) : T == 'H' ? FLinearColor(0.92f, 0.92f, 0.92f) : T == 'W' ? FLinearColor(0.2f, 0.55f, 1.f) : FLinearColor(1.f, 0.85f, 0.15f);
	}
}

FString AGPMHUD::Code3(const FString& Driver) const
{
	FString Full = Nm(Driver).TrimStartAndEnd();
	int32 Sp = INDEX_NONE;
	if (Full.FindLastChar(TEXT(' '), Sp)) Full = Full.Mid(Sp + 1);
	return Full.Left(3).ToUpper();
}

void AGPMHUD::SetPaused(bool bPause)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;
	UGameplayStatics::SetGamePaused(this, bPause);
	PC->bShowMouseCursor = bPause;
	PC->bEnableClickEvents = bPause;
	if (bPause)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else PC->SetInputMode(FInputModeGameOnly());
}

// Tour des positions : les 3 premiers, puis les voitures autour du joueur, avec la couleur de l'écurie et l'écart
void AGPMHUD::DrawTower(AGPMGameMode* GM, AGPMCar* Car, float X, float Y)
{
	const TArray<AGPMCar*>& Ord = GM->Standings();
	const int32 Num = Ord.Num();
	if (Num == 0) return;
	UFont* Mid = GEngine->GetMediumFont();
	const int32 Me = GM->PositionOf(Car) - 1;
	TArray<int32> Rows;
	if (Me <= 6) { for (int32 k = 0; k < FMath::Min(Num, 8); ++k) Rows.Add(k); }
	else
	{
		for (int32 k = 0; k < 3; ++k) Rows.Add(k);
		Rows.Add(-1);                                 // séparateur
		for (int32 k = FMath::Max(3, Me - 2); k <= FMath::Min(Num - 1, Me + 2); ++k) Rows.Add(k);
	}
	const FGPMTrackData* T = Car->GetTrack();
	const float PerLap = T ? float(FMath::Max(1, T->WX.Num())) : 1.f;
	const float LeadP = Ord[0]->Progress();
	const float RowH = 17.f * Scale, TW = 128.f * Scale, S = Scale * 0.82f;
	DrawRect(RacePanel, X, Y, TW, 18.f * Scale);
	DrawText(FString::Printf(TEXT("TOUR %d/%d"), FMath::Clamp(Ord[0]->Lap, 1, GM->TotalLaps), GM->TotalLaps), FLinearColor::White, X + 6 * Scale, Y + 2 * Scale, Mid, S);
	float Yr = Y + 19.f * Scale;
	for (int32 R : Rows)
	{
		if (R < 0) { DrawRect(RacePanel, X, Yr, TW, 5.f * Scale); Yr += 6.f * Scale; continue; }
		const AGPMCar* C = Ord[R];
		const bool bMe = C == Car;
		DrawRect(bMe ? FLinearColor(1.f, 1.f, 1.f, 0.85f) : RacePanel, X, Yr, TW, RowH - 1.f);
		DrawRect(C->TeamColor, X + 22 * Scale, Yr + 2 * Scale, 3 * Scale, RowH - 4 * Scale);
		const FLinearColor Ink = bMe ? FLinearColor(0.05f, 0.05f, 0.08f) : FLinearColor::White;
		DrawText(FString::FromInt(R + 1), Ink, X + 5 * Scale, Yr + 1.5f * Scale, Mid, S);
		DrawText(Code3(C->DriverName), Ink, X + 30 * Scale, Yr + 1.5f * Scale, Mid, S);
		FString G;
		if (R == 0) G = TEXT("Leader");
		else if (C->bFinished) G = TEXT("Arrivé");
		else
		{
			const float D = (LeadP - C->Progress()) / PerLap;
			G = D >= 1.f ? FString::Printf(TEXT("+%d T"), FMath::FloorToInt(D)) : FString::Printf(TEXT("+%.1f"), D * GM->LapEstimate);
		}
		if (C->PitPhase) G = TEXT("STAND");
		float GW, GH;
		GetTextSize(G, GW, GH, Mid, S);
		DrawText(G, bMe ? Ink : (C->PitPhase ? RaceYellow : RaceDim), X + TW - GW - 6 * Scale, Yr + 1.5f * Scale, Mid, S);
		Yr += RowH;
	}
}

// Compte-tours : diodes de régime, vitesse, rapport, pneus et carburant (en bas au centre)
void AGPMHUD::DrawCluster(AGPMCar* Car, float W, float H)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float CW = 236.f * Scale, CH = 70.f * Scale;
	const float X = (W - CW) / 2, Y = H - CH - (bShowTouch ? 66.f : 12.f) * Scale;
	DrawRect(RacePanel, X, Y, CW, CH);

	// diodes : vert, rouge, bleu ; tout clignote au rupteur
	const float Frac = FMath::Clamp((Car->Rpm() - 6500.f) / 5500.f, 0.f, 1.f);
	const int32 Lit = Car->SpeedKmh() < 2.f ? 0 : FMath::CeilToInt(Frac * 12.f);
	const bool bBlink = Frac > 0.96f && FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 0.16f) < 0.08f;
	const float DW = (CW - 24.f * Scale) / 12.f;
	for (int32 k = 0; k < 12; ++k)
	{
		FLinearColor Col = k < 5 ? FLinearColor(0.15f, 1.f, 0.3f) : k < 9 ? FLinearColor(1.f, 0.15f, 0.15f) : FLinearColor(0.25f, 0.45f, 1.f);
		if (k >= Lit || bBlink) Col = FLinearColor(Col.R * 0.18f, Col.G * 0.18f, Col.B * 0.18f, 0.8f);
		DrawRect(Col, X + 12 * Scale + k * DW + 1.f, Y + 5 * Scale, DW - 2.f, 6 * Scale);
	}

	// vitesse au centre
	const FString Spd = FString::Printf(TEXT("%d"), FMath::RoundToInt(Car->SpeedKmh()));
	float SW, SH;
	GetTextSize(Spd, SW, SH, Big, Scale * 1.7f);
	DrawText(Spd, FLinearColor::White, X + CW / 2 - SW / 2, Y + 14 * Scale, Big, Scale * 1.7f);
	DrawText(TEXT("km/h"), RaceDim, X + CW / 2 - 12 * Scale, Y + 52 * Scale, Mid, Scale * 0.8f);

	// rapport à droite
	const int32 Gr = Car->Gear();
	const float BX = X + CW - 52 * Scale, BY = Y + 16 * Scale;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.1f), BX, BY, 40 * Scale, 46 * Scale);
	const FString GS = Gr == 0 ? FString(TEXT("N")) : FString::FromInt(Gr);
	float GW, GH;
	GetTextSize(GS, GW, GH, Big, Scale * 1.5f);
	DrawText(GS, Frac > 0.9f ? FLinearColor(0.4f, 0.6f, 1.f) : FLinearColor::White, BX + 20 * Scale - GW / 2, BY + 1 * Scale, Big, Scale * 1.5f);

	// pneus et carburant à gauche
	const float Life = FMath::Max(0.f, 1.f - Car->Wear);
	const FLinearColor LifeCol = Life > 0.6f ? FLinearColor(0.3f, 1.f, 0.4f) : Life > 0.4f ? RaceYellow : FLinearColor(1.f, 0.25f, 0.2f);
	const float LX = X + 12 * Scale, LY = Y + 18 * Scale;
	DrawRect(TyreColour(Car->Tyre), LX, LY, 18 * Scale, 18 * Scale);
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f), LX + 4 * Scale, LY + 4 * Scale, 10 * Scale, 10 * Scale);
	DrawText(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Life * 100)), LifeCol, LX + 23 * Scale, LY + 1 * Scale, Mid, Scale * 0.9f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), LX, LY + 22 * Scale, 62 * Scale, 3 * Scale);
	DrawRect(LifeCol, LX, LY + 22 * Scale, 62 * Scale * Life, 3 * Scale);
	// carburant : marge en tours par rapport à l'arrivée
	if (const FGPMTrackData* T = Car->GetTrack())
	{
		AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>();
		const float PerLap = T->WX.Num() * 2.f / 1000.f * 1.7f * Car->UpFuel;
		if (GM && PerLap > 0.f && GM->Phase == AGPMGameMode::EPhase::Race)
		{
			const float FuelLaps = Car->Fuel / PerLap, Need = FMath::Max(0.f, GM->TotalLaps - Car->Lap + 1 - float(Car->Idx) / FMath::Max(1, T->WX.Num()));
			const float Margin = FuelLaps - Need;
			DrawText(FString::Printf(TEXT("Ess. %+.1f T"), Margin), Margin < 0.f ? FLinearColor(1.f, 0.3f, 0.2f) : RaceDim, LX, LY + 28 * Scale, Mid, Scale * 0.75f);
		}
	}
	if (Car->bAbsOn) DrawText(TEXT("ABS"), RaceYellow, X + CW - 92 * Scale, Y + 18 * Scale, Mid, Scale * 0.75f);
	if (Car->bTcOn) DrawText(TEXT("TC"), RaceYellow, X + CW - 92 * Scale, Y + 34 * Scale, Mid, Scale * 0.75f);
	if (Car->bBox) DrawText(TEXT("BOX"), FLinearColor(0.3f, 0.8f, 1.f), X + CW - 92 * Scale, Y + 50 * Scale, Mid, Scale * 0.75f);
}

void AGPMHUD::DrawPause(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0, 0, W, H);
	DrawCentered(TEXT("PAUSE"), FLinearColor::White, W, H * 0.16f, GEngine->GetLargeFont(), Scale * 1.6f);
	const float BW = 220.f * Scale, BH = 34.f * Scale, X = (W - BW) / 2;
	float Y = H * 0.32f;
	const FLinearColor Acc(0.85f, 0.12f, 0.12f);
	const FLinearColor Alt(0.2f, 0.22f, 0.28f);
	if (Btn(TEXT("REPRENDRE"), X, Y, BW, BH, Acc)) { SetPaused(false); return; }
	Y += BH + 8 * Scale;
	if (Btn(TEXT("RECOMMENCER LA COURSE"), X, Y, BW, BH, Alt)) { SetPaused(false); if (GM) GM->Restart(); return; }
	Y += BH + 8 * Scale;
	if (Btn(Car->IsCockpit() ? TEXT("CAMÉRA : COCKPIT") : TEXT("CAMÉRA : ARRIÈRE"), X, Y, BW, BH, Alt)) Car->ToggleCamera();
	Y += BH + 8 * Scale;
	if (Btn(bShowFps ? TEXT("COMPTEUR I/S : OUI") : TEXT("COMPTEUR I/S : NON"), X, Y, BW, BH, Alt)) bShowFps = !bShowFps;
	Y += BH + 8 * Scale;
	if (Btn(GM && GM->bCareer ? TEXT("QUITTER LE GRAND PRIX") : TEXT("MENU PRINCIPAL"), X, Y, BW, BH, Alt)) { SetPaused(false); if (GM) GM->GoHome(); return; }
	DrawCentered(TEXT("Échap ou P : reprendre"), RaceDim, W, Y + BH + 12 * Scale, GEngine->GetMediumFont(), Scale * 0.85f);
}

void AGPMHUD::DrawRace(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float M = 16.f * Scale;
	APlayerController* PC = GetOwningPlayerController();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	const bool bLive = GM && (GM->Phase == AGPMGameMode::EPhase::Race || GM->Phase == AGPMGameMode::EPhase::Lights || GM->Phase == AGPMGameMode::EPhase::Quali);
	if (PC && PC->WasInputKeyJustPressed(EKeys::F)) bShowFps = !bShowFps;
	if (PC && bLive && (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::P))) { SetPaused(!bPaused); return; }
	if (bPaused && !bLive) SetPaused(false);

	if (GM && (GM->Phase == AGPMGameMode::EPhase::QualiDone || GM->Phase == AGPMGameMode::EPhase::Done)) return;

	// Tour et chronos (en haut au centre)
	const float CW = 220.f * Scale, CX = (W - CW) / 2;
	DrawRect(RacePanel, CX, M, CW, 46.f * Scale);
	DrawText(FString::Printf(TEXT("TOUR %d/%d"), FMath::Clamp(Car->Lap, 1, GM ? GM->TotalLaps : 99), GM ? GM->TotalLaps : 0), FLinearColor::White, CX + 10 * Scale, M + 3 * Scale, Mid, Scale);
	DrawText(Chrono(Car->LapTime()), FLinearColor::White, CX + 118 * Scale, M + 3 * Scale, Mid, Scale);
	DrawText(Car->LastLap > 0.f ? TEXT("Dernier") : TEXT("Meilleur"), RaceDim, CX + 10 * Scale, M + 24 * Scale, Mid, Scale * 0.85f);
	const bool bBest = Car->LastLap > 0.f && FMath::IsNearlyEqual(Car->LastLap, Car->BestLap);
	DrawText(Chrono(Car->LastLap > 0.f ? Car->LastLap : Car->BestLap), bBest ? RacePurple : FLinearColor::White, CX + 70 * Scale, M + 24 * Scale, Mid, Scale * 0.85f);
	DrawText(Chrono(Car->BestLap), RacePurple, CX + 150 * Scale, M + 24 * Scale, Mid, Scale * 0.7f);

	if (GM && GM->Phase == AGPMGameMode::EPhase::Quali)
	{
		DrawCentered(TEXT("QUALIFICATIONS · TOUR LANCÉ"), RaceYellow, W, M + 52 * Scale, Mid, Scale);
		float Pole = 1e9f;
		for (const TPair<AGPMCar*, float>& Q : GM->QTimes) if (Q.Key != Car) Pole = FMath::Min(Pole, Q.Value);
		if (Pole < 1e8f)
		{
			DrawRect(RacePanel, M, M, 150 * Scale, 40 * Scale);
			DrawText(TEXT("Pole à battre"), RaceDim, M + 8 * Scale, M + 3 * Scale, Mid, Scale * 0.8f);
			DrawText(Chrono(Pole), RacePurple, M + 8 * Scale, M + 18 * Scale, Mid, Scale * 1.05f);
		}
	}
	else if (GM && GM->NumCars() > 0)
	{
		DrawTower(GM, Car, M, M);
		if (Car->Penalty > 0.f) DrawText(FString::Printf(TEXT("Pénalité +%.0f s"), Car->Penalty), FLinearColor(1.f, 0.3f, 0.2f), M, H * 0.62f, Mid, Scale);
		if (Car->PitPhase) DrawCentered(Car->PitPhase == 2 ? TEXT("ARRÊT AU STAND") : TEXT("LIMITEUR 80 km/h"), RaceYellow, W, H * 0.24f, Mid, Scale * 1.4f);
		if (GM->SafetyOut())
		{
			DrawRect(FLinearColor(1.f, 0.75f, 0.05f, 0.85f), (W - 260 * Scale) / 2, H * 0.17f, 260 * Scale, 26 * Scale);
			DrawCentered(TEXT("VOITURE DE SÉCURITÉ"), FLinearColor(0.05f, 0.05f, 0.05f), W, H * 0.17f + 3 * Scale, Mid, Scale * 1.1f);
		}
		if (GM->RadioT > 0.f)
		{
			float RW, RH;
			GetTextSize(GM->Radio, RW, RH, Mid, Scale);
			const float RX = (W - RW) / 2 - 10 * Scale, RY = M + 52 * Scale;
			DrawRect(FLinearColor(0.02f, 0.1f, 0.18f, 0.75f), RX, RY, RW + 20 * Scale, RH + 6 * Scale);
			DrawRect(FLinearColor(0.3f, 0.75f, 1.f), RX, RY, 3 * Scale, RH + 6 * Scale);
			DrawText(GM->Radio, FLinearColor(0.7f, 0.92f, 1.f), RX + 10 * Scale, RY + 3 * Scale, Mid, Scale);
		}
		if (GM->Phase == AGPMGameMode::EPhase::Lights) DrawLights(GM->LightsOn(), W);
		else if (GM->Clock < 1.5f) DrawCentered(TEXT("GO !"), FLinearColor(0.2f, 1.f, 0.3f), W, H * 0.3f, Big, Scale * 3.f);
		if (Car->bFinished) DrawCentered(FString::Printf(TEXT("Arrivée : P%d"), GM->PositionOf(Car)), FLinearColor::White, W, H * 0.3f, Big, Scale * 2.f);
	}

	DrawCluster(Car, W, H);

	// Mini-carte (en haut à droite) et bouton pause dessous
	const float MS = 112.f * Scale;
	DrawMiniMap(Car, FVector2D(W - M - MS, M + 6 * Scale), MS);
	if (bLive && !bPaused && Btn(TEXT("II"), W - M - 40 * Scale, M + MS + 18 * Scale, 40 * Scale, 28 * Scale, FLinearColor(0.2f, 0.22f, 0.28f))) { SetPaused(true); return; }
	if (bShowFps && GM)
		DrawText(FString::Printf(TEXT("%.0f i/s  ·  %d %%"), GM->Fps, GM->ScreenPct), GM->Fps < 50.f ? FLinearColor(1.f, 0.4f, 0.3f) : FLinearColor(0.4f, 1.f, 0.5f), W - M - 110 * Scale, M + MS + 52 * Scale, Mid, Scale * 0.8f);

	// Commandes tactiles, ou aide clavier
	if (bShowTouch)
	{
		DrawButton(Left, bLeft);
		DrawButton(Right, bRight);
		DrawButton(Brake, bBrake);
		DrawButton(Gas, bGas);
		DrawButton(Box, bBoxBtn || Car->bBox);
	}
	else if (GM && GM->Clock < 20.f)
		DrawText(TEXT("Flèches/ZQSD : conduire   C : caméra   B : stand   R : replacer   P : pause   F : i/s   T : tactile"), FLinearColor(1, 1, 1, 0.55f), M, H - 22 * Scale, Mid, Scale * 0.8f);

	if (bPaused) DrawPause(GM, Car, W, H);
}
