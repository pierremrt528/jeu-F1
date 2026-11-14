// HUD de course : tour des positions façon télé, compte-tours au centre en bas, tour et chronos, messages,
// mini-carte, commandes tactiles et menu pause (Échap / P, ou le bouton II sous la mini-carte).
#include "GPMHUD.h"
#include "GrandPrixManager.h"
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
#include "GPMUI.h"

namespace
{
	const FLinearColor RacePanel(0.02f, 0.02f, 0.04f, 0.62f);
	const FLinearColor RaceDim(1.f, 1.f, 1.f, 0.65f);
	const FLinearColor RacePurple(0.75f, 0.4f, 1.f);
	const FLinearColor RaceYellow(1.f, 0.85f, 0.2f);
	const FLinearColor RaceRain(0.35f, 0.65f, 1.f);
	const FLinearColor RaceRadio(0.3f, 0.75f, 1.f);
	FLinearColor InkOnColour(const FLinearColor& Bg) { return Bg.GetLuminance() > 0.55f ? FLinearColor(0.04f, 0.05f, 0.08f) : FLinearColor::White; }
	FLinearColor TyreColour(TCHAR T)
	{
		return T == 'S' ? FLinearColor(1.f, 0.2f, 0.2f) : T == 'H' ? FLinearColor(0.92f, 0.92f, 0.92f) : T == 'W' ? FLinearColor(0.2f, 0.55f, 1.f)
			: T == 'I' ? FLinearColor(0.25f, 0.85f, 0.35f) : FLinearColor(1.f, 0.85f, 0.15f);
	}
}

FString AGPMHUD::Code3(const FString& Driver) const
{
	FString Full = Nm(Driver).TrimStartAndEnd();
	int32 Sp = INDEX_NONE;
	if (Full.FindLastChar(TEXT(' '), Sp)) Full = Full.Mid(Sp + 1);
	return GPMUp(Full.Left(3));
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
	const float RowH = 18.f * Scale, TW = 150.f * Scale;
	// en-tête : tour en cours sur un bandeau penché aux couleurs de l'écurie
	Slab(X, Y, 96.f * Scale, 20.f * Scale, HudAcc);
	Ink(FString::Printf(TEXT("TOUR %d/%d"), FMath::Clamp(Ord[0]->Lap, 1, GM->TotalLaps), GM->TotalLaps), X + 9.f * Scale, Y + 2.5f * Scale, 0, 10.f, InkOnColour(HudAcc));
	float Yr = Y + 24.f * Scale;
	for (int32 R : Rows)
	{
		if (R < 0)
		{
			for (int32 d = 0; d < 3; ++d) Paint(4, X + 10.f * Scale + d * 6.f * Scale, Yr + 1.f * Scale, 3.f * Scale, 3.f * Scale, RaceDim);
			Yr += 7.f * Scale;
			continue;
		}
		const AGPMCar* C = Ord[R];
		const bool bMe = C == Car;
		DrawRect(RacePanel, X, Yr, TW, RowH - 1.f);
		if (bMe) Paint(2, X, Yr, TW, RowH - 1.f, FLinearColor(HudAcc.R, HudAcc.G, HudAcc.B, 0.9f));
		// place (or, argent, bronze pour le podium), trait de la couleur de l'écurie, code du pilote, écart
		const FLinearColor PosCol = bMe ? FLinearColor::White : R == 0 ? FLinearColor(1.f, 0.8f, 0.22f) : R == 1 ? FLinearColor(0.82f, 0.85f, 0.92f)
			: R == 2 ? FLinearColor(0.9f, 0.58f, 0.34f) : FLinearColor::White;
		Ink(FString::FromInt(R + 1), X + 18.f * Scale, Yr + 1.f * Scale, 0, 10.f, PosCol, 1.f);
		Paint(1, X + 23.f * Scale, Yr + 2.f * Scale, 6.f * Scale, RowH - 5.f * Scale, C->TeamColor);
		Ink(Code3(C->DriverName), X + 33.f * Scale, Yr + 1.f * Scale, 1, 10.f, FLinearColor::White);
		FString G;
		if (R == 0) G = TEXT("LEADER");
		else if (C->bFinished) G = TEXT("ARRIVÉ");
		else
		{
			const float D = (LeadP - C->Progress()) / PerLap;
			G = D >= 1.f ? FString::Printf(TEXT("+%d T"), FMath::FloorToInt(D)) : FString::Printf(TEXT("+%.1f"), D * GM->LapEstimate);
		}
		if (C->PitPhase) G = TEXT("STAND");
		Ink(G, X + TW - 7.f * Scale, Yr + 2.f * Scale, R == 0 || C->PitPhase ? 2 : 3, R == 0 || C->PitPhase ? 7.5f : 9.5f,
			bMe ? FLinearColor::White : (C->PitPhase ? RaceYellow : RaceDim), 1.f);
		Yr += RowH;
	}
}

