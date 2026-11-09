// Menus et écrans de gestion dessinés sur le canvas (index.html du jeu web) : accueil, création de l'écurie,
// course solo, réglages, fenêtres à choix (presse, flash info, histoires, négociations), mercato, licenciement,
// titres des cinématiques et carte du sacre. Tout se touche au doigt (ou à la souris) ; mise à l'échelle sur 844 × 390.
#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor MenuBg(0.01f, 0.015f, 0.03f, 0.88f);
	const FLinearColor CardBg(1.f, 1.f, 1.f, 0.07f);
	const FLinearColor CardOn(1.f, 0.85f, 0.2f, 0.22f);
	const FLinearColor Acc(0.85f, 0.12f, 0.12f, 1.f);
	const FLinearColor Gold(1.f, 0.85f, 0.2f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.65f);
	const FLinearColor Grey(0.25f, 0.27f, 0.3f, 1.f);
	const TCHAR* GoalText(const FString& G)
	{
		return G == TEXT("finish") ? TEXT("voir l'arrivée") : G == TEXT("beatMate") ? TEXT("battre ton équipier") : G == TEXT("gain") ? TEXT("gagner des places")
			: G == TEXT("pts") ? TEXT("marquer des points") : G == TEXT("podium") ? TEXT("monter sur le podium") : TEXT("meilleur tour");
	}
}

FString AGPMHUD::Nm(const FString& Driver) const
{
	const UGameInstance* GI = GetGameInstance();
	const UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
	return Career ? Career->Display(Driver) : Driver;
}

bool AGPMHUD::Btn(const FString& Label, float X, float Y, float BW, float BH, FLinearColor Col, bool bOn)
{
	DrawBtn(Label, X, Y, BW, BH, bOn ? Col : Grey);
	return bOn && Clicked(X, Y, BW, BH);
}

float AGPMHUD::DrawWrap(const FString& Text, float X, float Y, float MaxW, FLinearColor Color, float S)
{
	UFont* Mid = GEngine->GetMediumFont();
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "));
	FString Line;
	float Yc = Y, TW = 0.f, TH = 0.f;
	GetTextSize(TEXT("Ag"), TW, TH, Mid, S);
	const float LineH = TH * 1.05f;
	for (int32 k = 0; k < Words.Num(); ++k)
	{
		const FString Try = Line.IsEmpty() ? Words[k] : Line + TEXT(" ") + Words[k];
		GetTextSize(Try, TW, TH, Mid, S);
		if (TW > MaxW && !Line.IsEmpty()) { DrawText(Line, Color, X, Yc, Mid, S); Yc += LineH; Line = Words[k]; }
		else Line = Try;
	}
	if (!Line.IsEmpty()) { DrawText(Line, Color, X, Yc, Mid, S); Yc += LineH; }
	return Yc - Y;
}

// ---------- accueil ----------
void AGPMHUD::DrawHome(AGPMGameMode* GM, float W, float H)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), 0, 0, W, H);
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	DrawCentered(TEXT("GRAND PRIX MANAGER"), FLinearColor::White, W, 30 * Scale, Big, Scale * 2.f);
	DrawCentered(TEXT("Course de monoplaces et gestion d'écurie · univers 100 % fictif"), Dim, W, 84 * Scale, Mid, Scale);
	UGameInstance* GI = GetGameInstance();
	UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
	UGPMSeasonSubsystem* Season = GI ? GI->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
	const float BW = 320 * Scale, BH = 38 * Scale, X = (W - BW) / 2;
	float Y = 120 * Scale;
	if (Career && Career->IsCreated() && Season && Career->Save())
	{
		if (Btn(FString::Printf(TEXT("CONTINUER : %s · saison %d · GP %d/%d"), *Career->Save()->TeamName, Season->Year(), FMath::Min(Season->Round() + 1, Season->NumRounds()), Season->NumRounds()),
			X, Y, BW, BH, Acc)) { GM->ContinueCareer(); return; }
		Y += BH + 10 * Scale;
		if (bConfirmNew)
		{
			if (Btn(TEXT("Confirmer : effacer la carrière en cours"), X, Y, BW, BH, FLinearColor(0.6f, 0.1f, 0.1f))) { bConfirmNew = false; GM->NewCareer(); return; }
		}
		else if (Btn(TEXT("NOUVELLE CARRIÈRE"), X, Y, BW, BH, FLinearColor(0.3f, 0.32f, 0.38f))) bConfirmNew = true;
	}
	else if (Btn(TEXT("CARRIÈRE : CRÉER TON ÉCURIE"), X, Y, BW, BH, Acc)) { GM->NewCareer(); return; }
	Y += BH + 10 * Scale;
	if (Btn(TEXT("COURSE SOLO"), X, Y, BW, BH, FLinearColor(0.15f, 0.4f, 0.75f))) { GM->Phase = AGPMGameMode::EPhase::Solo; return; }
	Y += BH + 10 * Scale;
	if (Btn(TEXT("RÉGLAGES"), X, Y, BW, BH, FLinearColor(0.3f, 0.32f, 0.38f))) GM->Phase = AGPMGameMode::EPhase::Settings;
	DrawCentered(TEXT("Clavier : flèches/ZQSD conduire · C caméra · B stand · Entrée valider · Échap retour"), Dim, W, H - 24 * Scale, Mid, Scale * 0.85f);
}

