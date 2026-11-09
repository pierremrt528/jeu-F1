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
