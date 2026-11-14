// HUD de course « diffusion télé » (maquette Figma) : classement des 20 avec intervalles et pneus, chrono et secteurs,
// écarts devant / derrière, pneus et carburant avec l'état de la voiture, compte-tours, radio du muret, mini-carte,
// commandes tactiles et menu pause (Échap / P, ou le bouton II).
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
	// charte de la maquette (valeurs sRGB converties en couleurs linéaires)
	const FLinearColor RacePanel(0.0030f, 0.0040f, 0.0070f, 0.88f);   // #0A0D14
	const FLinearColor RaceRow(0.0060f, 0.0086f, 0.0159f, 0.84f);     // #121722
	const FLinearColor RaceBox(0.0109f, 0.0159f, 0.0296f, 0.96f);     // #1B2230
	const FLinearColor RaceLine(0.0232f, 0.0331f, 0.0582f, 1.f);      // #2A3344
	const FLinearColor RaceText(0.888f, 0.905f, 0.939f, 1.f);         // #F2F4F8
	const FLinearColor RaceDim(0.254f, 0.296f, 0.392f, 1.f);          // #8A94A8
	const FLinearColor RacePurple(0.361f, 0.0704f, 1.f, 1.f);         // #A24BFF : meilleur de la séance
	const FLinearColor RaceGreen(0.0242f, 0.672f, 0.147f, 1.f);       // #2BD66B : record perso, gain
	const FLinearColor RaceYellow(1.f, 0.578f, 0.0103f, 1.f);         // #FFC81A : plus lent, stands
	const FLinearColor RaceRed(0.783f, 0.0382f, 0.0284f, 1.f);        // #E5372F : perte, dégâts
	const FLinearColor RaceBlue(0.0331f, 0.246f, 1.f, 1.f);           // #3388FF : pluie, aspiration
	const FLinearColor RaceInk(0.0030f, 0.0040f, 0.0070f, 1.f);       // texte sombre sur fond clair
	FLinearColor RaceA(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, C.A * A); }
	FLinearColor InkOnColour(const FLinearColor& Bg) { return Bg.GetLuminance() > 0.4f ? RaceInk : FLinearColor::White; }
	FLinearColor TyreColour(TCHAR T)
	{
		return T == 'S' ? FLinearColor(1.f, 0.0331f, 0.0331f) : T == 'H' ? FLinearColor(0.846f, 0.846f, 0.846f) : T == 'W' ? RaceBlue
			: T == 'I' ? FLinearColor(0.0369f, 0.644f, 0.102f) : FLinearColor(1.f, 0.658f, 0.0194f);
	}
	const TCHAR* TyreName(TCHAR T)
	{
		return T == 'S' ? TEXT("TENDRES") : T == 'H' ? TEXT("DURS") : T == 'W' ? TEXT("PLUIE") : T == 'I' ? TEXT("INTERMÉDIAIRES") : TEXT("MOYENS");
	}
	FLinearColor SecColour(int32 Col) { return Col == 1 ? RacePurple : Col == 2 ? RaceGreen : Col == 3 ? RaceYellow : RaceLine; }
	FString Comma(const FString& S) { return S.Replace(TEXT("."), TEXT(",")); }
	/** Ligne du classement : voiture, écart affiché (police 4 chiffres ou 2 étiquette), pneus, meilleur tour, arrivée. */
	struct FRaceRow
	{
		const AGPMCar* C = nullptr;
		FString Gap;
		FLinearColor GapCol = FLinearColor(0.888f, 0.905f, 0.939f, 1.f);
		int32 GapKind = 4;
		TCHAR Tyre = 0;
		bool bFast = false, bFlag = false;
	};
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

// ---------- chronométrage suivi par le HUD ----------

// Points de passage (40 par tour) pour les écarts en temps comme à la télé, secteurs de chaque voiture (violet / vert / jaune),
// meilleur tour en course, profil du tour du joueur (delta en direct avec son meilleur tour), relais en cours (pneus, carburant)
void AGPMHUD::UpdateTiming(AGPMGameMode* GM, AGPMCar* Car, float Dt)
{
	AlertT = FMath::Max(0.f, AlertT - Dt);
	const FGPMTrackData* T = Car->GetTrack();
	if (!T || T->WX.Num() < 40) return;
	const int32 N = T->WX.Num();
	if (Car->RaceTime < LastRaceTime - 0.5f)
	{
		// nouvelle course ou course recommencée : on repart de zéro
		Timing.Reset();
		SesBest[0] = SesBest[1] = SesBest[2] = 0.f;
		FastestLap = 0.f; FastestCar = nullptr; FastestCode.Reset();
		CurProf.Reset(); BestProf.Reset(); ProfLap = -1;
		StintStops = -1; AlertT = 0.f;
	}
	LastRaceTime = Car->RaceTime;
	const TArray<AGPMCar*>& Ord = GM->Standings();
	const int32 LeadLap = Ord.Num() && Ord[0] ? Ord[0]->Lap : 0;
	const float MarkLen = N / 40.f;
	for (const AGPMCar* C : Ord)
	{
		if (!C) continue;
		FCarTiming& Tm = Timing.FindOrAdd(C);
		const int32 G = FMath::FloorToInt(C->Progress() / MarkLen);
		if (G > Tm.LastMark)
		{
			for (int32 g = FMath::Max(Tm.LastMark + 1, G - 4); g <= G; ++g) { Tm.Mark[g & 127] = C->RaceTime; Tm.MarkId[g & 127] = g; }
			Tm.LastMark = G;
		}
		if (C->Lap < 1) continue;
		const int32 Third = FMath::Clamp(C->Idx * 3 / N, 0, 2);
		auto Done = [&](int32 K, float Sec)
		{
			if (Sec <= 0.f) return;
			Tm.Cur[K] = Sec;
			const bool bSes = SesBest[K] <= 0.f || Sec < SesBest[K] - 1e-4f;
			const bool bPB = Tm.PB[K] <= 0.f || Sec < Tm.PB[K] - 1e-4f;
			Tm.CurCol[K] = bSes ? 1 : bPB ? 2 : 3;
			if (bSes)
			{
				if (C == Car && SesBest[K] > 0.f && LeadLap >= 3) { AlertTxt = FString::Printf(TEXT("SECTEUR %d · RECORD DE LA SÉANCE"), K + 1); AlertT = 3.f; }
				SesBest[K] = Sec;
			}
			if (bPB) Tm.PB[K] = Sec;
		};
		if (C->Lap != Tm.SecLap)
		{
			// ligne franchie : 3e secteur du tour précédent, meilleur tour en course, les secteurs passent en « dernier tour »
			if (Tm.Sec == 2 && Tm.SecLap == C->Lap - 1 && C->LastLap > Tm.SecAt[1]) Done(2, C->LastLap - Tm.SecAt[1]);
			if (Tm.SecLap >= 1 && C->LastLap > 0.f && (FastestLap <= 0.f || C->LastLap < FastestLap - 1e-4f))
			{
				if (FastestLap > 0.f && LeadLap >= 3) { AlertTxt = FString::Printf(TEXT("MEILLEUR TOUR EN COURSE · %s · %s"), *Code3(C->DriverName), *Chrono(C->LastLap)); AlertT = 3.f; }
				FastestLap = C->LastLap; FastestCar = C; FastestCode = Code3(C->DriverName);
			}
			for (int32 K = 0; K < 3; ++K) { Tm.Last[K] = Tm.Cur[K]; Tm.LastCol[K] = Tm.CurCol[K]; Tm.Cur[K] = 0.f; Tm.CurCol[K] = 0; }
			Tm.SecLap = C->Lap;
			Tm.Sec = Third == 0 ? 0 : -1;
		}
		else if (Tm.Sec >= 0 && Third == Tm.Sec + 1)
		{
			const float At = C->LapTime();
			Tm.SecAt[Tm.Sec] = At;
			Done(Tm.Sec, Tm.Sec == 0 ? At : At - Tm.SecAt[0]);
			Tm.Sec = Third;
		}
		else if (Tm.Sec >= 0 && Third > Tm.Sec + 1) Tm.Sec = -1;   // saut (voiture replacée) : secteurs de ce tour ignorés
	}

	// profil du tour du joueur (80 points) : le tour gardé est le meilleur
	if (Car->Lap >= 1)
	{
		if (CurProf.Num() != 80) CurProf.Init(-1.f, 80);
		if (Car->Lap != ProfLap)
		{
			int32 Filled = 0;
			for (const float Pt : CurProf) Filled += Pt >= 0.f ? 1 : 0;
			if (ProfLap >= 1 && Filled >= 72 && Car->LastLap > 0.f && FMath::IsNearlyEqual(Car->LastLap, Car->BestLap, 1e-3f))
			{
				BestProf = CurProf;
				BestProf.Add(Car->LastLap);
			}
			CurProf.Init(-1.f, 80);
			ProfLap = Car->Lap;
		}
		const int32 P = FMath::Clamp(Car->Idx * 80 / N, 0, 79);
		if (CurProf[P] < 0.f) CurProf[P] = Car->LapTime();
	}
	// relais en cours : tours depuis le dernier arrêt, carburant au départ du relais
	if (Car->Stops != StintStops) { StintStops = Car->Stops; StintLap0 = FMath::Max(1, Car->Lap); StintFuel = Car->Fuel; }
	StintFuel = FMath::Max(StintFuel, Car->Fuel);
}