// ---------- création de l'écurie (6 écrans) ----------
void AGPMHUD::DrawSetup(AGPMGameMode* GM, float W, float H)
{
	DrawRect(FLinearColor(0.01f, 0.015f, 0.03f, 0.55f), 0, 0, W, H);
	UGameInstance* GI = GetGameInstance();
	UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
	if (!Career) return;
	UFont* Mid = GEngine->GetMediumFont();
	FGPMPick& P = Career->Pick;
	static const TCHAR* Steps[] = { TEXT("Identité"), TEXT("Niveau"), TEXT("Couleurs"), TEXT("Motoriste"), TEXT("Équipier"), TEXT("Sponsor") };
	const int32 St = FMath::Clamp(GM->SetupStep, 0, 5);
	DrawText(FString::Printf(TEXT("CRÉE TON ÉCURIE · %d/6"), St + 1), FLinearColor::White, 14 * Scale, 10 * Scale, Mid, Scale * 1.2f);
	for (int32 k = 0; k < 6; ++k)
	{
		const float X = 230 * Scale + k * 82 * Scale;
		DrawRect(k == St ? Acc : k < St ? FLinearColor(0.3f, 0.6f, 0.35f) : CardBg, X, 12 * Scale, 78 * Scale, 20 * Scale);
		DrawText(Steps[k], FLinearColor::White, X + 6 * Scale, 14 * Scale, Mid, Scale * 0.8f);
	}
	DrawText(FString::Printf(TEXT("Budget de départ : %.1f M€"), Career->PickBudget()), Career->PickBudget() >= 0 ? FLinearColor(0.6f, 1.f, 0.7f) : FLinearColor(1.f, 0.3f, 0.2f),
		W - 230 * Scale, 40 * Scale, Mid, Scale);
	const float Top = 64 * Scale, S = Scale * 0.9f;
	const float CW = (W - 28 * Scale);
	auto Card = [this](float X, float Y, float CWd, float CH, bool bSel) -> bool
	{
		DrawRect(bSel ? CardOn : CardBg, X, Y, CWd, CH);
		if (bSel) DrawRect(Gold, X, Y, CWd, 2 * Scale);
		return Clicked(X, Y, CWd, CH);
	};
	if (St == 0)
	{
		DrawText(TEXT("Nom de l'écurie"), Dim, 14 * Scale, Top, Mid, S);
		for (int32 k = 0; k < Career->TeamNames.Num(); ++k)
		{
			const float X = 14 * Scale + (k % 4) * (CW / 4), Y = Top + 20 * Scale + (k / 4) * 36 * Scale;
			if (Card(X, Y, CW / 4 - 6 * Scale, 30 * Scale, P.Name == k)) P.Name = k;
			DrawText(Career->TeamNames[k], FLinearColor::White, X + 8 * Scale, Y + 7 * Scale, Mid, S);
		}
		DrawText(TEXT("Ton nom de pilote"), Dim, 14 * Scale, Top + 100 * Scale, Mid, S);
		for (int32 k = 0; k < Career->PilotNames.Num(); ++k)
		{
			const float X = 14 * Scale + (k % 3) * (CW / 3), Y = Top + 120 * Scale + (k / 3) * 36 * Scale;
			if (Card(X, Y, CW / 3 - 6 * Scale, 30 * Scale, P.Pilot == k)) P.Pilot = k;
			DrawText(Career->PilotNames[k], FLinearColor::White, X + 8 * Scale, Y + 7 * Scale, Mid, S);
		}
	}
	else if (St == 1)
	{
		for (int32 k = 0; k < Career->Tiers.Num(); ++k)
		{
			const FGPMTierOpt& T = Career->Tiers[k];
			const float X = 14 * Scale + k * (CW / 3), Y = Top, CWd = CW / 3 - 8 * Scale;
			if (Card(X, Y, CWd, 220 * Scale, P.Tier == k)) { P.Tier = k; if (Career->Sponsors.IsValidIndex(P.Sponsor) && Career->Sponsors[P.Sponsor].Rep > T.Rep) P.Sponsor = 0; }
			DrawText(T.Name, Gold, X + 10 * Scale, Y + 8 * Scale, Mid, Scale * 1.1f);
			const float Hh = DrawWrap(T.Desc, X + 10 * Scale, Y + 34 * Scale, CWd - 20 * Scale, FLinearColor::White, S);
			DrawText(FString::Printf(TEXT("Budget %.0f M€"), T.Budget), FLinearColor(0.6f, 1.f, 0.7f), X + 10 * Scale, Y + 44 * Scale + Hh, Mid, S);
			DrawText(FString::Printf(TEXT("Voiture %.1f %% · réputation %d · moral %d"), T.Perf * 100.f, T.Rep, T.Moral), Dim, X + 10 * Scale, Y + 64 * Scale + Hh, Mid, S * 0.95f);
		}
	}
	else if (St == 2)
	{
		for (int32 Row = 0; Row < 2; ++Row)
		{
			DrawText(Row == 0 ? TEXT("Couleur principale") : TEXT("Couleur secondaire"), Dim, 14 * Scale, Top + Row * 96 * Scale, Mid, S);
			for (int32 k = 0; k < Career->Colors.Num(); ++k)
			{
				const float Sz = FMath::Min(70 * Scale, CW / 10 - 6 * Scale);
				const float X = 14 * Scale + k * (Sz + 6 * Scale), Y = Top + 20 * Scale + Row * 96 * Scale;
				int32& Sel = Row == 0 ? P.Col1 : P.Col2;
				if (Sel == k) DrawRect(Gold, X - 3 * Scale, Y - 3 * Scale, Sz + 6 * Scale, Sz + 6 * Scale);
				DrawRect(Career->Colors[k].Color, X, Y, Sz, Sz * 0.8f);
				if (Clicked(X, Y, Sz, Sz * 0.8f))
				{
					Sel = k;
					// aperçu : la monoplace prend les couleurs tout de suite
					if (AGPMCar* C = Cast<AGPMCar>(GetOwningPawn()))
						if (Career->Colors.IsValidIndex(P.Col1) && Career->Colors.IsValidIndex(P.Col2)) C->SetLivery(Career->Colors[P.Col1].Color, Career->Colors[P.Col2].Color);
				}
				DrawText(Career->Colors[k].Name, Dim, X, Y + Sz * 0.8f + 2 * Scale, Mid, Scale * 0.6f);
			}
		}
		if (P.Col1 == P.Col2) DrawText(TEXT("Choisis deux couleurs différentes."), FLinearColor(1.f, 0.4f, 0.3f), 14 * Scale, Top + 200 * Scale, Mid, S);
	}
	else if (St == 3)
	{
		for (int32 k = 0; k < Career->Engines.Num(); ++k)
		{
			const FGPMEngineOpt& E = Career->Engines[k];
			const float X = 14 * Scale + k * (CW / 5), Y = Top, CWd = CW / 5 - 8 * Scale;
			if (Card(X, Y, CWd, 200 * Scale, P.Engine == k)) P.Engine = k;
			DrawText(E.Name, Gold, X + 8 * Scale, Y + 8 * Scale, Mid, S * 1.1f);
			DrawText(FString::Printf(TEXT("Puissance %d"), E.Perf), FLinearColor::White, X + 8 * Scale, Y + 40 * Scale, Mid, S);
			DrawText(FString::Printf(TEXT("Fiabilité %d"), E.Fiab), FLinearColor::White, X + 8 * Scale, Y + 62 * Scale, Mid, S);
			DrawText(FString::Printf(TEXT("Conso %d %%"), E.Conso), FLinearColor::White, X + 8 * Scale, Y + 84 * Scale, Mid, S);
			DrawText(FString::Printf(TEXT("%.0f M€ / saison"), E.Price), FLinearColor(1.f, 0.75f, 0.4f), X + 8 * Scale, Y + 116 * Scale, Mid, S);
		}
	}
	else if (St == 4)
	{
		for (int32 k = 0; k < Career->Market.Num(); ++k)
		{
			const FGPMMarketDriver& D = Career->Market[k];
			const float X = 14 * Scale + (k % 4) * (CW / 4), Y = Top + (k / 4) * 120 * Scale, CWd = CW / 4 - 8 * Scale;
			if (Card(X, Y, CWd, 112 * Scale, P.Mate == k)) P.Mate = k;
			DrawText(D.Name, Gold, X + 8 * Scale, Y + 6 * Scale, Mid, S * 1.05f);
			DrawText(FString::Printf(TEXT("%s · %d ans"), *D.From, D.Age), Dim, X + 8 * Scale, Y + 28 * Scale, Mid, S * 0.85f);
			DrawText(FString::Printf(TEXT("Note %.0f · vit. %.0f · pot. %.0f"), D.Rating(), D.Spd, D.Pot), FLinearColor::White, X + 8 * Scale, Y + 50 * Scale, Mid, S * 0.9f);
			const float Sal = UGPMCareerSubsystem::Salary(D);
			DrawText(Sal < 0 ? FString::Printf(TEXT("apporte %.1f M€"), -Sal) : FString::Printf(TEXT("%.1f M€ / saison"), Sal), FLinearColor(1.f, 0.75f, 0.4f), X + 8 * Scale, Y + 78 * Scale, Mid, S);
		}
	}
	else
	{
		const int32 TierRep = Career->Tiers.IsValidIndex(P.Tier) ? Career->Tiers[P.Tier].Rep : 50;
		for (int32 k = 0; k < Career->Sponsors.Num(); ++k)
		{
			const FGPMSponsorOpt& Sp = Career->Sponsors[k];
			const bool bOk = Sp.Rep <= TierRep;
			const float X = 14 * Scale + k * (CW / 5), Y = Top, CWd = CW / 5 - 8 * Scale;
			if (Card(X, Y, CWd, 210 * Scale, P.Sponsor == k) && bOk) P.Sponsor = k;
			DrawText(Sp.Name, bOk ? Gold : Dim, X + 8 * Scale, Y + 8 * Scale, Mid, S);
			DrawText(Sp.Sector, Dim, X + 8 * Scale, Y + 30 * Scale, Mid, S * 0.85f);
			DrawText(FString::Printf(TEXT("%.0f M€ / saison"), Sp.Year), FLinearColor::White, X + 8 * Scale, Y + 56 * Scale, Mid, S);
			DrawText(FString::Printf(TEXT("Signature +%.1f M€"), Sp.Sign), FLinearColor(0.6f, 1.f, 0.7f), X + 8 * Scale, Y + 78 * Scale, Mid, S);
			DrawWrap(FString::Printf(TEXT("Bonus %.1f M€ par GP si tu %s"), Sp.Bonus, GoalText(Sp.Goal)), X + 8 * Scale, Y + 102 * Scale, CWd - 14 * Scale, Dim, S * 0.85f);
			if (!bOk) DrawText(FString::Printf(TEXT("Réputation %d requise"), Sp.Rep), FLinearColor(1.f, 0.4f, 0.3f), X + 8 * Scale, Y + 178 * Scale, Mid, S * 0.85f);
		}
	}
	// navigation
	const float BY = H - 44 * Scale, BW = 170 * Scale, BH = 34 * Scale;
	if (Btn(St == 0 ? TEXT("ACCUEIL") : TEXT("RETOUR"), 14 * Scale, BY, BW, BH, FLinearColor(0.3f, 0.32f, 0.38f)))
	{
		if (St == 0) GM->Phase = AGPMGameMode::EPhase::Home; else --GM->SetupStep;
		return;
	}
	FString Why;
	const bool bValid = St == 2 ? P.Col1 != P.Col2 : St < 5 ? true : Career->PickValid(Why);
	if (!Why.IsEmpty()) DrawText(Why, FLinearColor(1.f, 0.4f, 0.3f), 200 * Scale, BY + 8 * Scale, Mid, S);
	if (Btn(St < 5 ? TEXT("SUIVANT") : TEXT("C'EST PARTI !"), W - BW - 14 * Scale, BY, BW, BH, Acc, bValid))
	{
		if (St < 5) ++GM->SetupStep; else GM->FinishSetup();
	}
}