// Compte-tours : diodes de régime, vitesse, rapport, pneus et carburant
void AGPMHUD::DrawCluster(AGPMCar* Car, float W, float H)
{
	// au clavier : en bas à droite (la voiture reste dégagée au centre) ; en tactile : au centre, au-dessus du bouton STAND
	const float CW = 256.f * Scale, CH = 78.f * Scale;
	const float X = bShowTouch ? (W - CW) / 2 : W - CW - 16.f * Scale, Y = H - CH - (bShowTouch ? 66.f : 14.f) * Scale;
	Slab(X, Y, CW, CH, FLinearColor(0.015f, 0.02f, 0.04f, 0.8f));
	if (Car->Penalty > 0.f)
	{
		const FString Pen = FString::Printf(TEXT("PÉNALITÉ +%.0f S"), Car->Penalty);
		const FVector2D PS = InkSize(Pen, 0, 10.f);
		Slab(X + CW / 2 - PS.X / 2 - 14.f * Scale, Y - PS.Y - 8.f * Scale, PS.X + 28.f * Scale, PS.Y + 4.f * Scale, FLinearColor(0.6f, 0.05f, 0.04f, 0.92f));
		Ink(Pen, X + CW / 2, Y - PS.Y - 6.f * Scale, 0, 10.f, FLinearColor::White, 0.5f);
	}

	// diodes : vert, rouge, bleu ; tout clignote au rupteur
	const float Frac = FMath::Clamp((Car->Rpm() - 6500.f) / 5500.f, 0.f, 1.f);
	const float Kmh = FMath::Abs(Car->SpeedKmh());
	const int32 Lit = Kmh < 2.f ? 0 : FMath::CeilToInt(Frac * 12.f);
	const bool bBlink = Frac > 0.96f && FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 0.16f) < 0.08f;
	const float DW = (CW - 44.f * Scale) / 12.f;
	for (int32 k = 0; k < 12; ++k)
	{
		FLinearColor Col = k < 5 ? FLinearColor(0.15f, 1.f, 0.3f) : k < 9 ? FLinearColor(1.f, 0.15f, 0.15f) : FLinearColor(0.25f, 0.45f, 1.f);
		const bool bOn = k < Lit && !bBlink;
		if (!bOn) Col = FLinearColor(Col.R * 0.2f, Col.G * 0.2f, Col.B * 0.2f, 0.85f);
		else Paint(3, X + 22.f * Scale + k * DW - 4.f * Scale, Y + 1.f * Scale, DW + 8.f * Scale, 14.f * Scale, FLinearColor(Col.R, Col.G, Col.B, 0.45f));
		Paint(1, X + 22.f * Scale + k * DW, Y + 5.f * Scale, DW - 2.f * Scale, 7.f * Scale, Col);
	}

	// rapport à droite, dans un bloc penché de la couleur de l'écurie (bleu au rupteur)
	const int32 Gr = Car->Gear();
	const float BX = X + CW - 60.f * Scale, BY = Y + 17.f * Scale;
	const FLinearColor GearBg = Frac > 0.9f ? FLinearColor(0.2f, 0.4f, 1.f) : HudAcc;
	Slab(BX, BY, 48.f * Scale, 52.f * Scale, GearBg);
	Ink(Gr == 0 ? FString(TEXT("N")) : FString::FromInt(Gr), BX + 24.f * Scale, BY + 5.f * Scale, 0, 25.f, InkOnColour(GearBg), 0.5f);

	// vitesse, très grande, alignée sur le bloc du rapport
	Ink(FString::Printf(TEXT("%d"), FMath::RoundToInt(Kmh)), BX - 8.f * Scale, Y + 12.f * Scale, 0, 30.f, FLinearColor::White, 1.f);
	Ink(TEXT("KM/H"), BX - 10.f * Scale, Y + 56.f * Scale, 2, 7.f, RaceDim, 1.f);

	// pneus (gomme, usure) et carburant à gauche
	const float Life = FMath::Max(0.f, 1.f - Car->Wear);
	const FLinearColor LifeCol = Life > 0.6f ? FLinearColor(0.3f, 1.f, 0.4f) : Life > 0.4f ? RaceYellow : FLinearColor(1.f, 0.25f, 0.2f);
	const float LX = X + 20.f * Scale, LY = Y + 20.f * Scale;
	TyreIcon(Car->Tyre, LX + 11.f * Scale, LY + 11.f * Scale, 11.f * Scale);
	Ink(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Life * 100)), LX + 27.f * Scale, LY + 1.f * Scale, 1, 12.f, LifeCol);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), LX, LY + 27.f * Scale, 70.f * Scale, 3.f * Scale);
	DrawRect(LifeCol, LX, LY + 27.f * Scale, 70.f * Scale * Life, 3.f * Scale);
	// carburant : marge en tours jusqu'au prochain ravitaillement (arrêt prévu) ou jusqu'à l'arrivée
	if (const FGPMTrackData* T = Car->GetTrack())
	{
		AGPMGameMode* GM = GetWorld()->GetAuthGameMode<AGPMGameMode>();
		const float PerLap = T->WX.Num() * 2.f / 1000.f * 1.7f * Car->UpFuel;
		if (GM && PerLap > 0.f && GM->Phase == AGPMGameMode::EPhase::Race)
		{
			const float FuelLaps = Car->Fuel / PerLap, Need = GM->LapsToRefuel(Car);
			const float Margin = FuelLaps - Need;
			Ink(FString::Printf(TEXT("ESSENCE %+.1f T"), Margin), LX, LY + 33.f * Scale, 2, 7.f, Margin < 0.f ? FLinearColor(1.f, 0.3f, 0.2f) : RaceDim);
		}
	}
	// aides et demande d'arrêt : petites étiquettes sous la vitesse
	float TagX = X + 104.f * Scale;
	auto Tag = [this, &TagX, Y](const TCHAR* Label, const FLinearColor& Col)
	{
		Ink(Label, TagX, Y + 66.f * Scale, 2, 6.5f, Col);
		TagX += InkSize(Label, 2, 6.5f).X + 8.f * Scale;
	};
	if (Car->bAbsOn) Tag(TEXT("ABS"), RaceYellow);
	if (Car->bTcOn) Tag(TEXT("TC"), RaceYellow);
	if (Car->bBox) Tag(TEXT("BOX"), FLinearColor(0.3f, 0.8f, 1.f));
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

