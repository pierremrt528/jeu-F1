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
	const FLinearColor RaceRain(0.35f, 0.65f, 1.f);
	const FLinearColor RaceRadio(0.3f, 0.75f, 1.f);
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
	DrawDamage(Car, X - 46.f * Scale, Y, CH);
}

// Dégâts : silhouette de la voiture, ailerons colorés (jaune abîmé, rouge cassé, barré s'il est parti)
void AGPMHUD::DrawDamage(AGPMCar* Car, float X, float Y, float H)
{
	if (Car->Dmg < 0.05f && Car->DmgR < 0.05f && !Car->bFlat) return;
	const float PW = 40.f * Scale;
	DrawRect(RacePanel, X, Y, PW, H);
	auto Shade = [](float D, bool bOff) { return bOff ? FLinearColor(1.f, 0.15f, 0.1f) : D > 0.5f ? FLinearColor(1.f, 0.3f, 0.2f) : D > 0.15f ? FLinearColor(1.f, 0.8f, 0.2f) : FLinearColor(0.3f, 1.f, 0.4f); };
	const float Cx = X + PW / 2;
	// aileron avant, coque, aileron arrière
	const FLinearColor Front = Shade(Car->Dmg, Car->bFwOff), Rear = Shade(Car->DmgR, Car->bRwOff);
	DrawRect(Front, Cx - 15 * Scale, Y + 8 * Scale, 30 * Scale, 5 * Scale);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.55f), Cx - 3 * Scale, Y + 15 * Scale, 6 * Scale, 34 * Scale);
	DrawRect(Car->bFlat ? FLinearColor(1.f, 0.3f, 0.2f) : FLinearColor(1.f, 1.f, 1.f, 0.35f), Cx - 13 * Scale, Y + 18 * Scale, 6 * Scale, 9 * Scale);
	DrawRect(Car->bFlat ? FLinearColor(1.f, 0.3f, 0.2f) : FLinearColor(1.f, 1.f, 1.f, 0.35f), Cx + 7 * Scale, Y + 18 * Scale, 6 * Scale, 9 * Scale);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), Cx - 13 * Scale, Y + 37 * Scale, 6 * Scale, 10 * Scale);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), Cx + 7 * Scale, Y + 37 * Scale, 6 * Scale, 10 * Scale);
	DrawRect(Rear, Cx - 10 * Scale, Y + 52 * Scale, 20 * Scale, 5 * Scale);
	if (Car->bFwOff) DrawLine(Cx - 16 * Scale, Y + 14 * Scale, Cx + 16 * Scale, Y + 7 * Scale, FLinearColor::Black, 2.f * Scale);
	if (Car->bRwOff) DrawLine(Cx - 11 * Scale, Y + 58 * Scale, Cx + 11 * Scale, Y + 51 * Scale, FLinearColor::Black, 2.f * Scale);
}

// Pluie à l'écran : traits qui tombent, plus vite et plus penchés avec la vitesse (sans coût 3D)
void AGPMHUD::DrawRain(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	const float Amount = GM ? GM->RainShow : 0.f;
	if (Amount < 0.02f) return;
	const float Tm = GetWorld()->GetTimeSeconds();
	const int32 Count = FMath::RoundToInt(120.f * Amount);
	const float Spd = 1.f + FMath::Max(0.f, Car->V) / 45.f;
	const float Lean = FMath::Clamp(-Car->Steer * 0.35f, -0.35f, 0.35f) + 0.08f;
	for (int32 i = 0; i < Count; ++i)
	{
		const float R1 = FMath::Frac(FMath::Sin(i * 12.9898f) * 43758.547f);
		const float R2 = FMath::Frac(FMath::Sin(i * 78.233f + 1.7f) * 24634.633f);
		const float Len = (16.f + 26.f * R2) * Scale * FMath::Min(2.2f, Spd);
		const float Fall = (700.f + 600.f * R1) * Scale * Spd;
		const float Yp = FMath::Fmod(Tm * Fall + R2 * (H + Len) * 7.f, H + Len) - Len;
		const float Xp = R1 * W + Lean * Yp;
		DrawLine(Xp, Yp, Xp + Lean * Len, Yp + Len, FLinearColor(0.82f, 0.88f, 1.f, 0.16f + 0.14f * R2), (1.f + R2) * Scale);
	}
}