// ---------- course solo ----------
void AGPMHUD::DrawSolo(AGPMGameMode* GM, float W, float H)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0, 0, W, H);
	UGameInstance* GI = GetGameInstance();
	UGPMDataSubsystem* Data = GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
	if (!Data) return;
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const TArray<FString> Ids = Data->GetCircuitIds();
	if (Ids.Num() == 0) return;
	GM->SoloIdx = (GM->SoloIdx % Ids.Num() + Ids.Num()) % Ids.Num();
	const FGPMTrackData* T = Data->GetTrack(Ids[GM->SoloIdx]);
	DrawCentered(TEXT("COURSE SOLO"), FLinearColor::White, W, 16 * Scale, Big, Scale * 1.3f);
	DrawCentered(TEXT("Sans effet sur le budget, le garage ni la presse"), Dim, W, 52 * Scale, Mid, Scale * 0.9f);
	const float BH = 40 * Scale;
	if (Btn(TEXT("<"), W / 2 - 260 * Scale, 90 * Scale, 50 * Scale, BH, FLinearColor(0.3f, 0.32f, 0.38f))) --GM->SoloIdx;
	if (Btn(TEXT(">"), W / 2 + 210 * Scale, 90 * Scale, 50 * Scale, BH, FLinearColor(0.3f, 0.32f, 0.38f))) ++GM->SoloIdx;
	DrawCentered(T ? T->Name : Ids[GM->SoloIdx], Gold, W, 96 * Scale, Big, Scale * 1.1f);
	if (T) DrawCentered(FString::Printf(TEXT("Circuit %d / %d · %.2f km · largeur %.0f m"), GM->SoloIdx + 1, Ids.Num(), T->WX.Num() * 2.f / 1000.f, T->Width), Dim, W, 134 * Scale, Mid, Scale * 0.9f);
	static const int32 LapOpts[] = { 3, 5, 10, 20 };
	DrawText(TEXT("Tours"), Dim, W / 2 - 260 * Scale, 172 * Scale, Mid, Scale);
	for (int32 k = 0; k < 4; ++k)
		if (Btn(FString::FromInt(LapOpts[k]), W / 2 - 170 * Scale + k * 90 * Scale, 166 * Scale, 80 * Scale, 32 * Scale, GM->SoloLaps == LapOpts[k] ? Acc : FLinearColor(0.3f, 0.32f, 0.38f))) GM->SoloLaps = LapOpts[k];
	DrawText(TEXT("Grille"), Dim, W / 2 - 260 * Scale, 222 * Scale, Mid, Scale);
	if (Btn(TEXT("Qualifications jouées"), W / 2 - 170 * Scale, 216 * Scale, 170 * Scale, 32 * Scale, GM->bSoloQuali ? Acc : FLinearColor(0.3f, 0.32f, 0.38f))) GM->bSoloQuali = true;
	if (Btn(TEXT("Milieu de grille"), W / 2 + 10 * Scale, 216 * Scale, 170 * Scale, 32 * Scale, !GM->bSoloQuali ? Acc : FLinearColor(0.3f, 0.32f, 0.38f))) GM->bSoloQuali = false;
	if (Btn(TEXT("RETOUR"), 14 * Scale, H - 48 * Scale, 160 * Scale, 36 * Scale, FLinearColor(0.3f, 0.32f, 0.38f))) { GM->Phase = AGPMGameMode::EPhase::Home; return; }
	if (Btn(TEXT("DÉPART"), W - 174 * Scale, H - 48 * Scale, 160 * Scale, 36 * Scale, Acc)) GM->StartSolo();
}