// Message du muret sur plusieurs lignes (le point complet est long), sous le nom du canal
void AGPMHUD::DrawRadioMsg(const FString& Msg, float W, float Y)
{
	const float MaxW = W * 0.5f, Sz = 10.5f;
	TArray<FString> Words;
	Msg.ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Cur;
	for (const FString& Wd : Words)
	{
		const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
		if (InkSize(Try, 1, Sz).X > MaxW && !Cur.IsEmpty()) { Lines.Add(Cur); Cur = Wd; }
		else Cur = Try;
	}
	if (!Cur.IsEmpty()) Lines.Add(Cur);
	if (Lines.Num() == 0) return;
	float TextW = InkSize(TEXT("RADIO DU MURET"), 2, 7.f).X + 12.f * Scale;
	for (const FString& Ln : Lines) TextW = FMath::Max(TextW, InkSize(Ln, 1, Sz).X);
	const float LH = InkSize(TEXT("Ég"), 1, Sz).Y, Pad = 14.f * Scale, Head = 18.f * Scale;
	const float BoxW = TextW + Pad * 2.f, BoxH = Head + Lines.Num() * LH + 6.f * Scale, RX = (W - BoxW) / 2;
	DrawRect(FLinearColor(0.015f, 0.04f, 0.08f, 0.88f), RX, Y, BoxW, BoxH);
	Paint(2, RX, Y, BoxW * 0.6f, BoxH, FLinearColor(RaceRadio.R, RaceRadio.G, RaceRadio.B, 0.14f));
	DrawRect(RaceRadio, RX, Y, 3.f * Scale, BoxH);
	// voyant qui pulse pendant le message
	const float Pulse = 0.55f + 0.45f * FMath::Abs(FMath::Sin(float(GetWorld()->GetRealTimeSeconds()) * 5.f));
	Paint(3, RX + Pad - 5.f * Scale, Y + 3.f * Scale, 15.f * Scale, 15.f * Scale, FLinearColor(RaceRadio.R, RaceRadio.G, RaceRadio.B, 0.5f * Pulse));
	Paint(4, RX + Pad, Y + 8.f * Scale, 5.f * Scale, 5.f * Scale, RaceRadio);
	Ink(TEXT("RADIO DU MURET"), RX + Pad + 10.f * Scale, Y + 4.f * Scale, 2, 7.f, RaceRadio);
	for (int32 i = 0; i < Lines.Num(); ++i) Ink(Lines[i], RX + Pad, Y + Head + i * LH, 1, Sz, FLinearColor(0.86f, 0.95f, 1.f));
}