// Message du muret sur plusieurs lignes (le point complet est long)
void AGPMHUD::DrawRadioMsg(const FString& Msg, float W, float Y)
{
	UFont* Mid = GEngine->GetMediumFont();
	const float MaxW = W * 0.56f;
	TArray<FString> Words;
	Msg.ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Cur;
	for (const FString& Wd : Words)
	{
		const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
		float TW = 0.f, TH = 0.f;
		GetTextSize(Try, TW, TH, Mid, Scale);
		if (TW > MaxW && !Cur.IsEmpty()) { Lines.Add(Cur); Cur = Wd; }
		else Cur = Try;
	}
	if (!Cur.IsEmpty()) Lines.Add(Cur);
	if (Lines.Num() == 0) return;
	float BW = 0.f, LH = 0.f;
	for (const FString& Ln : Lines)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Ln, TW, TH, Mid, Scale);
		BW = FMath::Max(BW, TW); LH = FMath::Max(LH, TH);
	}
	const float BH = Lines.Num() * LH + 6.f * Scale, RX = (W - BW) / 2 - 10.f * Scale;
	DrawRect(FLinearColor(0.02f, 0.1f, 0.18f, 0.8f), RX, Y, BW + 20.f * Scale, BH);
	DrawRect(RaceRadio, RX, Y, 3.f * Scale, BH);
	for (int32 i = 0; i < Lines.Num(); ++i) DrawText(Lines[i], FLinearColor(0.72f, 0.93f, 1.f), RX + 10.f * Scale, Y + 3.f * Scale + i * LH, Mid, Scale);
}

// Radio du muret : point complet, rythme (mode affiché), pneus du prochain arrêt ; touches 1 2 3 sur ordinateur
void AGPMHUD::DrawRadioPad(AGPMGameMode* GM, AGPMCar* Car)
{
	UFont* Mid = GEngine->GetMediumFont();
	const TCHAR NextT = Car->bBox ? Car->NextTyre : GM->NextStopTyre(Car);
	const FLinearColor ModeCol = Car->Mode == 0 ? FLinearColor(1.f, 0.45f, 0.2f) : Car->Mode == 2 ? FLinearColor(0.35f, 1.f, 0.5f) : FLinearColor(0.8f, 0.8f, 0.85f);
	const FString Keys[3] = { TEXT("1"), TEXT("2"), TEXT("3") };
	const FString Labels[3] = { TEXT("POINT RADIO"), Car->Mode == 0 ? FString(TEXT("ATTAQUE")) : Car->Mode == 2 ? FString(TEXT("ÉCONOMIE")) : FString(TEXT("RYTHME NORMAL")), TEXT("PNEUS SUIVANTS") };
	const FLinearColor Marks[3] = { RaceRadio, ModeCol, TyreColour(NextT) };
	for (int32 k = 0; k < 3; ++k)
	{
		const FButton& Bt = RadioBtn[k];
		DrawRect(bRadioDown[k] ? FLinearColor(1.f, 1.f, 1.f, 0.3f) : RacePanel, Bt.Pos.X, Bt.Pos.Y, Bt.Size.X, Bt.Size.Y);
		DrawRect(Marks[k], Bt.Pos.X, Bt.Pos.Y, 3.f * Scale, Bt.Size.Y);
		float TX = Bt.Pos.X + 8.f * Scale;
		if (!bShowTouch)
		{
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.14f), TX, Bt.Pos.Y + 6.f * Scale, 14.f * Scale, 16.f * Scale);
			DrawText(Keys[k], FLinearColor::White, TX + 3.5f * Scale, Bt.Pos.Y + 6.5f * Scale, Mid, Scale * 0.75f);
			TX += 19.f * Scale;
		}
		DrawText(Labels[k], k == 1 ? ModeCol : FLinearColor::White, TX, Bt.Pos.Y + 7.f * Scale, Mid, Scale * 0.72f);
		if (k == 2)
		{
			const float D = 12.f * Scale, DX = Bt.Pos.X + Bt.Size.X - D - 6.f * Scale, DY = Bt.Pos.Y + (Bt.Size.Y - D) / 2;
			DrawRect(TyreColour(NextT), DX, DY, D, D);
			DrawRect(FLinearColor(0.05f, 0.05f, 0.05f), DX + 3.f * Scale, DY + 3.f * Scale, D - 6.f * Scale, D - 6.f * Scale);
		}
	}
}