// ---------- réglages ----------
void AGPMHUD::DrawSettings(AGPMGameMode* GM, float W, float H)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 0, 0, W, H);
	UFont* Big = GEngine->GetLargeFont();
	DrawCentered(TEXT("RÉGLAGES"), FLinearColor::White, W, 16 * Scale, Big, Scale * 1.3f);
	struct FOpt { const TCHAR* Label; bool* Val; };
	FOpt Opts[] = { { TEXT("ABS (freinage sans blocage)"), &GM->bSetABS }, { TEXT("Antipatinage"), &GM->bSetTC }, { TEXT("Aide à la stabilité"), &GM->bSetStab },
		{ TEXT("Caméra cockpit au départ"), &GM->bSetCockpit }, { TEXT("Boutons tactiles à l'écran"), &bShowTouch } };
	const float BW = 380 * Scale, BH = 34 * Scale, X = (W - BW) / 2;
	for (int32 k = 0; k < 5; ++k)
		if (Btn(FString::Printf(TEXT("%s : %s"), Opts[k].Label, *Opts[k].Val ? TEXT("OUI") : TEXT("NON")), X, 64 * Scale + k * (BH + 8 * Scale), BW, BH,
			*Opts[k].Val ? FLinearColor(0.2f, 0.55f, 0.3f) : FLinearColor(0.3f, 0.32f, 0.38f)))
		{
			*Opts[k].Val = !*Opts[k].Val;
			GM->SaveSettings();
		}
	if (Btn(TEXT("RETOUR"), 14 * Scale, H - 48 * Scale, 160 * Scale, 36 * Scale, FLinearColor(0.3f, 0.32f, 0.38f))) GM->Phase = AGPMGameMode::EPhase::Home;
}