// Radio du muret : point complet, rythme (mode affiché), pneus du prochain arrêt ; touches 1 2 3 sur ordinateur
void AGPMHUD::DrawRadioPad(AGPMGameMode* GM, AGPMCar* Car)
{
	const TCHAR NextT = Car->bBox ? Car->NextTyre : GM->NextStopTyre(Car);
	const FLinearColor ModeCol = Car->Mode == 0 ? FLinearColor(1.f, 0.45f, 0.2f) : Car->Mode == 2 ? FLinearColor(0.35f, 1.f, 0.5f) : FLinearColor(0.8f, 0.8f, 0.85f);
	const FString Keys[3] = { TEXT("1"), TEXT("2"), TEXT("3") };
	const FString Labels[3] = { TEXT("POINT RADIO"), Car->Mode == 0 ? FString(TEXT("ATTAQUE")) : Car->Mode == 2 ? FString(TEXT("ÉCONOMIE")) : FString(TEXT("RYTHME NORMAL")), TEXT("PNEUS SUIVANTS") };
	const FLinearColor Marks[3] = { RaceRadio, ModeCol, TyreColour(NextT) };
	for (int32 k = 0; k < 3; ++k)
	{
		const FButton& Bt = RadioBtn[k];
		const float BX = Bt.Pos.X, BY = Bt.Pos.Y, BW = Bt.Size.X, BH = Bt.Size.Y;
		Slab(BX, BY, BW, BH, bRadioDown[k] ? FLinearColor(1.f, 1.f, 1.f, 0.28f) : RacePanel);
		Paint(1, BX + 7.f * Scale, BY + 4.f * Scale, 5.f * Scale, BH - 8.f * Scale, Marks[k]);
		float TX = BX + 17.f * Scale;
		if (!bShowTouch)
		{
			// touche du clavier
			const float KS = 15.f * Scale, KY = BY + (BH - KS) / 2;
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.13f), TX, KY, KS, KS);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), TX, KY + KS - 1.5f * Scale, KS, 1.5f * Scale);
			Ink(Keys[k], TX + KS / 2, KY + (KS - InkSize(Keys[k], 1, 8.f).Y) / 2, 1, 8.f, FLinearColor::White, 0.5f);
			TX += KS + 6.f * Scale;
		}
		Ink(Labels[k], TX, BY + (BH - InkSize(Labels[k], 2, 7.f).Y) / 2, 2, 7.f, k == 1 ? ModeCol : FLinearColor::White);
		if (k == 2) TyreIcon(NextT, BX + BW - 17.f * Scale, BY + BH / 2, 8.5f * Scale);
	}
}