// Écart en temps entre deux voitures : au dernier point de passage de celle de derrière (comme les chronos de la télé),
// qui grandit en direct si elle ne passe plus (arrêt) ; sinon estimé avec la distance
float AGPMHUD::TimeGap(const AGPMCar* Ahead, const AGPMCar* Behind, float PerLap, float LapEst) const
{
	const FCarTiming* TA = Timing.Find(Ahead);
	const FCarTiming* TB = Timing.Find(Behind);
	if (TA && TB && TB->LastMark >= 0)
	{
		const int32 g = TB->LastMark;
		if (TA->MarkId[g & 127] == g)
		{
			float Gt = TB->Mark[g & 127] - TA->Mark[g & 127];
			if (TA->MarkId[(g + 1) & 127] == g + 1) Gt = FMath::Max(Gt, Behind->RaceTime - TA->Mark[(g + 1) & 127]);
			return FMath::Max(0.f, Gt);
		}
	}
	return FMath::Max(0.f, (Ahead->Progress() - Behind->Progress()) / FMath::Max(1.f, PerLap) * LapEst);
}

// ---------- classement ----------

// Classement façon télé : place (podium en blanc), trait de l'écurie, code du pilote, puce violette du meilleur tour,
// intervalle avec la voiture de devant (ou écart avec le leader, touche G), pneus. En qualifications : temps de la séance.
float AGPMHUD::DrawTower(AGPMGameMode* GM, AGPMCar* Car, float X, float Y)
{
	const float S = Scale;
	const FGPMTrackData* T = Car->GetTrack();
	const float PerLap = T ? float(FMath::Max(1, T->WX.Num())) : 1.f;
	const bool bQuali = GM->Phase == AGPMGameMode::EPhase::Quali;
	TArray<FRaceRow> Rows;
	int32 Me = 0;
	if (bQuali)
	{
		// temps des IA révélés au fil du tour du joueur ; le joueur est placé avec son écart en direct sur la pole
		float Delta = 0.f;
		const bool bLive = GM->QDelta(Delta);
		const float* Prev = GM->QTimes.Find(Car);
		const float Reveal = Prev ? 1.f : Car->Lap >= 1 ? 0.3f + 0.7f * Car->Idx / PerLap : 0.3f;
		TArray<TPair<const AGPMCar*, float>> Times;
		float Best = 0.f;
		for (const auto& E : GM->QTimes)
			if (E.Key && E.Key != Car && (GetTypeHash(E.Key->DriverName) % 1000) / 1000.f < Reveal)
			{
				Times.Emplace(E.Key, E.Value);
				Best = Best <= 0.f ? E.Value : FMath::Min(Best, E.Value);
			}
		const float Proj = bLive ? GM->QPole() + Delta : 0.f;
		const float Mine = Prev && *Prev < 900.f ? (Proj > 0.f ? FMath::Min(*Prev, Proj) : *Prev) : Proj > 0.f ? Proj : 1e6f;
		Times.Emplace(Car, Mine);
		Times.Sort([](const TPair<const AGPMCar*, float>& A, const TPair<const AGPMCar*, float>& B) { return A.Value < B.Value; });
		for (const TPair<const AGPMCar*, float>& E : Times)
		{
			FRaceRow Row;
			Row.C = E.Key;
			if (E.Key == Car)
			{
				Me = Rows.Num();
				if (bLive) Row.Gap = FString::Printf(TEXT("%+.3f"), Delta);
				else if (Mine < 1e5f) Row.Gap = Best > 0.f && Mine > Best ? FString::Printf(TEXT("+%.3f"), Mine - Best) : Chrono(Mine);
				else { Row.Gap = TEXT("EN PISTE"); Row.GapKind = 2; }
			}
			else if (FMath::IsNearlyEqual(E.Value, Best)) { Row.Gap = Chrono(E.Value); Row.GapCol = RacePurple; }
			else Row.Gap = FString::Printf(TEXT("+%.3f"), E.Value - Best);
			Rows.Add(Row);
		}
	}
	else
	{
		const TArray<AGPMCar*>& Ord = GM->Standings();
		Me = FMath::Max(0, GM->PositionOf(Car) - 1);
		for (int32 R = 0; R < Ord.Num(); ++R)
		{
			const AGPMCar* C = Ord[R];
			if (!C) continue;
			FRaceRow Row;
			Row.C = C;
			Row.Tyre = C->Tyre;
			Row.bFast = C == FastestCar && FastestLap > 0.f;
			Row.bFlag = C->bFinished;
			if (R == 0) { Row.Gap = TEXT("LEADER"); Row.GapKind = 2; }
			else
			{
				const AGPMCar* Ref = bGapLeader ? Ord[0] : Ord[R - 1];
				const int32 Laps = FMath::FloorToInt((Ref->Progress() - C->Progress()) / PerLap);
				Row.Gap = Laps >= 1 ? FString::Printf(TEXT("+%d T"), Laps) : FString::Printf(TEXT("+%.3f"), TimeGap(Ref, C, PerLap, GM->LapEstimate));
			}
			if (C->PitPhase) { Row.Gap = TEXT("STAND"); Row.GapKind = 2; Row.GapCol = RaceYellow; }
			Rows.Add(Row);
		}
	}
	const int32 Num = Rows.Num();
	if (Num == 0) return Y;

	// lignes affichées : tout le plateau au clavier, 10 en tactile (le podium, puis les voitures autour du joueur)
	const int32 MaxRows = bShowTouch ? 10 : 20;
	TArray<int32> Show;
	if (Num <= MaxRows || Me < MaxRows - 1) { for (int32 k = 0; k < FMath::Min(Num, MaxRows); ++k) Show.Add(k); }
	else
	{
		for (int32 k = 0; k < 3; ++k) Show.Add(k);
		Show.Add(-1);
		const int32 Win = MaxRows - 3, Start = FMath::Clamp(Me - Win / 2, 3, Num - Win);
		for (int32 k = Start; k < Start + Win; ++k) Show.Add(k);
	}

	// en-tête : tour en cours, circuit, type d'écart
	const float TW = 156.f * S, HH = 24.f * S, RowH = (bShowTouch ? 13.f : 13.5f) * S;
	Panel(X, Y, TW, HH, RacePanel);
	DrawRect(HudAcc, X, Y, 2.5f * S, HH);
	if (bQuali)
	{
		Ink(TEXT("QUALIFICATIONS"), X + 8.f * S, Y + 3.f * S, 2, 5.f, RaceDim);
		Ink(Car->Lap >= 1 ? TEXT("TOUR LANCÉ") : TEXT("TOUR DE LANCEMENT"), X + 8.f * S, Y + 10.f * S, 1, 7.f, RaceYellow);
	}
	else
	{
		const AGPMCar* Lead = GM->Standings()[0];
		const FString LapNum = FString::FromInt(FMath::Clamp(Lead ? Lead->Lap : 1, 1, GM->TotalLaps));
		Ink(TEXT("TOUR"), X + 8.f * S, Y + 2.5f * S, 2, 5.f, RaceDim);
		Ink(LapNum, X + 8.f * S, Y + 7.5f * S, 4, 11.f, RaceText);
		Ink(FString::Printf(TEXT("/ %d"), GM->TotalLaps), X + 11.f * S + InkSize(LapNum, 4, 11.f).X, Y + 11.5f * S, 4, 7.f, RaceDim);
	}
	if (T) Ink(GPMUp(T->Name), X + TW - 8.f * S, Y + 3.f * S, 2, 5.f, RaceDim, 1.f);
	Ink(bQuali ? TEXT("TEMPS") : bGapLeader ? TEXT("ÉCART") : TEXT("INTERVALLE"), X + TW - 8.f * S, Y + 10.5f * S, 1, 6.5f, RaceText, 1.f);

	float Yr = Y + HH + 2.f * S;
	for (const int32 R : Show)
	{
		if (R < 0)
		{
			for (int32 d = 0; d < 3; ++d) Paint(4, X + 6.f * S + d * 4.5f * S, Yr + 1.f * S, 2.5f * S, 2.5f * S, RaceDim);
			Yr += 5.f * S;
			continue;
		}
		const FRaceRow& Row = Rows[R];
		const AGPMCar* C = Row.C;
		const bool bMe = C == Car;
		const float RH = RowH - 1.f * S, PW = 15.f * S;
		const FLinearColor Fg = bMe ? InkOnColour(HudAcc) : RaceText;
		DrawRect(bMe ? RaceA(HudAcc, 0.94f) : RaceRow, X, Yr, TW, RH);
		const FLinearColor PosBg = bMe ? FMath::Lerp(HudAcc, FLinearColor::White, 0.3f) : R < 3 ? RaceText : RaceBox;
		DrawRect(PosBg, X, Yr, PW, RH);
		InkV(FString::FromInt(R + 1), X + PW * 0.5f, Yr, RH, 4, 7.5f, InkOnColour(PosBg), 0.5f);
		DrawRect(C->TeamColor, X + PW + 4.f * S, Yr + 2.5f * S, 2.f * S, RH - 5.f * S);
		InkV(Code3(C->DriverName), X + PW + 9.f * S, Yr, RH, 1, 7.5f, Fg);
		if (Row.bFast)
		{
			// puce du meilleur tour en course
			const float FX = X + PW + 40.f * S, FW = 18.f * S, FH = 7.5f * S;
			DrawRect(RacePurple, FX, Yr + (RH - FH) * 0.5f, FW, FH);
			InkV(TEXT("TOUR"), FX + FW * 0.5f, Yr + (RH - FH) * 0.5f, FH, 2, 4.4f, FLinearColor::White, 0.5f);
		}
		const float GapR = X + TW - (Row.Tyre ? 15.f : 6.f) * S;
		const float Sz = Row.GapKind == 2 ? 5.5f : 7.5f;
		InkV(Row.Gap, GapR, Yr, RH, Row.GapKind, Sz, bMe ? Fg : Row.GapCol, 1.f);
		if (Row.bFlag)
		{
			// drapeau à damier devant l'écart
			const float Q = 2.f * S, FX = GapR - InkSize(Row.Gap, Row.GapKind, Sz).X - 4.f * S - 3.f * Q, FY = Yr + RH * 0.5f - Q;
			for (int32 i = 0; i < 6; ++i)
				DrawRect((i + i / 3) % 2 ? RaceInk : FLinearColor::White, FX + (i % 3) * Q, FY + (i / 3) * Q, Q, Q);
		}
		if (Row.Tyre) TyreIcon(Row.Tyre, X + TW - 7.5f * S, Yr + RH * 0.5f, 4.3f * S);
		Yr += RowH;
	}
	return Yr;
}