// ---------- fenêtres à choix : presse, flash info, histoires, négociations ----------
void AGPMHUD::DrawDialog(AGPMGameMode* GM, float W, float H)
{
	DrawRect(MenuBg, 0, 0, W, H);
	UGameInstance* GI = GetGameInstance();
	UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
	FGPMDialog* Dl = Career ? Career->Current() : nullptr;
	if (!Dl) { GM->EndDialogs(); return; }
	UFont* Mid = GEngine->GetMediumFont();
	const float CW = FMath::Min(W - 28 * Scale, 760 * Scale), X = (W - CW) / 2;
	float Y = 12 * Scale;
	DrawRect(Dl->Color, X, Y, CW, 24 * Scale);
	DrawText(Dl->Kicker, FLinearColor::White, X + 10 * Scale, Y + 4 * Scale, Mid, Scale * 0.85f);
	Y += 30 * Scale;
	if (UGPMSaveGame* G = Career->Save())
		DrawText(FString::Printf(TEXT("%s · Moral %d · Budget %.1f M€ · Réputation %d · Direction %d %%"), *G->TeamName, G->Moral, G->Budget, G->Rep, G->Conf), FLinearColor(0.6f, 0.9f, 1.f),
			X, Y, Mid, Scale * 0.8f);
	Y += 20 * Scale;
	if (!Dl->Title.IsEmpty()) { Y += DrawWrap(Dl->Title, X, Y, CW, Gold, Scale * 1.05f); Y += 4 * Scale; }
	Y += DrawWrap(Dl->Text, X, Y, CW, FLinearColor::White, Scale) + 10 * Scale;
	APlayerController* PC = GetOwningPlayerController();
	if (Dl->Chosen < 0)
	{
		const float BH = 30 * Scale;
		for (int32 k = 0; k < Dl->Choices.Num(); ++k)
		{
			const FGPMChoice& C = Dl->Choices[k];
			const float By = Y + k * (BH + 6 * Scale);
			DrawRect(C.bOff ? FLinearColor(1, 1, 1, 0.04f) : FLinearColor(1, 1, 1, 0.12f), X, By, CW, BH);
			DrawRect(Dl->Color, X, By, 4 * Scale, BH);
			DrawText(FString::Printf(TEXT("%d. %s"), k + 1, *C.Text), C.bOff ? Dim : FLinearColor::White, X + 12 * Scale, By + 6 * Scale, Mid, Scale * 0.9f);
			static const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
			if (Clicked(X, By, CW, BH) || (k < 5 && PC && PC->WasInputKeyJustPressed(Keys[k]))) { Career->Choose(k); return; }
		}
	}
	else
	{
		if (Dl->Choices.IsValidIndex(Dl->Chosen) && Dl->Kind != FGPMDialog::EKind::Nego)
		{
			DrawRect(CardOn, X, Y, CW, 30 * Scale);
			DrawText(Dl->Choices[Dl->Chosen].Text, Gold, X + 12 * Scale, Y + 6 * Scale, Mid, Scale * 0.9f);
			Y += 38 * Scale;
		}
		if (!Dl->Result.IsEmpty()) Y += DrawWrap(Dl->Result, X, Y, CW, Dl->bBad ? FLinearColor(1.f, 0.45f, 0.35f) : FLinearColor::White, Scale) + 6 * Scale;
		if (!Dl->FxText.IsEmpty()) Y += DrawWrap(Dl->FxText, X, Y, CW, FLinearColor(0.6f, 1.f, 0.7f), Scale * 0.9f);
		const float BW = 200 * Scale;
		if (Btn(TEXT("CONTINUER"), (W - BW) / 2, H - 50 * Scale, BW, 36 * Scale, Acc) || (PC && PC->WasInputKeyJustPressed(EKeys::Enter)))
		{
			Career->Next();
			if (!Career->Current()) GM->EndDialogs();
		}
	}
}