// Qualifications : pole à battre, écart en direct, secteurs (violet = meilleur, vert = record perso, jaune = plus lent)
void AGPMHUD::DrawQualiPanel(AGPMGameMode* GM, AGPMCar* Car, float X, float Y)
{
	const float PW = 200.f * Scale, PH = 98.f * Scale;
	Slab(X, Y, 124.f * Scale, 20.f * Scale, RaceYellow);
	Ink(TEXT("QUALIFICATIONS"), X + 9.f * Scale, Y + 3.f * Scale, 0, 9.5f, InkOnColour(RaceYellow));
	const float PY = Y + 24.f * Scale;
	DrawRect(RacePanel, X, PY, PW, PH);
	DrawRect(RacePurple, X, PY, 3.f * Scale, 40.f * Scale);
	const float Pole = GM->QPole();
	Ink(TEXT("POLE À BATTRE"), X + 11.f * Scale, PY + 5.f * Scale, 2, 7.f, RaceDim);
	Ink(Pole > 0.f ? Chrono(Pole) : FString(TEXT("--:--.---")), X + 11.f * Scale, PY + 16.f * Scale, 0, 14.f, RacePurple);
	float Delta = 0.f;
	if (GM->QDelta(Delta))
		Ink(FString::Printf(TEXT("%+.2f"), Delta), X + PW - 10.f * Scale, PY + 8.f * Scale, 0, 17.f, Delta < 0.f ? FLinearColor(0.3f, 1.f, 0.45f) : FLinearColor(1.f, 0.38f, 0.3f), 1.f);
	else if (Car->Lap < 1) Ink(TEXT("TOUR DE LANCEMENT"), X + PW - 10.f * Scale, PY + 7.f * Scale, 2, 6.5f, RaceDim, 1.f);
	const float CW = (PW - 24.f * Scale) / 3.f;
	for (int32 k = 0; k < 3; ++k)
	{
		const float CX = X + 8.f * Scale + k * (CW + 4.f * Scale), CY = PY + 48.f * Scale;
		const int32 Tint = GM->QCol[k];
		const FLinearColor Fill = Tint == 1 ? FLinearColor(0.55f, 0.25f, 0.9f, 0.92f) : Tint == 2 ? FLinearColor(0.12f, 0.62f, 0.28f, 0.92f) : Tint == 3 ? FLinearColor(0.8f, 0.66f, 0.08f, 0.92f) : FLinearColor(1.f, 1.f, 1.f, 0.07f);
		DrawRect(Fill, CX, CY, CW, 42.f * Scale);
		Paint(5, CX, CY, CW, 42.f * Scale, FLinearColor(0.f, 0.f, 0.f, 0.22f));
		Ink(FString::Printf(TEXT("S%d"), k + 1), CX + 6.f * Scale, CY + 4.f * Scale, 2, 7.f, FLinearColor(1.f, 1.f, 1.f, 0.8f));
		const bool bNow = GM->QSec[k] > 0.f;
		const float Sec = bNow ? GM->QSec[k] : GM->QPB[k];
		Ink(Sec > 0.f ? FString::Printf(TEXT("%.3f"), Sec) : FString(TEXT("--.---")), CX + 6.f * Scale, CY + 18.f * Scale, 1, 11.f, bNow ? FLinearColor::White : RaceDim);
	}
}

// Météo (à gauche de la mini-carte) : pluie, humidité de la piste, changement annoncé
void AGPMHUD::DrawWeatherChip(AGPMGameMode* GM, float RightX, float Y)
{
	const bool bChange = GM->WxChangeAt >= 0.f && !GM->bWxDone;
	if (!GM->bRaining && GM->TrackWet < 0.03f && !bChange) return;
	const float CW = 112.f * Scale, CH = (bChange ? 46.f : 34.f) * Scale, X0 = RightX - CW;
	DrawRect(RacePanel, X0, Y, CW, CH);
	DrawRect(GM->bRaining ? RaceRain : RaceDim, X0, Y, 3.f * Scale, CH);
	float TX = X0 + 10.f * Scale;
	if (GM->bRaining)
	{
		// trois gouttes penchées
		for (int32 d = 0; d < 3; ++d) Paint(1, TX + d * 5.f * Scale, Y + (5.f + (d % 2) * 2.f) * Scale, 3.f * Scale, 8.f * Scale, RaceRain);
		TX += 20.f * Scale;
	}
	const FString Top = GM->bRaining ? FString(TEXT("PLUIE")) : GM->TrackWet > 0.25f ? FString(TEXT("PISTE HUMIDE")) : FString(TEXT("TEMPS SEC"));
	Ink(Top, TX, Y + 4.f * Scale, 2, 7.5f, GM->bRaining ? RaceRain : FLinearColor::White);
	const float BarX = X0 + 10.f * Scale, BarW = CW - 18.f * Scale;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), BarX, Y + 22.f * Scale, BarW, 4.f * Scale);
	DrawRect(RaceRain, BarX, Y + 22.f * Scale, BarW * FMath::Clamp(GM->TrackWet, 0.f, 1.f), 4.f * Scale);
	if (bChange) Ink(GM->bRainStart ? TEXT("ÉCLAIRCIE ATTENDUE") : TEXT("AVERSE ATTENDUE"), BarX, Y + 31.f * Scale, 2, 6.5f, RaceYellow);
}