// ---------- colonne de droite ----------

// Tour en cours : chrono, delta en direct (vert = plus rapide), meilleur tour et record, secteurs colorés
void AGPMHUD::DrawLapPanel(AGPMGameMode* GM, AGPMCar* Car, float X, float Y, float PW)
{
	const float S = Scale, PH = 74.f * S;
	const bool bQuali = GM->Phase == AGPMGameMode::EPhase::Quali;
	Panel(X, Y, PW, PH, RacePanel);
	DrawRect(HudAcc, X, Y, 2.5f * S, PH);
	Ink(bQuali ? (Car->Lap >= 1 ? TEXT("TOUR LANCÉ") : TEXT("TOUR DE LANCEMENT")) : TEXT("TOUR EN COURS"), X + 9.f * S, Y + 4.f * S, 2, 5.f, RaceDim);
	Ink(Chrono(Car->LapTime()), X + 9.f * S, Y + 8.f * S, 4, 19.f, RaceText);

	// delta : sur la pole en qualifications, sur son meilleur tour en course
	float Delta = 0.f;
	bool bDelta = false;
	if (bQuali) bDelta = GM->QDelta(Delta);
	else if (BestProf.Num() == 81 && Car->Lap >= 2 && !Car->PitPhase && Car->GetTrack())
	{
		const float F = FMath::Clamp(Car->Idx * 80.f / FMath::Max(1, Car->GetTrack()->WX.Num()), 0.f, 79.999f);
		const int32 I0 = FMath::FloorToInt(F);
		if (BestProf[I0] >= 0.f && BestProf[I0 + 1] >= 0.f)
		{
			Delta = Car->LapTime() - FMath::Lerp(BestProf[I0], BestProf[I0 + 1], F - I0);
			bDelta = true;
		}
	}
	if (bDelta)
	{
		const FLinearColor DCol = Delta <= 0.f ? RaceGreen : RaceRed;
		const float DW = 50.f * S, DH = 15.f * S, DX = X + PW - 9.f * S - DW, DY = Y + 13.f * S;
		DrawRect(DCol, DX, DY, DW, DH);
		InkV(FString::Printf(TEXT("%+.3f"), Delta), DX + DW * 0.5f, DY, DH, 4, 9.5f, InkOnColour(DCol), 0.5f);
	}

	// meilleur tour à gauche, record à droite (pole à battre en qualifications)
	const float L2 = Y + 37.f * S, BH = 9.f * S;
	const FString LeftLab = bQuali ? TEXT("RECORD PERSO") : TEXT("MEILLEUR");
	const float* Prev = bQuali ? GM->QTimes.Find(Car) : nullptr;
	const float LeftVal = bQuali ? (Prev && *Prev < 900.f ? *Prev : 0.f) : Car->BestLap;
	InkV(LeftLab, X + 9.f * S, L2, BH, 2, 5.f, RaceDim);
	InkV(Chrono(LeftVal), X + 13.f * S + InkSize(LeftLab, 2, 5.f).X, L2, BH, 4, 8.f, RaceText);
	const float RightVal = bQuali ? GM->QPole() : FastestLap;
	const FString RightLab = bQuali ? FString(TEXT("POLE")) : FastestCode.IsEmpty() ? FString(TEXT("RECORD")) : FString::Printf(TEXT("RECORD · %s"), *FastestCode);
	const FString RightTxt = Chrono(RightVal);
	InkV(RightTxt, X + PW - 9.f * S, L2, BH, 4, 8.f, RightVal > 0.f ? RacePurple : RaceDim, 1.f);
	InkV(RightLab, X + PW - 13.f * S - InkSize(RightTxt, 4, 8.f).X, L2, BH, 2, 5.f, RaceDim, 1.f);

	// secteurs : barre colorée, numéro, temps (ceux du tour précédent en grisé tant que le nouveau n'est pas fait)
	const float SY = Y + 52.f * S, Gp = 6.f * S, CW = (PW - 18.f * S - 2.f * Gp) / 3.f;
	const FCarTiming* Tm = Timing.Find(Car);
	for (int32 k = 0; k < 3; ++k)
	{
		float SecT = 0.f;
		int32 Col = 0;
		bool bOld = false;
		if (bQuali)
		{
			bOld = GM->QSec[k] <= 0.f;
			SecT = bOld ? GM->QPB[k] : GM->QSec[k];
			Col = bOld ? 0 : GM->QCol[k];
		}
		else if (Tm)
		{
			bOld = Tm->Cur[k] <= 0.f;
			SecT = bOld ? Tm->Last[k] : Tm->Cur[k];
			Col = bOld ? Tm->LastCol[k] : Tm->CurCol[k];
		}
		const float CX = X + 9.f * S + k * (CW + Gp);
		DrawRect(Col ? RaceA(SecColour(Col), bOld ? 0.35f : 1.f) : RaceLine, CX, SY, CW, 2.5f * S);
		InkV(FString::Printf(TEXT("S%d"), k + 1), CX, SY + 5.f * S, 12.f * S, 2, 5.f, RaceDim);
		InkV(SecT > 0.f ? FString::Printf(TEXT("%.3f"), SecT) : FString(TEXT("--.---")), CX + 11.f * S, SY + 5.f * S, 12.f * S, 4, 8.5f, bOld ? RaceDim : RaceText);
	}
}