// Qualifications : pole à battre, écart en direct, secteurs (violet = meilleur, vert = record perso, jaune = plus lent)
void AGPMHUD::DrawQualiPanel(AGPMGameMode* GM, AGPMCar* Car, float X, float Y)
{
	UFont* Mid = GEngine->GetMediumFont();
	const float PW = 196.f * Scale, PH = 94.f * Scale;
	DrawRect(RacePanel, X, Y, PW, PH);
	const float Pole = GM->QPole();
	DrawText(TEXT("Pole à battre"), RaceDim, X + 8.f * Scale, Y + 3.f * Scale, Mid, Scale * 0.8f);
	DrawText(Pole > 0.f ? Chrono(Pole) : FString(TEXT("--:--.---")), RacePurple, X + 8.f * Scale, Y + 18.f * Scale, Mid, Scale * 1.05f);
	float Delta = 0.f;
	if (GM->QDelta(Delta))
	{
		const FString DS = FString::Printf(TEXT("%+.2f"), Delta);
		float TW = 0.f, TH = 0.f;
		GetTextSize(DS, TW, TH, Mid, Scale * 1.3f);
		DrawText(DS, Delta < 0.f ? FLinearColor(0.3f, 1.f, 0.45f) : FLinearColor(1.f, 0.38f, 0.3f), X + PW - TW - 8.f * Scale, Y + 8.f * Scale, Mid, Scale * 1.3f);
	}
	else if (Car->Lap < 1) DrawText(TEXT("tour de lancement"), RaceDim, X + PW - 104.f * Scale, Y + 6.f * Scale, Mid, Scale * 0.7f);
	const float CW = (PW - 24.f * Scale) / 3.f;
	for (int32 k = 0; k < 3; ++k)
	{
		const float CX = X + 8.f * Scale + k * (CW + 4.f * Scale), CY = Y + 46.f * Scale;
		const int32 Tint = GM->QCol[k];
		const FLinearColor Fill = Tint == 1 ? FLinearColor(0.55f, 0.25f, 0.9f, 0.92f) : Tint == 2 ? FLinearColor(0.12f, 0.62f, 0.28f, 0.92f) : Tint == 3 ? FLinearColor(0.8f, 0.66f, 0.08f, 0.92f) : FLinearColor(1.f, 1.f, 1.f, 0.08f);
		DrawRect(Fill, CX, CY, CW, 40.f * Scale);
		DrawText(FString::Printf(TEXT("S%d"), k + 1), FLinearColor(1.f, 1.f, 1.f, 0.75f), CX + 5.f * Scale, CY + 2.f * Scale, Mid, Scale * 0.7f);
		const bool bNow = GM->QSec[k] > 0.f;
		const float Sec = bNow ? GM->QSec[k] : GM->QPB[k];
		DrawText(Sec > 0.f ? FString::Printf(TEXT("%.3f"), Sec) : FString(TEXT("--.---")), bNow ? FLinearColor::White : RaceDim, CX + 5.f * Scale, CY + 18.f * Scale, Mid, Scale * 0.85f);
	}
}