// ---------- mercato ----------
void AGPMHUD::DrawMercato(AGPMGameMode* GM, float W, float H)
{
	DrawRect(MenuBg, 0, 0, W, H);
	UGameInstance* GI = GetGameInstance();
	UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
	const UGPMSaveGame* G = Career ? Career->Save() : nullptr;
	if (!G) return;
	UFont* Mid = GEngine->GetMediumFont();
	DrawText(TEXT("MERCATO DES PILOTES"), FLinearColor::White, 14 * Scale, 8 * Scale, Mid, Scale * 1.2f);
	DrawText(FString::Printf(TEXT("Budget %.1f M€ · indemnités de transfert ×2,5 pendant la saison"), G->Budget), FLinearColor(0.6f, 0.9f, 1.f), 14 * Scale, 32 * Scale, Mid, Scale * 0.85f);
	DrawText(FString::Printf(TEXT("Ton équipier : %s · vitesse %.0f · %.1f M€ / saison · contrat jusqu'en %d · statut %s"), *G->MateName, G->MateSpd, G->MateSal,
		G->MateSince + G->MateYears - 1, G->MateRole == TEXT("n1") ? TEXT("N°1") : G->MateRole == TEXT("n2") ? TEXT("N°2") : TEXT("Égalité")), Gold, 14 * Scale, 50 * Scale, Mid, Scale * 0.85f);
	const TArray<FGPMMarketDriver> List = Career->MarketList();
	const int32 PerPage = 9, Pages = FMath::Max(1, (List.Num() + PerPage - 1) / PerPage);
	GM->MercatoPage = FMath::Clamp(GM->MercatoPage, 0, Pages - 1);
	const float Row = 28 * Scale, X0 = 14 * Scale, RW = W - 28 * Scale;
	float Y = 74 * Scale;
	for (int32 k = GM->MercatoPage * PerPage; k < List.Num() && k < (GM->MercatoPage + 1) * PerPage; ++k, Y += Row + 2 * Scale)
	{
		const FGPMMarketDriver& D = List[k];
		DrawRect(CardBg, X0, Y, RW, Row);
		DrawText(D.Name, FLinearColor::White, X0 + 8 * Scale, Y + 5 * Scale, Mid, Scale * 0.9f);
		DrawText(D.Seat >= 0 ? D.Team : D.From, Dim, X0 + 150 * Scale, Y + 5 * Scale, Mid, Scale * 0.85f);
		DrawText(FString::Printf(TEXT("%d ans · note %.0f · vit. %.0f · pot. %.0f"), D.Age, D.Rating(), D.Spd, D.Pot), FLinearColor::White, X0 + 330 * Scale, Y + 5 * Scale, Mid, Scale * 0.85f);
		DrawText(Career->Status(D), FLinearColor(1.f, 0.75f, 0.4f), X0 + RW - 170 * Scale, Y + 5 * Scale, Mid, Scale * 0.85f);
		if (Clicked(X0, Y, RW, Row)) { Career->OpenNego(D); GM->bNegoFromMercato = true; GM->Phase = AGPMGameMode::EPhase::Press; return; }
	}
	const float BY = H - 44 * Scale;
	if (Btn(TEXT("RETOUR"), 14 * Scale, BY, 150 * Scale, 34 * Scale, FLinearColor(0.3f, 0.32f, 0.38f))) { GM->bNegoFromMercato = false; GM->Phase = AGPMGameMode::EPhase::Season; return; }
	DrawCentered(FString::Printf(TEXT("page %d / %d · touche un pilote pour négocier avec son agent"), GM->MercatoPage + 1, Pages), Dim, W, BY + 8 * Scale, Mid, Scale * 0.85f);
	if (Btn(TEXT("<"), W - 130 * Scale, BY, 54 * Scale, 34 * Scale, FLinearColor(0.3f, 0.32f, 0.38f), GM->MercatoPage > 0)) --GM->MercatoPage;
	if (Btn(TEXT(">"), W - 68 * Scale, BY, 54 * Scale, 34 * Scale, FLinearColor(0.3f, 0.32f, 0.38f), GM->MercatoPage < Pages - 1)) ++GM->MercatoPage;
}