// Écarts avec la voiture de devant et celle de derrière
void AGPMHUD::DrawGaps(AGPMGameMode* GM, AGPMCar* Car, float X, float Y, float PW)
{
	const TArray<AGPMCar*>& Ord = GM->Standings();
	const int32 Pos = GM->PositionOf(Car);
	const FGPMTrackData* T = Car->GetTrack();
	if (!T || Pos < 1) return;
	const float S = Scale, RH = 17.f * S, PerLap = float(FMath::Max(1, T->WX.Num()));
	for (int32 k = 0; k < 2; ++k)
	{
		const int32 OtherIdx = k == 0 ? Pos - 2 : Pos;
		const float RY = Y + k * (RH + 3.f * S);
		const AGPMCar* Other = Ord.IsValidIndex(OtherIdx) ? Ord[OtherIdx] : nullptr;
		if (!Other && k == 1) continue;
		Panel(X, RY, PW, RH, RacePanel, 4.f);
		DrawRect(k == 0 ? RacePurple : RaceYellow, X, RY, 2.5f * S, RH);
		InkV(k == 0 ? TEXT("DEVANT") : TEXT("DERRIÈRE"), X + 9.f * S, RY, RH, 2, 5.f, RaceDim);
		if (!Other) { InkV(TEXT("EN TÊTE"), X + PW - 9.f * S, RY, RH, 1, 7.5f, RaceText, 1.f); continue; }
		InkV(FString::Printf(TEXT("P%d"), OtherIdx + 1), X + 52.f * S, RY, RH, 4, 8.f, RaceDim);
		DrawRect(Other->TeamColor, X + 68.f * S, RY + 4.f * S, 2.f * S, RH - 8.f * S);
		InkV(Code3(Other->DriverName), X + 73.f * S, RY, RH, 1, 8.f, RaceText);
		const AGPMCar* Ahead = k == 0 ? Other : Car;
		const AGPMCar* Behind = k == 0 ? Car : Other;
		const int32 Laps = FMath::FloorToInt((Ahead->Progress() - Behind->Progress()) / PerLap);
		const float Gt = TimeGap(Ahead, Behind, PerLap, GM->LapEstimate);
		FString Txt;
		FLinearColor Col = RaceText;
		if (Laps >= 1) Txt = FString::Printf(TEXT("%s%d T"), k == 0 ? TEXT("+") : TEXT("-"), Laps);
		else
		{
			Txt = FString::Printf(TEXT("%s%.3f"), k == 0 ? TEXT("+") : TEXT("-"), Gt);
			Col = k == 0 ? (Gt < 1.f ? RaceGreen : RaceText) : (Gt < 1.f ? RaceYellow : RaceGreen);
		}
		InkV(Txt, X + PW - 9.f * S, RY, RH, 4, 9.5f, Col, 1.f);
	}
}

