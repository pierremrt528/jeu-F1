// Petits éléments dessinés sur le canvas : boutons (menu pause), texte sur plusieurs lignes, titres des cinématiques.
// Les écrans de gestion sont en Slate (GPMUI.cpp, GPMUIScreens.cpp).
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
	const FLinearColor Grey(0.25f, 0.27f, 0.3f, 1.f);
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

// ---------- cinématiques : bandes noires, titres, confettis ----------
void AGPMHUD::DrawCine(AGPMGameMode* GM, float W, float H)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Mid = GEngine->GetMediumFont();
	// bandes noires qui glissent en place au début, fondu au noir entre deux plans
	const float Bar = H * 0.11f * FMath::Clamp(0.25f + GM->CineT * 2.5f, 0.f, 1.f);
	const float Cut = GM->CineCut();
	if (Cut > 0.f) DrawRect(FLinearColor(0.f, 0.f, 0.f, Cut), 0, 0, W, H);
	DrawRect(FLinearColor::Black, 0, 0, W, Bar);
	DrawRect(FLinearColor::Black, 0, H - Bar, W, Bar);
	const float Fade = FMath::Clamp(GM->CineT * 1.5f, 0.f, 1.f);
	using EC = AGPMGameMode::ECine;
	if (GM->Cine == EC::Intro)
	{
		static const TCHAR* Shots[] = { TEXT("VUE DU DRONE"), TEXT("LA GRILLE"), TEXT("CONCENTRATION"), TEXT("PRÊT AU DÉPART"), TEXT("CAMÉRA DU DÉPART"), TEXT("EN PISTE") };
		DrawText(Shots[FMath::Clamp(GM->CineShot(), 0, 5)], FLinearColor(1, 1, 1, 0.6f * (1.f - Cut)), 16 * Scale, Bar + 10 * Scale, Mid, Scale * 0.9f);
	}
	const bool bWin = GM->Cine == EC::Podium && GM->CineTitle.StartsWith(TEXT("VICTOIRE"));
	const bool bCrown = GM->Cine == EC::Crown;
	if (bWin || bCrown)
	{
		// confettis : papiers qui tombent en tournoyant, sans coût de rendu 3D ; tout en or pour le sacre
		static const FLinearColor Conf[] = { FLinearColor(1.f, 0.85f, 0.2f), FLinearColor(0.9f, 0.2f, 0.2f), FLinearColor(0.2f, 0.6f, 1.f), FLinearColor::White, FLinearColor(0.3f, 0.9f, 0.4f) };
		static const FLinearColor Gold[] = { FLinearColor(1.f, 0.85f, 0.2f), FLinearColor(1.f, 0.72f, 0.12f), FLinearColor(1.f, 0.95f, 0.6f), FLinearColor::White, FLinearColor(0.85f, 0.6f, 0.1f) };
		const int32 Count = bCrown ? 140 : 90;
		for (int32 k = 0; k < Count; ++k)
		{
			const float Fx = FMath::Frac(FMath::Sin(k * 12.9898f) * 43758.5453f), Sp = 0.5f + FMath::Frac(k * 0.371f);
			const float X = FMath::Fmod(Fx * W + FMath::Sin(GM->CineT * 2.f + k) * 30.f * Scale + W, W);
			const float Y = FMath::Fmod(GM->CineT * 140.f * Scale * Sp + k * 53.f * Scale, H);
			const float Spin = FMath::Abs(FMath::Sin(GM->CineT * 5.f + k));
			DrawRect((bCrown ? Gold : Conf)[k % 5] * FLinearColor(1.f, 1.f, 1.f, 0.6f + 0.4f * Spin), X, Y, (3.f + 5.f * Spin) * Scale, 7.f * Scale);
		}
		// flashs des photographes : petits éclairs blancs au bord de l'image, à intervalles irréguliers
		for (int32 k = 0; k < (bCrown ? 7 : 4); ++k)
		{
			const float Period = 0.9f + FMath::Frac(k * 0.618f) * 1.3f;
			const float Ph = FMath::Fmod(GM->CineT + k * 0.37f, Period);
			if (Ph > 0.12f) continue;
			const int32 Burst = FMath::FloorToInt((GM->CineT + k * 0.37f) / Period);
			const float Rx = FMath::Frac(FMath::Sin((k * 7 + Burst) * 78.233f) * 43758.5453f);
			const float Ry = FMath::Frac(FMath::Sin((k * 3 + Burst) * 39.425f) * 24634.6345f);
			const float X = Rx < 0.5f ? Rx * 0.5f * W : W - (Rx - 0.5f) * 0.5f * W;
			const float Y = Bar + Ry * (H - 2.f * Bar) * 0.6f;
			// étoile : deux traits fins qui se croisent et un point très lumineux au centre
			const float A = 1.f - Ph / 0.12f, R = (14.f + 30.f * A) * Scale, T = 1.5f * Scale;
			DrawRect(FLinearColor(1.f, 0.98f, 0.9f, 0.7f * A), X - R, Y - T * 0.5f, 2.f * R, T);
			DrawRect(FLinearColor(1.f, 0.98f, 0.9f, 0.7f * A), X - T * 0.5f, Y - R * 0.6f, T, 1.2f * R);
			DrawRect(FLinearColor(1.f, 1.f, 1.f, A), X - 2.5f * Scale, Y - 2.5f * Scale, 5.f * Scale, 5.f * Scale);
		}
	}
	// photo de la signature : flash blanc qui s'éteint en 0,4 s
	if (GM->Cine == EC::Sign && GM->CineT >= 2.65f && GM->CineT < 3.05f)
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.9f * (1.f - (GM->CineT - 2.65f) / 0.4f)), 0, 0, W, H);
	const FString Title = GM->Cine == AGPMGameMode::ECine::Welcome && !GM->bReveal ? FString(TEXT("BIENVENUE AU QG")) : GM->CineTitle;
	// survol : le titre du circuit sur les deux premiers plans seulement (ta voiture reste dégagée ensuite) ;
	// ombre portée sous le texte pour le lire sur un fond clair
	const float TFade = GM->Cine == EC::Intro ? Fade * (1.f - FMath::Clamp((GM->CineT - 6.4f) / 0.4f, 0.f, 1.f)) : Fade;
	if (TFade > 0.f)
	{
		const float Sh = 2.f * Scale;
		DrawCentered(Title, FLinearColor(0.f, 0.f, 0.f, 0.6f * TFade), W + 2.f * Sh, H - Bar - 74 * Scale + Sh, Big, Scale * 1.6f);
		DrawCentered(Title, bCrown ? FLinearColor(1.f, 0.86f, 0.3f, TFade) : FLinearColor(1, 1, 1, TFade), W, H - Bar - 74 * Scale, Big, Scale * 1.6f);
		DrawCentered(GM->CineSub, FLinearColor(0.f, 0.f, 0.f, 0.6f * TFade), W + 2.f * Sh, H - Bar - 30 * Scale + Sh, Mid, Scale * 1.05f);
		DrawCentered(GM->CineSub, FLinearColor(1.f, 0.85f, 0.2f, TFade), W, H - Bar - 30 * Scale, Mid, Scale * 1.05f);
	}
	DrawText(TEXT("Touche l'écran ou Entrée pour passer"), FLinearColor(1, 1, 1, 0.5f), W - 290 * Scale, H - Bar + 10 * Scale, Mid, Scale * 0.8f);
	if (bTap)
	{
		const bool bRoom = GM->RoomCine();
		GM->EndCine();
		if (bRoom) GM->StartCine(AGPMGameMode::ECine::Menu);
		bTap = false;
		bWasDown = true;
	}
}