// ---------- licencié ----------
void AGPMHUD::DrawFired(AGPMGameMode* GM, float W, float H)
{
	DrawRect(FLinearColor(0.08f, 0.f, 0.f, 0.9f), 0, 0, W, H);
	UGameInstance* GI = GetGameInstance();
	UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	DrawCentered(TEXT("LICENCIÉ !"), FLinearColor(1.f, 0.3f, 0.25f), W, 40 * Scale, Big, Scale * 2.f);
	const FString Team = Career && Career->Save() ? Career->Save()->TeamName : FString(TEXT("l'écurie"));
	DrawCentered(FString::Printf(TEXT("La direction de %s a perdu confiance et met fin à ton contrat."), *Team), FLinearColor::White, W, 110 * Scale, Mid, Scale);
	float Y = 140 * Scale;
	for (const FString& L : GM->LastLines) { DrawCentered(L, Dim, W, Y, Mid, Scale * 0.9f); Y += 20 * Scale; }
	if (Btn(TEXT("NOUVELLE CARRIÈRE"), (W - 240 * Scale) / 2, H - 70 * Scale, 240 * Scale, 40 * Scale, Acc)) GM->NewCareer();
}

// ---------- cinématiques : bandes noires, titres, confettis ----------
void AGPMHUD::DrawCine(AGPMGameMode* GM, float W, float H)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	const float Bar = H * 0.11f;
	DrawRect(FLinearColor::Black, 0, 0, W, Bar);
	DrawRect(FLinearColor::Black, 0, H - Bar, W, Bar);
	const float Fade = FMath::Clamp(GM->CineT * 1.5f, 0.f, 1.f);
	if (GM->Cine == AGPMGameMode::ECine::Intro)
	{
		static const TCHAR* Shots[] = { TEXT("VUE DU DRONE"), TEXT("LA GRILLE"), TEXT("EN PISTE") };
		DrawText(Shots[GM->CineShot()], FLinearColor(1, 1, 1, 0.6f), 16 * Scale, Bar + 10 * Scale, Mid, Scale * 0.9f);
	}
	if (GM->Cine == AGPMGameMode::ECine::Podium && GM->CineTitle.StartsWith(TEXT("VICTOIRE")))
	{
		// confettis : 90 papiers qui tombent en tournoyant, sans coût de rendu 3D
		static const FLinearColor Conf[] = { FLinearColor(1.f, 0.85f, 0.2f), FLinearColor(0.9f, 0.2f, 0.2f), FLinearColor(0.2f, 0.6f, 1.f), FLinearColor::White, FLinearColor(0.3f, 0.9f, 0.4f) };
		for (int32 k = 0; k < 90; ++k)
		{
			const float Fx = FMath::Frac(FMath::Sin(k * 12.9898f) * 43758.5453f), Sp = 0.5f + FMath::Frac(k * 0.371f);
			const float X = FMath::Fmod(Fx * W + FMath::Sin(GM->CineT * 2.f + k) * 30.f * Scale + W, W);
			const float Y = FMath::Fmod(GM->CineT * 140.f * Scale * Sp + k * 53.f * Scale, H);
			const float Sz = (4.f + 4.f * FMath::Abs(FMath::Sin(GM->CineT * 5.f + k))) * Scale;
			DrawRect(Conf[k % 5], X, Y, Sz, 7.f * Scale);
		}
	}
	const FString Title = GM->Cine == AGPMGameMode::ECine::Welcome ? FString(TEXT("BIENVENUE AU QG")) : GM->CineTitle;
	DrawCentered(Title, FLinearColor(1, 1, 1, Fade), W, H - Bar - 74 * Scale, Big, Scale * 1.6f);
	DrawCentered(GM->CineSub, FLinearColor(1.f, 0.85f, 0.2f, Fade), W, H - Bar - 30 * Scale, Mid, Scale * 1.05f);
	DrawText(TEXT("Touche l'écran pour passer"), FLinearColor(1, 1, 1, 0.5f), W - 200 * Scale, H - Bar + 10 * Scale, Mid, Scale * 0.8f);
	if (bTap)
	{
		const bool bWelcome = GM->Cine == AGPMGameMode::ECine::Welcome;
		GM->EndCine();
		if (bWelcome) GM->StartCine(AGPMGameMode::ECine::Menu);
		bTap = false;
		bWasDown = true;
	}
}