// Pneus (gomme, tours, usure), carburant (marge en tours) et silhouette de la voiture (ailerons, pneus, crevaison)
void AGPMHUD::DrawTyrePanel(AGPMGameMode* GM, AGPMCar* Car, float X, float Y, float PW)
{
	const float S = Scale, PH = 62.f * S;
	Panel(X, Y, PW, PH, RacePanel);
	DrawRect(HudAcc, X, Y, 2.5f * S, PH);
	const bool bRace = GM && GM->Phase == AGPMGameMode::EPhase::Race;
	const float Life = FMath::Clamp(1.f - Car->Wear, 0.f, 1.f);
	const FLinearColor LifeCol = Car->bFlat ? RaceRed : Life > 0.6f ? RaceGreen : Life > 0.4f ? RaceYellow : RaceRed;
	TyreIcon(Car->Tyre, X + 17.f * S, Y + 14.f * S, 8.f * S);
	Ink(TyreName(Car->Tyre), X + 29.f * S, Y + 3.f * S, 1, 8.f, RaceText);
	const int32 Laps = bRace ? FMath::Max(0, Car->Lap - StintLap0) : 0;
	const FString Sub = Car->bFlat ? FString(TEXT("CREVAISON !"))
		: FString::Printf(TEXT("%d TOUR%s · USURE %d %%"), Laps, Laps > 1 ? TEXT("S") : TEXT(""), FMath::RoundToInt(Car->Wear * 100.f));
	Ink(Sub, X + 29.f * S, Y + 15.f * S, 2, 5.f, Car->bFlat ? RaceRed : RaceDim);
	DrawRect(RaceLine, X + 9.f * S, Y + 27.f * S, 96.f * S, 1.f * S);

	Ink(TEXT("CARBURANT"), X + 9.f * S, Y + 30.f * S, 2, 5.f, RaceDim);
	Ink(Comma(FString::Printf(TEXT("%.1f KG"), Car->Fuel)), X + 9.f * S, Y + 34.f * S, 4, 11.f, RaceText);
	FLinearColor FuelCol = RaceGreen;
	const FGPMTrackData* T = Car->GetTrack();
	const float PerLap = T ? T->WX.Num() * 2.f / 1000.f * 1.7f * Car->UpFuel : 0.f;
	if (bRace && PerLap > 0.f)
	{
		// marge : tours d'essence en plus de ceux qu'il faut jusqu'au prochain ravitaillement (ou à l'arrivée)
		const float Margin = Car->Fuel / PerLap - GM->LapsToRefuel(Car);
		FuelCol = Margin < 0.f ? RaceRed : Margin < 0.5f ? RaceYellow : RaceGreen;
		Ink(Comma(FString::Printf(TEXT("%+.1f TOUR%s DE MARGE"), Margin, FMath::Abs(Margin) >= 2.f ? TEXT("S") : TEXT(""))), X + 9.f * S, Y + 48.f * S, 2, 5.f, FuelCol);
	}
	const float Frac = StintFuel > 0.f && bRace ? FMath::Clamp(Car->Fuel / StintFuel, 0.f, 1.f) : 1.f;
	DrawRect(RaceLine, X + 9.f * S, Y + 56.f * S, 96.f * S, 2.f * S);
	DrawRect(FuelCol, X + 9.f * S, Y + 56.f * S, 96.f * S * Frac, 2.f * S);

	// silhouette vue de dessus : ailerons selon les dégâts, pneus selon l'usure
	auto Wing = [](float D, bool bOff) { return bOff || D > 0.5f ? RaceRed : D > 0.15f ? RaceYellow : FLinearColor(0.55f, 0.58f, 0.64f, 1.f); };
	const float Cx = X + PW - 24.f * S, Ty = Y + 7.f * S;
	const FLinearColor Body(0.12f, 0.14f, 0.18f, 1.f), Tyres = RaceA(LifeCol, 0.95f);
	DrawRect(Wing(Car->Dmg, Car->bFwOff), Cx - 13.f * S, Ty, 26.f * S, 3.5f * S);
	DrawRect(Body, Cx - 2.f * S, Ty + 3.5f * S, 4.f * S, 11.f * S);
	DrawRect(Tyres, Cx - 13.f * S, Ty + 6.f * S, 6.f * S, 10.f * S);
	DrawRect(Tyres, Cx + 7.f * S, Ty + 6.f * S, 6.f * S, 10.f * S);
	DrawRect(Body, Cx - 4.f * S, Ty + 14.f * S, 8.f * S, 25.f * S);
	DrawRect(Body, Cx - 8.f * S, Ty + 20.f * S, 16.f * S, 12.f * S);
	DrawRect(Tyres, Cx - 14.f * S, Ty + 30.f * S, 7.f * S, 12.f * S);
	DrawRect(Tyres, Cx + 7.f * S, Ty + 30.f * S, 7.f * S, 12.f * S);
	DrawRect(Wing(Car->DmgR, Car->bRwOff), Cx - 10.f * S, Ty + 44.f * S, 20.f * S, 3.5f * S);
	if (Car->bFwOff) DrawLine(Cx - 15.f * S, Ty + 5.f * S, Cx + 15.f * S, Ty - 1.5f * S, FLinearColor::White, 1.5f * S);
	if (Car->bRwOff) DrawLine(Cx - 12.f * S, Ty + 49.f * S, Cx + 12.f * S, Ty + 42.5f * S, FLinearColor::White, 1.5f * S);
}

// Compte-tours en trapèze : 16 diodes (vert, jaune, rouge, bleu ; tout clignote au rupteur), vitesse, rapport,
// mode de pilotage, aspiration, aides et demande d'arrêt ; pénalité au-dessus
void AGPMHUD::DrawCluster(AGPMCar* Car, float W, float H)
{
	const float S = Scale, CW = 216.f * S, CH = 58.f * S, X = (W - CW) * 0.5f, Y = H - CH - (bShowTouch ? 4.f : 8.f) * S;
	Paint(7, X, Y, CW, CH, RacePanel);
	if (Car->Penalty > 0.f)
	{
		const FString Pen = FString::Printf(TEXT("PÉNALITÉ +%.0f S"), Car->Penalty);
		const float PW = InkSize(Pen, 1, 7.f).X + 20.f * S, PH = 14.f * S;
		DrawRect(RaceRed, (W - PW) * 0.5f, Y - PH - 4.f * S, PW, PH);
		InkV(Pen, W * 0.5f, Y - PH - 4.f * S, PH, 1, 7.f, FLinearColor::White, 0.5f);
	}

	const float Frac = FMath::Clamp((Car->Rpm() - 6500.f) / 5500.f, 0.f, 1.f);
	const float Kmh = FMath::Abs(Car->SpeedKmh());
	const int32 Lit = Kmh < 2.f ? 0 : FMath::CeilToInt(Frac * 16.f);
	const bool bBlink = Frac > 0.96f && FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 0.16f) < 0.08f;
	const float LW = 150.f * S, LG = 1.5f * S, DW = (LW - 15.f * LG) / 16.f, LX = X + (CW - LW) * 0.5f;
	for (int32 k = 0; k < 16; ++k)
	{
		const FLinearColor Col = k < 8 ? RaceGreen : k < 12 ? RaceYellow : k < 14 ? RaceRed : RaceBlue;
		const bool bOn = k < Lit && !bBlink;
		DrawRect(bOn ? Col : FLinearColor(Col.R * 0.16f, Col.G * 0.16f, Col.B * 0.16f, 0.9f), LX + k * (DW + LG), Y + 6.f * S, DW, 4.f * S);
	}

	// rapport au centre (bleu au rupteur), vitesse à gauche
	const int32 Gr = Car->Gear();
	const float GB = 30.f * S, GX = X + (CW - GB) * 0.5f, GY = Y + 16.f * S;
	const FLinearColor GearBg = Frac > 0.9f ? RaceBlue : HudAcc;
	DrawRect(GearBg, GX, GY, GB, GB);
	InkV(Gr == 0 ? FString(TEXT("N")) : FString::FromInt(Gr), GX + GB * 0.5f, GY, GB, 4, 24.f, InkOnColour(GearBg), 0.5f);
	InkV(FString::FromInt(FMath::RoundToInt(Kmh)), GX - 7.f * S, GY - 2.f * S, GB, 4, 25.f, RaceText, 1.f);
	Ink(TEXT("KM/H"), GX - 7.f * S, GY + GB - 1.f * S, 2, 4.5f, RaceDim, 1.f);

	// mode de pilotage, aspiration, aides
	const float MX = GX + GB + 8.f * S, MW = 62.f * S, MH = 11.f * S, MY = GY + 1.f * S;
	const FLinearColor ModeCol = Car->Mode == 0 ? FLinearColor(1.f, 0.16f, 0.1f) : Car->Mode == 2 ? RaceGreen : RaceDim;
	DrawRect(FLinearColor(ModeCol.R * 0.22f, ModeCol.G * 0.22f, ModeCol.B * 0.22f, 0.95f), MX, MY, MW, MH);
	InkV(Car->Mode == 0 ? TEXT("ATTAQUE") : Car->Mode == 2 ? TEXT("ÉCONOMIE") : TEXT("NORMAL"), MX + MW * 0.5f, MY, MH, 1, 6.f, ModeCol, 0.5f);
	Ink(TEXT("ASPIRATION"), MX, MY + 13.f * S, 2, 4.5f, RaceDim);
	DrawRect(RaceLine, MX, MY + 21.f * S, MW, 2.5f * S);
	DrawRect(RaceBlue, MX, MY + 21.f * S, MW * FMath::Clamp(Car->Slipstream, 0.f, 1.f), 2.5f * S);
	FString Aids;
	if (Car->bAbsOn) Aids += TEXT("ABS  ");
	if (Car->bTcOn) Aids += TEXT("TC  ");
	if (Car->bBox) Aids += TEXT("BOX");
	if (!Aids.IsEmpty()) Ink(Aids.TrimEnd(), MX, MY + 26.f * S, 2, 4.5f, Car->bBox ? RaceBlue : RaceYellow);
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