// Météo (à gauche de la mini-carte) : pluie, humidité de la piste, changement annoncé
void AGPMHUD::DrawWeatherChip(AGPMGameMode* GM, float Right, float Y)
{
	const bool bChange = GM->WxChangeAt >= 0.f && !GM->bWxDone;
	if (!GM->bRaining && GM->TrackWet < 0.03f && !bChange) return;
	UFont* Mid = GEngine->GetMediumFont();
	const float CW = 108.f * Scale, CH = (bChange ? 44.f : 32.f) * Scale, X0 = Right - CW;
	DrawRect(RacePanel, X0, Y, CW, CH);
	const FString Top = GM->bRaining ? FString(TEXT("PLUIE")) : GM->TrackWet > 0.25f ? FString(TEXT("PISTE HUMIDE")) : FString(TEXT("TEMPS SEC"));
	DrawText(Top, GM->bRaining ? RaceRain : FLinearColor::White, X0 + 7.f * Scale, Y + 3.f * Scale, Mid, Scale * 0.8f);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), X0 + 7.f * Scale, Y + 21.f * Scale, CW - 14.f * Scale, 4.f * Scale);
	DrawRect(RaceRain, X0 + 7.f * Scale, Y + 21.f * Scale, (CW - 14.f * Scale) * FMath::Clamp(GM->TrackWet, 0.f, 1.f), 4.f * Scale);
	if (bChange) DrawText(GM->bRainStart ? TEXT("éclaircie attendue") : TEXT("averse attendue"), RaceYellow, X0 + 7.f * Scale, Y + 28.f * Scale, Mid, Scale * 0.65f);
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
	DrawRain(GM, Car, W, H);

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
		DrawQualiPanel(GM, Car, M, M);
	}
	else if (GM && GM->NumCars() > 0)
	{
		DrawTower(GM, Car, M, M);
		if (Car->Penalty > 0.f) DrawCentered(FString::Printf(TEXT("Pénalité +%.0f s"), Car->Penalty), FLinearColor(1.f, 0.3f, 0.2f), W, H - (70.f + (bShowTouch ? 66.f : 12.f) + 22.f) * Scale, Mid, Scale);
		if (Car->PitPhase) DrawCentered(Car->PitPhase == 2 ? TEXT("ARRÊT AU STAND") : TEXT("LIMITEUR 80 km/h"), RaceYellow, W, H * 0.24f, Mid, Scale * 1.4f);
		if (GM->SafetyOut())
		{
			DrawRect(FLinearColor(1.f, 0.75f, 0.05f, 0.85f), (W - 260 * Scale) / 2, H * 0.17f, 260 * Scale, 26 * Scale);
			DrawCentered(TEXT("VOITURE DE SÉCURITÉ"), FLinearColor(0.05f, 0.05f, 0.05f), W, H * 0.17f + 3 * Scale, Mid, Scale * 1.1f);
		}
		if (GM->RadioT > 0.f) DrawRadioMsg(GM->Radio, W, M + 52 * Scale);
		if (GM->Phase == AGPMGameMode::EPhase::Race && !Car->bFinished) DrawRadioPad(GM, Car);
		if (GM->Phase == AGPMGameMode::EPhase::Lights) DrawLights(GM->LightsOn(), W);
		else if (GM->Clock < 1.5f) DrawCentered(TEXT("GO !"), FLinearColor(0.2f, 1.f, 0.3f), W, H * 0.3f, Big, Scale * 3.f);
		if (Car->bFinished) DrawCentered(FString::Printf(TEXT("Arrivée : P%d"), GM->PositionOf(Car)), FLinearColor::White, W, H * 0.3f, Big, Scale * 2.f);
	}

	DrawCluster(Car, W, H);

	// Mini-carte (en haut à droite) et bouton pause dessous
	const float MS = 112.f * Scale;
	DrawMiniMap(Car, FVector2D(W - M - MS, M + 6 * Scale), MS);
	if (GM) DrawWeatherChip(GM, W - M - 48 * Scale, M + MS + 18 * Scale);   // sous la mini-carte, à gauche du bouton pause
	if (bLive && !bPaused && Btn(TEXT("II"), W - M - 40 * Scale, M + MS + 18 * Scale, 40 * Scale, 28 * Scale, FLinearColor(0.2f, 0.22f, 0.28f))) { SetPaused(true); return; }
	if (bShowFps && GM)
		DrawText(FString::Printf(TEXT("%.0f i/s  ·  %d %%"), GM->Fps, GM->ScreenPct), GM->Fps < 50.f ? FLinearColor(1.f, 0.4f, 0.3f) : FLinearColor(0.4f, 1.f, 0.5f), W - M - 110 * Scale, M + MS + 68 * Scale, Mid, Scale * 0.8f);

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
		DrawText(TEXT("Flèches/ZQSD : conduire   C : caméra   B : stand   1 2 3 : radio   R : replacer   P : pause   T : tactile"), FLinearColor(1, 1, 1, 0.55f), M, H - 22 * Scale, Mid, Scale * 0.8f);

	if (bPaused) DrawPause(GM, Car, W, H);
}