void AGPMHUD::DrawPause(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	DrawRect(FLinearColor(0.01f, 0.012f, 0.025f, 0.74f), 0, 0, W, H);
	Paint(2, 0, 0, W * 0.6f, H, FLinearColor(HudAcc.R, HudAcc.G, HudAcc.B, 0.2f));
	Ink(GM && GM->bCareer ? TEXT("GRAND PRIX") : TEXT("COURSE SOLO"), W / 2, H * 0.08f, 2, 8.f, RaceDim, 0.5f);
	Ink(TEXT("PAUSE"), W / 2, H * 0.08f + 13.f * Scale, 0, 32.f, FLinearColor::White, 0.5f);
	Paint(1, W / 2 - 28.f * Scale, H * 0.08f + 66.f * Scale, 56.f * Scale, 4.f * Scale, HudAcc);
	const float BW = 240.f * Scale, BH = 34.f * Scale, X = (W - BW) / 2;
	float Y = H * 0.33f;
	const FLinearColor Alt(0.13f, 0.15f, 0.21f, 0.96f);
	if (Btn(TEXT("REPRENDRE"), X, Y, BW, BH, HudAcc)) { SetPaused(false); return; }
	Y += BH + 8 * Scale;
	if (Btn(TEXT("RECOMMENCER LA COURSE"), X, Y, BW, BH, Alt)) { SetPaused(false); if (GM) GM->Restart(); return; }
	Y += BH + 8 * Scale;
	if (Btn(Car->IsCockpit() ? TEXT("CAMÉRA : COCKPIT") : TEXT("CAMÉRA : ARRIÈRE"), X, Y, BW, BH, Alt)) Car->ToggleCamera();
	Y += BH + 8 * Scale;
	if (Btn(bShowFps ? TEXT("COMPTEUR I/S : OUI") : TEXT("COMPTEUR I/S : NON"), X, Y, BW, BH, Alt)) bShowFps = !bShowFps;
	Y += BH + 8 * Scale;
	if (Btn(GM && GM->bCareer ? TEXT("QUITTER LE GRAND PRIX") : TEXT("MENU PRINCIPAL"), X, Y, BW, BH, Alt)) { SetPaused(false); if (GM) GM->GoHome(); return; }
	Ink(TEXT("ÉCHAP OU P : REPRENDRE"), W / 2, Y + BH + 12.f * Scale, 2, 7.f, RaceDim, 0.5f);
}