// Message du muret en haut au centre : pastille haut-parleur, « RADIO · MURET », texte sur plusieurs lignes ; renvoie le bas
float AGPMHUD::DrawRadioMsg(const FString& Msg, float W, float Y)
{
	const float S = Scale, MaxW = 280.f * S, Sz = 8.f;
	TArray<FString> Words;
	Msg.ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Cur;
	for (const FString& Wd : Words)
	{
		const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
		if (InkSize(Try, 3, Sz).X > MaxW && !Cur.IsEmpty()) { Lines.Add(Cur); Cur = Wd; }
		else Cur = Try;
	}
	if (!Cur.IsEmpty()) Lines.Add(Cur);
	if (Lines.Num() == 0) return Y;
	float TextW = InkSize(TEXT("RADIO · MURET"), 2, 5.f).X;
	for (const FString& Ln : Lines) TextW = FMath::Max(TextW, InkSize(Ln, 3, Sz).X);
	const float LH = InkSize(TEXT("Ég"), 3, Sz).Y * 0.9f, IconW = 30.f * S, Pad = 10.f * S, Head = 12.f * S;
	const float BoxW = IconW + TextW + Pad, BoxH = Head + Lines.Num() * LH + 4.f * S, RX = (W - BoxW) * 0.5f;
	const FLinearColor Lab = FMath::Lerp(HudAcc, FLinearColor::White, 0.3f);
	Panel(RX, Y, BoxW, BoxH, RacePanel);
	DrawRect(HudAcc, RX, Y, BoxW - 6.f * S, 2.f * S);
	// pastille haut-parleur qui pulse pendant le message
	const float Cx = RX + 15.f * S, Cy = Y + FMath::Min(BoxH * 0.5f, 17.f * S), Rd = 8.f * S;
	const float Pulse = 0.55f + 0.45f * FMath::Abs(FMath::Sin(float(GetWorld()->GetRealTimeSeconds()) * 5.f));
	Paint(3, Cx - Rd * 1.9f, Cy - Rd * 1.9f, Rd * 3.8f, Rd * 3.8f, RaceA(HudAcc, 0.45f * Pulse));
	Paint(4, Cx - Rd, Cy - Rd, Rd * 2.f, Rd * 2.f, HudAcc);
	const FLinearColor Ic = InkOnColour(HudAcc);
	DrawRect(Ic, Cx - 3.5f * S, Cy - 1.5f * S, 2.5f * S, 3.f * S);
	DrawLine(Cx - 1.f * S, Cy - 1.5f * S, Cx + 2.5f * S, Cy - 4.f * S, Ic, 1.2f * S);
	DrawLine(Cx + 2.5f * S, Cy - 4.f * S, Cx + 2.5f * S, Cy + 4.f * S, Ic, 1.2f * S);
	DrawLine(Cx + 2.5f * S, Cy + 4.f * S, Cx - 1.f * S, Cy + 1.5f * S, Ic, 1.2f * S);
	Ink(TEXT("RADIO · MURET"), RX + IconW, Y + 3.5f * S, 2, 5.f, Lab);
	for (int32 i = 0; i < Lines.Num(); ++i) Ink(Lines[i], RX + IconW, Y + Head + i * LH, 3, Sz, RaceText);
	return Y + BoxH;
}

// Radio du muret : point complet, rythme (mode affiché), pneus du prochain arrêt ; touches 1 2 3 sur ordinateur
void AGPMHUD::DrawRadioPad(AGPMGameMode* GM, AGPMCar* Car)
{
	const float S = Scale;
	const TCHAR NextT = Car->bBox ? Car->NextTyre : GM->NextStopTyre(Car);
	const FLinearColor ModeCol = Car->Mode == 0 ? FLinearColor(1.f, 0.16f, 0.1f) : Car->Mode == 2 ? RaceGreen : RaceText;
	const FString Keys[3] = { TEXT("1"), TEXT("2"), TEXT("3") };
	const FString Labels[3] = { TEXT("POINT RADIO"), Car->Mode == 0 ? FString(TEXT("ATTAQUE")) : Car->Mode == 2 ? FString(TEXT("ÉCONOMIE")) : FString(TEXT("RYTHME NORMAL")), TEXT("PNEUS SUIVANTS") };
	const FLinearColor Marks[3] = { HudAcc, ModeCol, TyreColour(NextT) };
	for (int32 k = 0; k < 3; ++k)
	{
		const FButton& Bt = RadioBtn[k];
		const float BX = Bt.Pos.X, BY = Bt.Pos.Y, BW = Bt.Size.X, BH = Bt.Size.Y;
		Panel(BX, BY, BW, BH, bRadioDown[k] ? RaceA(RaceLine, 0.95f) : RacePanel, 4.f);
		DrawRect(Marks[k], BX, BY, 2.5f * S, BH);
		float TX = BX + 9.f * S;
		if (!bShowTouch)
		{
			// touche du clavier
			const float KS = 12.f * S, KY = BY + (BH - KS) * 0.5f;
			DrawRect(RaceBox, TX, KY, KS, KS);
			DrawRect(RaceLine, TX, KY + KS - 1.5f * S, KS, 1.5f * S);
			InkV(Keys[k], TX + KS * 0.5f, KY, KS, 1, 6.5f, RaceText, 0.5f);
			TX += KS + 6.f * S;
		}
		InkV(Labels[k], TX, BY, BH, 2, 5.5f, k == 1 ? ModeCol : RaceText);
		if (k == 2) TyreIcon(NextT, BX + BW - 13.f * S, BY + BH * 0.5f, 6.5f * S);
	}
}