// ---------- sacre de fin de saison ----------
void AGPMHUD::DrawChamp(AGPMGameMode* GM, float W, float H)
{
	UGameInstance* GI = GetGameInstance();
	UGPMSeasonSubsystem* Season = GI ? GI->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
	UGPMCareerSubsystem* Career = GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
	if (!Season) return;
	const TArray<FGPMStanding> Drv = Season->Drivers(), Tms = Season->Teams();
	const bool bDrv = Drv.Num() && Drv[0].bPlayer, bTeam = Tms.Num() && Tms[0].bPlayer;
	if (!bDrv && !bTeam) { GM->bChampShown = true; return; }
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f), 0, 0, W, H);
	const float CW = 520 * Scale, CH = 250 * Scale, X = (W - CW) / 2, Y = (H - CH) / 2;
	DrawRect(FLinearColor(0.12f, 0.1f, 0.02f, 1.f), X, Y, CW, CH);
	DrawRect(Gold, X, Y, CW, 4 * Scale);
	DrawRect(Gold, X, Y + CH - 4 * Scale, CW, 4 * Scale);
	DrawCentered(FString::Printf(TEXT("SAISON %d"), Season->Year()), Dim, W, Y + 18 * Scale, Mid, Scale);
	DrawCentered(bDrv ? TEXT("CHAMPION DU MONDE !") : TEXT("CHAMPIONS DES ÉCURIES !"), Gold, W, Y + 48 * Scale, Big, Scale * 1.7f);
	if (bDrv) DrawCentered(FString::Printf(TEXT("%s · %d points"), *Nm(Drv[0].Name), Drv[0].Pts), FLinearColor::White, W, Y + 116 * Scale, Mid, Scale * 1.1f);
	if (bTeam) DrawCentered(FString::Printf(TEXT("%s, champion des écuries · %d points"), *Tms[0].Name, Tms[0].Pts), FLinearColor::White, W, Y + 146 * Scale, Mid, Scale);
	if (Career && Career->Save()) DrawCentered(FString::Printf(TEXT("Réputation %d · budget %.1f M€"), Career->Save()->Rep, Career->Save()->Budget), Dim, W, Y + 180 * Scale, Mid, Scale * 0.9f);
	DrawCentered(TEXT("Touche l'écran pour continuer"), Dim, W, Y + CH - 30 * Scale, Mid, Scale * 0.85f);
	if (bTap) { GM->bChampShown = true; bTap = false; bWasDown = true; }
}