void AGPMHUD::DrawRace(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	const float M = 16.f * Scale;
	// bandeaux aux couleurs de l'écurie du joueur (éclaircie si elle est trop sombre pour se voir)
	HudAcc = FLinearColor(Car->TeamColor.R, Car->TeamColor.G, Car->TeamColor.B, 1.f);
	if (HudAcc.GetLuminance() < 0.06f) HudAcc = HudAcc * 0.4f + FLinearColor(0.55f, 0.58f, 0.66f, 1.f) * 0.6f;
	APlayerController* PC = GetOwningPlayerController();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	const bool bLive = GM && (GM->Phase == AGPMGameMode::EPhase::Race || GM->Phase == AGPMGameMode::EPhase::Lights || GM->Phase == AGPMGameMode::EPhase::Quali);
	if (PC && PC->WasInputKeyJustPressed(EKeys::F)) bShowFps = !bShowFps;
	if (PC && bLive && (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::P))) { SetPaused(!bPaused); return; }
	if (bPaused && !bLive) SetPaused(false);

	if (GM && (GM->Phase == AGPMGameMode::EPhase::QualiDone || GM->Phase == AGPMGameMode::EPhase::Done)) return;
	DrawRain(GM, Car, W, H);

	// bandeau centré, penché, à la taille du texte
	auto Banner = [this, W](const FString& Text, float Y, const FLinearColor& Bg, const FLinearColor& Fg, float Size)
	{
		const FVector2D Sz = InkSize(Text, 0, Size);
		const float BW = Sz.X + 40.f * Scale, BH = Sz.Y + 8.f * Scale;
		Slab((W - BW) / 2, Y, BW, BH, Bg);
		Ink(Text, W / 2, Y + 4.f * Scale, 0, Size, Fg, 0.5f);
	};

	// Tour et chronos (en haut au centre) : bloc du tour à la couleur de l'écurie, puis tour en cours, dernier et meilleur
	const bool bQuali = GM && GM->Phase == AGPMGameMode::EPhase::Quali;
	const float CW = 320.f * Scale, CH = 44.f * Scale, CX = (W - CW) / 2, LB = 88.f * Scale;
	Slab(CX, M, CW, CH, RacePanel);
	const FLinearColor LapCol = bQuali ? RaceYellow : HudAcc;
	Slab(CX, M, LB, CH, LapCol);
	Ink(bQuali ? TEXT("SÉANCE") : TEXT("TOUR"), CX + LB / 2, M + 5.f * Scale, 2, 6.5f, InkOnColour(LapCol), 0.5f);
	const FString LapTxt = bQuali ? FString(TEXT("QUALIF")) : FString::Printf(TEXT("%d/%d"), FMath::Clamp(Car->Lap, 1, GM ? GM->TotalLaps : 99), GM ? GM->TotalLaps : 0);
	Ink(LapTxt, CX + LB / 2, M + 15.f * Scale, 0, bQuali ? 11.f : 14.f, InkOnColour(LapCol), 0.5f);
	const float C1 = CX + LB + 12.f * Scale, C2 = CX + LB + 100.f * Scale, C3 = CX + CW - 12.f * Scale;
	Ink(TEXT("EN COURS"), C1, M + 5.f * Scale, 2, 6.5f, RaceDim);
	Ink(Chrono(Car->LapTime()), C1, M + 16.f * Scale, 1, 12.5f, FLinearColor::White);
	const bool bBest = Car->LastLap > 0.f && FMath::IsNearlyEqual(Car->LastLap, Car->BestLap);
	Ink(TEXT("DERNIER"), C2, M + 5.f * Scale, 2, 6.5f, RaceDim);
	Ink(Chrono(Car->LastLap), C2, M + 18.f * Scale, 3, 10.5f, bBest ? RacePurple : FLinearColor::White);
	Ink(TEXT("MEILLEUR"), C3, M + 5.f * Scale, 2, 6.5f, RaceDim, 1.f);
	Ink(Chrono(Car->BestLap), C3, M + 18.f * Scale, 3, 10.5f, RacePurple, 1.f);

	if (bQuali)
	{
		Ink(Car->Lap < 1 ? TEXT("TOUR DE LANCEMENT") : TEXT("TOUR LANCÉ"), W / 2, M + CH + 8.f * Scale, 2, 8.f, RaceYellow, 0.5f);
		DrawQualiPanel(GM, Car, M, M);
	}
	else if (GM && GM->NumCars() > 0)
	{
		DrawTower(GM, Car, M, M);
		if (Car->PitPhase) Banner(Car->PitPhase == 2 ? TEXT("ARRÊT AU STAND") : TEXT("LIMITEUR 80 KM/H"), H * 0.24f, FLinearColor(0.02f, 0.02f, 0.04f, 0.8f), RaceYellow, 14.f);
		if (GM->SafetyOut()) Banner(TEXT("VOITURE DE SÉCURITÉ"), H * 0.16f, FLinearColor(1.f, 0.75f, 0.05f, 0.92f), FLinearColor(0.05f, 0.05f, 0.05f), 12.f);
		if (GM->RadioT > 0.f) DrawRadioMsg(GM->Radio, W, M + CH + 8.f * Scale);
		if (GM->Phase == AGPMGameMode::EPhase::Race && !Car->bFinished) DrawRadioPad(GM, Car);
		if (GM->Phase == AGPMGameMode::EPhase::Lights) DrawLights(GM->LightsOn(), W);
		else if (GM->Clock < 1.5f)
		{
			// « GO ! » vert avec un halo, qui s'efface
			const float A = FMath::Clamp((1.5f - GM->Clock) * 2.f, 0.f, 1.f), GY = H * 0.24f;
			Paint(3, W / 2 - 150.f * Scale, GY - 50.f * Scale, 300.f * Scale, 170.f * Scale, FLinearColor(0.1f, 1.f, 0.3f, 0.35f * A));
			Ink(TEXT("GO !"), W / 2, GY, 0, 46.f, FLinearColor(0.25f, 1.f, 0.4f, A), 0.5f);
		}
		if (Car->bFinished)
		{
			const int32 Pos = GM->PositionOf(Car);
			const FLinearColor PosCol = Pos == 1 ? FLinearColor(1.f, 0.8f, 0.22f) : Pos <= 3 ? FLinearColor(0.85f, 0.88f, 0.95f) : FLinearColor::White;
			const float FY = H * 0.24f;
			Slab(W / 2 - 110.f * Scale, FY, 220.f * Scale, 78.f * Scale, FLinearColor(0.02f, 0.02f, 0.04f, 0.8f));
			Paint(1, W / 2 - 24.f * Scale, FY + 5.f * Scale, 48.f * Scale, 3.f * Scale, HudAcc);
			Ink(TEXT("ARRIVÉE"), W / 2, FY + 12.f * Scale, 2, 8.f, RaceDim, 0.5f);
			Ink(FString::Printf(TEXT("P%d"), Pos), W / 2, FY + 24.f * Scale, 0, 30.f, PosCol, 0.5f);
		}
	}

	DrawCluster(Car, W, H);

	// Mini-carte (en haut à droite) et bouton pause dessous
	const float MS = 112.f * Scale;
	DrawMiniMap(Car, FVector2D(W - M - MS, M + 6 * Scale), MS);
	if (GM) DrawWeatherChip(GM, W - M - 48 * Scale, M + MS + 18 * Scale);   // sous la mini-carte, à gauche du bouton pause
	if (bLive && !bPaused && Btn(TEXT("II"), W - M - 40 * Scale, M + MS + 18 * Scale, 40 * Scale, 28 * Scale, FLinearColor(0.13f, 0.15f, 0.21f, 0.92f))) { SetPaused(true); return; }
	if (bShowFps && GM)
		Ink(FString::Printf(TEXT("%.0f I/S  ·  %d %%"), GM->Fps, GM->ScreenPct), W - M, M + MS + 70 * Scale, 1, 9.f, GM->Fps < 50.f ? FLinearColor(1.f, 0.4f, 0.3f) : FLinearColor(0.4f, 1.f, 0.5f), 1.f);

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
	{
		// aide clavier les 20 premières secondes, en touches, sur deux lignes en bas à gauche (le compteur est au centre)
		Paint(2, 0.f, H - 58.f * Scale, 560.f * Scale, 58.f * Scale, FLinearColor(0.f, 0.f, 0.f, 0.6f));
		float KX = M, KY = H - 48.f * Scale;
		auto KeyCap = [this, &KX, &KY](const TCHAR* K, const TCHAR* What)
		{
			const FVector2D KS = InkSize(K, 1, 8.f);
			const float KW = KS.X + 10.f * Scale, KH = 16.f * Scale;
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), KX, KY, KW, KH);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.35f), KX, KY + KH - 1.5f * Scale, KW, 1.5f * Scale);
			Ink(K, KX + 5.f * Scale, KY + (KH - KS.Y) / 2, 1, 8.f, FLinearColor::White);
			KX += KW + 5.f * Scale;
			const FVector2D WS = InkSize(What, 2, 7.f);
			Ink(What, KX, KY + (KH - WS.Y) / 2, 2, 7.f, FLinearColor(1.f, 1.f, 1.f, 0.72f));
			KX += WS.X + 14.f * Scale;
		};
		KeyCap(TEXT("FLÈCHES / ZQSD"), TEXT("CONDUIRE"));
		KeyCap(TEXT("C"), TEXT("CAMÉRA"));
		KeyCap(TEXT("B"), TEXT("STAND"));
		KX = M; KY += 21.f * Scale;
		KeyCap(TEXT("1 2 3"), TEXT("RADIO"));
		KeyCap(TEXT("R"), TEXT("REPLACER"));
		KeyCap(TEXT("P"), TEXT("PAUSE"));
		KeyCap(TEXT("T"), TEXT("TACTILE"));
	}

	if (bPaused) DrawPause(GM, Car, W, H);
}