// Météo (colonne de droite, à gauche du bouton pause) : pluie, humidité de la piste, changement annoncé
void AGPMHUD::DrawWeatherChip(AGPMGameMode* GM, float RightX, float Y)
{
	const bool bChange = GM->WxChangeAt >= 0.f && !GM->bWxDone;
	if (!GM->bRaining && GM->TrackWet < 0.03f && !bChange) return;
	const float S = Scale, CW = 128.f * S, CH = (bChange ? 36.f : 26.f) * S, X0 = RightX - CW;
	Panel(X0, Y, CW, CH, RacePanel, 4.f);
	DrawRect(GM->bRaining ? RaceBlue : RaceDim, X0, Y, 2.5f * S, CH);
	float TX = X0 + 9.f * S;
	if (GM->bRaining)
	{
		// trois gouttes penchées
		for (int32 d = 0; d < 3; ++d) Paint(1, TX + d * 4.5f * S, Y + (4.f + (d % 2) * 2.f) * S, 2.5f * S, 7.f * S, RaceBlue);
		TX += 17.f * S;
	}
	const FString Top = GM->bRaining ? FString(TEXT("PLUIE")) : GM->TrackWet > 0.25f ? FString(TEXT("PISTE HUMIDE")) : FString(TEXT("TEMPS SEC"));
	Ink(Top, TX, Y + 3.f * S, 1, 6.5f, GM->bRaining ? RaceBlue : RaceText);
	const float BarX = X0 + 9.f * S, BarW = CW - 17.f * S;
	DrawRect(RaceLine, BarX, Y + 18.f * S, BarW, 3.f * S);
	DrawRect(RaceBlue, BarX, Y + 18.f * S, BarW * FMath::Clamp(GM->TrackWet, 0.f, 1.f), 3.f * S);
	if (bChange) Ink(GM->bRainStart ? TEXT("ÉCLAIRCIE ATTENDUE") : TEXT("AVERSE ATTENDUE"), BarX, Y + 23.f * S, 2, 5.f, RaceYellow);
}

void AGPMHUD::DrawPause(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	const float S = Scale;
	DrawRect(FLinearColor(0.002f, 0.003f, 0.006f, 0.8f), 0, 0, W, H);
	Paint(2, 0, 0, W * 0.6f, H, RaceA(HudAcc, 0.18f));
	Ink(GM && GM->bCareer ? TEXT("GRAND PRIX") : TEXT("COURSE SOLO"), W * 0.5f, H * 0.08f, 2, 7.f, RaceDim, 0.5f);
	Ink(TEXT("PAUSE"), W * 0.5f, H * 0.08f + 11.f * S, 0, 30.f, RaceText, 0.5f);
	DrawRect(HudAcc, W * 0.5f - 24.f * S, H * 0.08f + 60.f * S, 48.f * S, 3.f * S);
	const float BW = 240.f * S, BH = 32.f * S, X = (W - BW) * 0.5f;
	float Y = H * 0.33f;
	const FLinearColor Alt(0.0109f, 0.0159f, 0.0296f, 0.96f);
	if (Btn(TEXT("REPRENDRE"), X, Y, BW, BH, HudAcc)) { SetPaused(false); return; }
	Y += BH + 8.f * S;
	if (Btn(TEXT("RECOMMENCER LA COURSE"), X, Y, BW, BH, Alt)) { SetPaused(false); if (GM) GM->Restart(); return; }
	Y += BH + 8.f * S;
	if (Btn(FString::Printf(TEXT("CAMÉRA : %s"), *Car->CamLabel()), X, Y, BW, BH, Alt)) Car->ToggleCamera();
	Y += BH + 8.f * S;
	if (Btn(bShowFps ? TEXT("COMPTEUR I/S : OUI") : TEXT("COMPTEUR I/S : NON"), X, Y, BW, BH, Alt)) bShowFps = !bShowFps;
	Y += BH + 8.f * S;
	if (Btn(GM && GM->bCareer ? TEXT("QUITTER LE GRAND PRIX") : TEXT("MENU PRINCIPAL"), X, Y, BW, BH, Alt)) { SetPaused(false); if (GM) GM->GoHome(); return; }
	Ink(TEXT("ÉCHAP OU P : REPRENDRE"), W * 0.5f, Y + BH + 12.f * S, 2, 6.f, RaceDim, 0.5f);
}

void AGPMHUD::DrawRace(AGPMGameMode* GM, AGPMCar* Car, float W, float H)
{
	const float S = Scale, M = 10.f * S;
	// couleur de l'écurie du joueur (éclaircie si elle est trop sombre pour se voir)
	HudAcc = FLinearColor(Car->TeamColor.R, Car->TeamColor.G, Car->TeamColor.B, 1.f);
	if (HudAcc.GetLuminance() < 0.06f) HudAcc = HudAcc * 0.4f + FLinearColor(0.55f, 0.58f, 0.66f, 1.f) * 0.6f;
	APlayerController* PC = GetOwningPlayerController();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	const bool bLive = GM && (GM->Phase == AGPMGameMode::EPhase::Race || GM->Phase == AGPMGameMode::EPhase::Lights || GM->Phase == AGPMGameMode::EPhase::Quali);
	if (PC && PC->WasInputKeyJustPressed(EKeys::F)) bShowFps = !bShowFps;
	if (PC && PC->WasInputKeyJustPressed(EKeys::G)) bGapLeader = !bGapLeader;
	if (PC && bLive && (PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::P))) { SetPaused(!bPaused); return; }
	if (bPaused && !bLive) SetPaused(false);

	if (GM && (GM->Phase == AGPMGameMode::EPhase::QualiDone || GM->Phase == AGPMGameMode::EPhase::Done)) return;
	DrawRain(GM, Car, W, H);
	const bool bQuali = GM && GM->Phase == AGPMGameMode::EPhase::Quali;
	const bool bRacing = GM && (GM->Phase == AGPMGameMode::EPhase::Race || GM->Phase == AGPMGameMode::EPhase::Lights);
	if (bRacing) UpdateTiming(GM, Car, GetWorld()->GetDeltaSeconds());

	// bandeau centré, penché, à la taille du texte
	auto Banner = [this, W, S](const FString& Text, float Y, const FLinearColor& Bg, const FLinearColor& Fg, float Size)
	{
		const FVector2D Sz = InkSize(Text, 0, Size);
		const float BW = Sz.X + 40.f * S, BH = Sz.Y + 6.f * S;
		Slab((W - BW) * 0.5f, Y, BW, BH, Bg);
		InkV(Text, W * 0.5f, Y, BH, 0, Size, Fg, 0.5f);
		return Y + BH;
	};

	// colonne de gauche : classement, puis pneus et carburant (en bas au clavier, sous le classement en tactile)
	float TowerBottom = M;
	if (GM && (bQuali || GM->NumCars() > 0)) TowerBottom = DrawTower(GM, Car, M, M);
	const float TPW = 156.f * S, TPH = 62.f * S;
	DrawTyrePanel(GM, Car, M, bShowTouch ? TowerBottom + 6.f * S : FMath::Max(TowerBottom + 6.f * S, H - M - TPH), TPW);

	// colonne de droite : tour en cours et secteurs, écarts, radio du muret, météo et pause
	const float RW = 176.f * S, RX = W - M - RW;
	if (GM && (bQuali || bRacing))
	{
		DrawLapPanel(GM, Car, RX, M, RW);
		if (GM->Phase == AGPMGameMode::EPhase::Race) DrawGaps(GM, Car, RX, M + 80.f * S, RW);
	}

	if (GM && GM->NumCars() > 0)
	{
		// en haut au centre, les uns sous les autres : radio, alerte (secteur, meilleur tour), voiture de sécurité
		float TopY = M;
		if (GM->RadioT > 0.f && !GM->Radio.IsEmpty()) TopY = DrawRadioMsg(GM->Radio, W, TopY) + 4.f * S;
		if (AlertT > 0.f && !AlertTxt.IsEmpty())
		{
			const float A = FMath::Clamp(AlertT / 0.4f, 0.f, 1.f), AH = 14.f * S;
			const float AW = InkSize(AlertTxt, 1, 6.5f).X + 24.f * S;
			DrawRect(RaceA(RacePurple, 0.95f * A), (W - AW) * 0.5f, TopY, AW, AH);
			InkV(AlertTxt, W * 0.5f, TopY, AH, 1, 6.5f, RaceA(FLinearColor::White, A), 0.5f);
			TopY += AH + 4.f * S;
		}
		if (GM->SafetyOut()) Banner(TEXT("VOITURE DE SÉCURITÉ"), TopY, RaceYellow, RaceInk, 11.f);

		if (Car->PitPhase)
		{
			// voie des stands : où est le carré de l'écurie ?
			FString Msg = Car->PitPhase == 2 ? (Car->PitStopT > 1e5f ? FString(TEXT("ARRÊT AU STAND")) : FString::Printf(TEXT("PNEUS… %.1f S"), FMath::Max(0.f, Car->PitStopT)))
				: Car->PitPhase == 3 ? FString(TEXT("SORTIE · LIMITEUR 80 KM/H")) : FString(TEXT("LIMITEUR 80 KM/H"));
			if (Car->bPitManual && Car->PitPhase == 1 && Car->GetTrack())
			{
				const int32 D = FMath::RoundToInt((FGPMTrackData::PitBox(Car->TeamIdx) - Car->PitKf) * 2.f);
				const bool bLeftOf = Car->Lat < Car->GetTrack()->SlotLc() - 0.9f;
				Msg = Car->InSlot() ? FString(TEXT("FREINE !")) : D > 2 ? FString::Printf(TEXT("CARRÉ JAUNE DANS %d M%s"), D, bLeftOf ? TEXT(" · SERRE À DROITE") : TEXT(""))
					: D < -2 ? FString::Printf(TEXT("RECULE DE %d M (FREIN À L'ARRÊT)"), -D) : FString(TEXT("SERRE À DROITE"));
			}
			Banner(Msg, H * 0.4f, RacePanel, RaceYellow, 13.f);
		}
		if (GM->Phase == AGPMGameMode::EPhase::Race && !Car->bFinished) DrawRadioPad(GM, Car);
		if (GM->Phase == AGPMGameMode::EPhase::Lights) DrawLights(GM->LightsOn(), W);
		else if (GM->Phase == AGPMGameMode::EPhase::Race && GM->Clock < 1.5f)
		{
			// « GO ! » vert avec un halo, qui s'efface
			const float A = FMath::Clamp((1.5f - GM->Clock) * 2.f, 0.f, 1.f), GY = H * 0.24f;
			Paint(3, W * 0.5f - 150.f * S, GY - 50.f * S, 300.f * S, 170.f * S, RaceA(RaceGreen, 0.35f * A));
			Ink(TEXT("GO !"), W * 0.5f, GY, 0, 46.f, RaceA(RaceGreen, A), 0.5f);
		}
		if (Car->bFinished)
		{
			const int32 Pos = GM->PositionOf(Car);
			const FLinearColor PosCol = Pos == 1 ? RaceYellow : RaceText;
			const float FY = H * 0.24f, FW = 200.f * S, FH = 72.f * S, FX = (W - FW) * 0.5f;
			Panel(FX, FY, FW, FH, RacePanel);
			DrawRect(HudAcc, FX, FY, FW - 6.f * S, 2.f * S);
			Ink(TEXT("ARRIVÉE"), W * 0.5f, FY + 8.f * S, 2, 6.f, RaceDim, 0.5f);
			Ink(FString::Printf(TEXT("P%d"), Pos), W * 0.5f, FY + 16.f * S, 4, 34.f, PosCol, 0.5f);
		}
	}

	DrawCluster(Car, W, H);

	// mini-carte en bas à droite (au clavier ; en tactile, la place est aux pédales)
	if (!bShowTouch)
	{
		const float MS = 92.f * S;
		DrawMiniMap(Car, FVector2D(W - M - MS - 8.f * S, H - M - MS - 8.f * S), MS);
	}
	const float PauseY = 212.f * S;
	if (GM) DrawWeatherChip(GM, W - M - 46.f * S, PauseY);
	if (bLive && !bPaused && Btn(TEXT("II"), W - M - 40.f * S, PauseY, 40.f * S, 26.f * S, RaceBox)) { SetPaused(true); return; }
	if (bShowFps && GM)
		Ink(FString::Printf(TEXT("%.0f I/S  ·  %d %%"), GM->Fps, GM->ScreenPct), W - M, PauseY + 30.f * S, 1, 7.f, GM->Fps < 50.f ? RaceRed : RaceGreen, 1.f);

	// commandes tactiles, ou aide clavier les 20 premières secondes (une ligne au-dessus du compte-tours)
	if (bShowTouch)
	{
		DrawButton(Left, bLeft);
		DrawButton(Right, bRight);
		DrawButton(Brake, bBrake);
		DrawButton(Gas, bGas);
	}
	else if (GM && GM->Clock < 20.f)
	{
		static const TCHAR* const KeyHelp[][2] = {
			{ TEXT("FLÈCHES / ZQSD"), TEXT("CONDUIRE") }, { TEXT("C"), TEXT("CAMÉRA") }, { TEXT("1 2 3"), TEXT("RADIO") },
			{ TEXT("G"), TEXT("ÉCARTS") }, { TEXT("R"), TEXT("REPLACER") }, { TEXT("P"), TEXT("PAUSE") }, { TEXT("T"), TEXT("TACTILE") } };
		const float KH = 13.f * S, Pad = 4.f * S, Sep = 11.f * S;
		float Total = -Sep;
		for (const auto& KP : KeyHelp) Total += InkSize(KP[0], 1, 6.5f).X + 8.f * S + Pad + InkSize(KP[1], 2, 5.f).X + Sep;
		float KX = (W - Total) * 0.5f;
		const float KY = H - 58.f * S - 8.f * S - KH - 8.f * S;
		Panel(KX - 8.f * S, KY - 4.f * S, Total + 16.f * S, KH + 8.f * S, RaceA(RacePanel, 0.85f), 4.f);
		for (const auto& KP : KeyHelp)
		{
			const float KW = InkSize(KP[0], 1, 6.5f).X + 8.f * S;
			DrawRect(RaceBox, KX, KY, KW, KH);
			DrawRect(RaceLine, KX, KY + KH - 1.5f * S, KW, 1.5f * S);
			InkV(KP[0], KX + KW * 0.5f, KY, KH, 1, 6.5f, RaceText, 0.5f);
			KX += KW + Pad;
			InkV(KP[1], KX, KY, KH, 2, 5.f, RaceDim);
			KX += InkSize(KP[1], 2, 5.f).X + Sep;
		}
	}

	if (bPaused) DrawPause(GM, Car, W, H);
}
