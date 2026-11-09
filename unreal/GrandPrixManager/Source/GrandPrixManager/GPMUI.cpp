// Écrans de gestion en Slate : socle (affichage, reconstruction, briques communes) et petits écrans
// (accueil, course solo, réglages, fenêtres à choix, licenciement). Les grands écrans sont dans GPMUIScreens.cpp.
#include "GPMUI.h"
#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
#include "GPMDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"

using namespace GPMUIColor;

namespace GPMUIStyle
{
	const FSlateBrush* Rounded(float Radius)
	{
		static FSlateRoundedBoxBrush R6(FLinearColor::White, 6.f);
		static FSlateRoundedBoxBrush R12(FLinearColor::White, 12.f);
		static FSlateRoundedBoxBrush R40(FLinearColor::White, 40.f);
		static FSlateColorBrush Flat(FLinearColor::White);
		if (Radius <= 0.f) return &Flat;
		return Radius < 9.f ? &R6 : Radius < 20.f ? &R12 : &R40;
	}

	FLinearColor Lift(const FLinearColor& C, float K)
	{
		return FLinearColor(FMath::Clamp(C.R * K + (K > 1.f ? 0.05f : 0.f), 0.f, 1.f), FMath::Clamp(C.G * K + (K > 1.f ? 0.05f : 0.f), 0.f, 1.f),
			FMath::Clamp(C.B * K + (K > 1.f ? 0.05f : 0.f), 0.f, 1.f), C.A);
	}

	/** Style de bouton arrondi d'une couleur (gardé en cache : les styles doivent vivre plus longtemps que les boutons). */
	const FButtonStyle* ButtonOf(const FLinearColor& C, bool bOutline)
	{
		static TMap<uint32, TSharedPtr<FButtonStyle>> Cache;
		const uint32 Key = HashCombine(C.ToFColor(false).DWColor(), bOutline ? 7u : 3u);
		if (const TSharedPtr<FButtonStyle>* Found = Cache.Find(Key)) return Found->Get();
		TSharedPtr<FButtonStyle> St = MakeShared<FButtonStyle>();
		const FLinearColor Edge = bOutline ? FLinearColor(1.f, 0.8f, 0.22f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.f);
		St->SetNormal(FSlateRoundedBoxBrush(C, 10.f, Edge, bOutline ? 2.f : 0.f));
		St->SetHovered(FSlateRoundedBoxBrush(Lift(C, 1.25f), 10.f, bOutline ? Edge : FLinearColor(1.f, 1.f, 1.f, 0.55f), 2.f));
		St->SetPressed(FSlateRoundedBoxBrush(Lift(C, 0.8f), 10.f, Edge, bOutline ? 2.f : 0.f));
		St->SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.16f, 0.17f, 0.2f, 0.85f), 10.f));
		St->SetNormalPadding(FMargin(0.f));
		St->SetPressedPadding(FMargin(0.f));
		Cache.Add(Key, St);
		return St.Get();
	}
}

FGPMUI::FGPMUI(AGPMHUD* InHud) : Hud(InHud) {}

UGPMCareerSubsystem* FGPMUI::CareerSys() const
{
	const AGPMHUD* H = Hud.Get();
	UGameInstance* GI = H ? H->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UGPMCareerSubsystem>() : nullptr;
}

UGPMSeasonSubsystem* FGPMUI::SeasonSys() const
{
	const AGPMHUD* H = Hud.Get();
	UGameInstance* GI = H ? H->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UGPMSeasonSubsystem>() : nullptr;
}

UGPMDataSubsystem* FGPMUI::DataSys() const
{
	const AGPMHUD* H = Hud.Get();
	UGameInstance* GI = H ? H->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UGPMDataSubsystem>() : nullptr;
}

FString FGPMUI::Nm(const FString& Driver) const
{
	const UGPMCareerSubsystem* C = CareerSys();
	return C ? C->Display(Driver) : Driver;
}

FString FGPMUI::Clock(float S)
{
	if (S <= 0.f || S >= 999.f) return TEXT("--:--.---");
	const int32 Mn = FMath::FloorToInt(S / 60.f);
	return FString::Printf(TEXT("%d:%06.3f"), Mn, S - Mn * 60.f);
}

// ---------- affichage ----------

void FGPMUI::Shutdown()
{
	if (bAdded && Root.IsValid())
		if (AGPMHUD* H = Hud.Get())
			if (UWorld* W = H->GetWorld())
				if (UGameViewportClient* VP = W->GetGameViewport()) VP->RemoveViewportWidgetContent(Root.ToSharedRef());
	bAdded = false;
	Root.Reset();
}

uint32 FGPMUI::Signature(AGPMGameMode* GM) const
{
	uint32 H = HashCombine(GetTypeHash(int32(GM->Phase)), GetTypeHash(Version));
	H = HashCombine(H, GetTypeHash(GM->SetupStep * 100 + GM->MercatoPage));
	H = HashCombine(H, GetTypeHash(GM->SoloIdx * 100 + GM->SoloLaps + (GM->bSoloQuali ? 7 : 0) + GM->SoloWeather * 100000));
	H = HashCombine(H, GetTypeHash(SelTree * 100 + SelPart + (GM->bChampShown ? 50000 : 0)));
	H = HashCombine(H, GetTypeHash(GM->SetupK * 10 + (GM->bLongRace ? 1 : 0) + (GM->bGridReady ? 2 : 0) + GM->Plans.Num() * 1000));
	H = HashCombine(H, GetTypeHash((GM->bRainStart ? 1 : 0) + (GM->WxChangeAt >= 0.f ? 2 : 0)));
	for (const TPair<TCHAR, int32>& St : GM->Stints) H = HashCombine(H, GetTypeHash(int32(St.Key) * 1000 + St.Value));
	if (const UGPMCareerSubsystem* C = CareerSys())
	{
		H = HashCombine(H, GetTypeHash(C->Queue.Num()));
		if (C->Queue.Num()) H = HashCombine(H, GetTypeHash(C->Queue[0].Chosen * 31 + C->Queue[0].Choices.Num()));
	}
	return H | 1u;
}

void FGPMUI::HandleKeys(AGPMGameMode* GM)
{
	AGPMHUD* H = Hud.Get();
	APlayerController* PC = H ? H->GetOwningPlayerController() : nullptr;
	UGPMCareerSubsystem* C = CareerSys();
	using EP = AGPMGameMode::EPhase;
	// première image d'un écran : la touche qui vient de l'ouvrir ne doit pas aussi agir dessus
	const bool bFresh = KeyPhase != int32(GM->Phase);
	KeyPhase = int32(GM->Phase);
	if (bFresh) return;
	if (GM->Phase == EP::Tuning && PC)
	{
		const int32 Sel = GM->SetupK >= 0 ? GM->SetupK : GM->BestSetup();
		if (PC->WasInputKeyJustPressed(EKeys::Up)) GM->SetupK = FMath::Max(0, Sel - 1);
		else if (PC->WasInputKeyJustPressed(EKeys::Down)) GM->SetupK = FMath::Min(4, Sel + 1);
		else if (PC->WasInputKeyJustPressed(EKeys::Enter)) GM->PlayQuali(Sel);
		return;
	}
	if (GM->Phase == EP::Prerace && PC)
	{
		for (int32 k = 0; k < GM->Plans.Num() && k < 3; ++k)
			if (AGPMGameMode::DigitPressed(PC, k + 1)) GM->Stints = GM->Plans[k].Stints;
		if (PC->WasInputKeyJustPressed(EKeys::Enter)) GM->Go(false);
		return;
	}
	if (GM->Phase == EP::Press && C)
	{
		FGPMDialog* Dl = C->Current();
		if (!Dl) { GM->EndDialogs(); return; }
		if (!PC) return;
		if (Dl->Chosen < 0)
		{
			for (int32 k = 0; k < Dl->Choices.Num() && k < 5; ++k)
				if (AGPMGameMode::DigitPressed(PC, k + 1) && !Dl->Choices[k].bOff) { C->Choose(k); return; }
		}
		else if (PC->WasInputKeyJustPressed(EKeys::Enter))
		{
			C->Next();
			if (!C->Current()) GM->EndDialogs();
		}
	}
	else if (GM->Phase == EP::Season)
	{
		// sacre : seulement si le joueur ou son écurie est champion
		UGPMSeasonSubsystem* S = SeasonSys();
		if (S && S->IsOver() && !GM->bChampShown)
		{
			const TArray<FGPMStanding> Drv = S->Drivers(), Tms = S->Teams();
			if (!(Drv.Num() && Drv[0].bPlayer) && !(Tms.Num() && Tms[0].bPlayer)) GM->bChampShown = true;
		}
	}
	else if (GM->Phase == EP::Factory && PC)
	{
		const UGPMFactorySubsystem* F = H->GetGameInstance() ? H->GetGameInstance()->GetSubsystem<UGPMFactorySubsystem>() : nullptr;
		if (!F || F->Trees().Num() == 0) return;
		const int32 NT = F->Trees().Num();
		if (PC->WasInputKeyJustPressed(EKeys::Left)) { SelTree = (SelTree + NT - 1) % NT; SelPart = 0; }
		if (PC->WasInputKeyJustPressed(EKeys::Right)) { SelTree = (SelTree + 1) % NT; SelPart = 0; }
		SelTree = FMath::Clamp(SelTree, 0, NT - 1);
		const int32 NP = F->Trees()[SelTree].Parts.Num();
		if (PC->WasInputKeyJustPressed(EKeys::Up)) SelPart = FMath::Max(0, SelPart - 1);
		if (PC->WasInputKeyJustPressed(EKeys::Down)) SelPart = FMath::Min(NP - 1, SelPart + 1);
	}
}

bool FGPMUI::Update(AGPMGameMode* GM)
{
	AGPMHUD* H = Hud.Get();
	if (!H || !GM) return false;
	using EP = AGPMGameMode::EPhase;
	using EC = AGPMGameMode::ECine;
	const bool bCine = GM->Cine == EC::Intro || GM->Cine == EC::Podium || GM->Cine == EC::Welcome;
	const bool bWant = !bCine && (GM->InMenu() || GM->Phase == EP::QualiDone || GM->Phase == EP::Done);
	if (!bAdded)
	{
		UWorld* W = H->GetWorld();
		UGameViewportClient* VP = W ? W->GetGameViewport() : nullptr;
		if (!VP) return false;
		SAssignNew(Root, SBox).Visibility(EVisibility::Collapsed);
		VP->AddViewportWidgetContent(Root.ToSharedRef(), 10);
		bAdded = true;
	}
	if (!bWant)
	{
		KeyPhase = -1;
		if (Shown != 0) { Root->SetContent(SNullWidget::NullWidget); Root->SetVisibility(EVisibility::Collapsed); Shown = 0; }
		return false;
	}
	HandleKeys(GM);
	const uint32 Sig = Signature(GM);
	if (Sig != Shown)
	{
		Shown = Sig;
		// accent : couleurs de l'écurie du joueur
		if (const UGPMCareerSubsystem* C = CareerSys())
			if (const UGPMSaveGame* G = C->Save())
				if (C->IsCreated() && C->Colors.IsValidIndex(G->Col1) && C->Colors.IsValidIndex(G->Col2))
				{
					Acc = C->Colors[G->Col1].Color; Acc.A = 1.f;
					Acc2 = C->Colors[G->Col2].Color; Acc2.A = 1.f;
					if (Acc.GetLuminance() < 0.04f) Acc = GPMUIStyle::Lift(Acc, 1.6f);
				}
		Root->SetContent(Build(GM));
		Root->SetVisibility(EVisibility::Visible);
	}
	return true;
}

TSharedRef<SWidget> FGPMUI::Build(AGPMGameMode* GM)
{
	using EP = AGPMGameMode::EPhase;
	switch (GM->Phase)
	{
	case EP::Home: return HomeScreen(GM);
	case EP::Setup: return SetupScreen(GM);
	case EP::Solo: return SoloScreen(GM);
	case EP::Settings: return SettingsScreen(GM);
	case EP::Press: return DialogScreen(GM);
	case EP::Mercato: return MercatoScreen(GM);
	case EP::Fired: return FiredScreen(GM);
	case EP::Factory: return FactoryScreen(GM);
	case EP::Season: return SeasonScreen(GM);
	case EP::QualiDone: return QualiScreen(GM);
	case EP::Done: return ResultsScreen(GM);
	case EP::Tuning: return TuningScreen(GM);
	case EP::Prerace: return PreraceScreen(GM);
	default: return SNullWidget::NullWidget;
	}
}

// ---------- briques ----------

FLinearColor FGPMUI::InkOn(const FLinearColor& Bg)
{
	return Bg.GetLuminance() > 0.55f ? FLinearColor(0.04f, 0.05f, 0.08f, 1.f) : FLinearColor::White;
}

TSharedRef<SWidget> FGPMUI::Txt(const FString& S, int32 Size, const FLinearColor& Col, bool bBold, bool bWrap, bool bCenter) const
{
	return SNew(STextBlock)
		.Text(FText::FromString(S))
		.Font(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size))
		.ColorAndOpacity(FSlateColor(Col))
		.AutoWrapText(bWrap)
		.Justification(bCenter ? ETextJustify::Center : ETextJustify::Left);
}

TSharedRef<SWidget> FGPMUI::Card(const TSharedRef<SWidget>& Content, const FLinearColor& Bg, float Pad) const
{
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Rounded(12.f))
		.BorderBackgroundColor(FSlateColor(Bg))
		.Padding(FMargin(Pad))
		[
			Content
		];
}

TSharedRef<SWidget> FGPMUI::Button(const FString& Label, const FLinearColor& Col, TFunction<void()> OnClick, bool bEnabled, float MinW, float Height, int32 Size)
{
	TWeakPtr<FGPMUI> Self = AsShared();
	return SNew(SBox)
		.MinDesiredWidth(MinW)
		.HeightOverride(Height)
		[
			SNew(SButton)
			.ButtonStyle(GPMUIStyle::ButtonOf(Col, false))
			.IsFocusable(false)
			.IsEnabled(bEnabled)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(FMargin(18.f, 4.f))
			.OnClicked_Lambda([Self, OnClick]()
			{
				if (TSharedPtr<FGPMUI> Me = Self.Pin()) { if (OnClick) OnClick(); Me->Touch(); }
				return FReply::Handled();
			})
			[
				Txt(Label, Size, bEnabled ? InkOn(Col) : CFaint, true)
			]
		];
}

TSharedRef<SWidget> FGPMUI::Pick(const TSharedRef<SWidget>& Content, bool bSel, TFunction<void()> OnClick, bool bEnabled)
{
	TWeakPtr<FGPMUI> Self = AsShared();
	const FLinearColor Bg = bSel ? FLinearColor(Acc.R * 0.45f, Acc.G * 0.45f, Acc.B * 0.45f, 0.85f) : FLinearColor(0.09f, 0.1f, 0.14f, 0.9f);
	return SNew(SButton)
		.ButtonStyle(GPMUIStyle::ButtonOf(Bg, bSel))
		.IsFocusable(false)
		.IsEnabled(bEnabled)
		.ContentPadding(FMargin(14.f, 10.f))
		.OnClicked_Lambda([Self, OnClick]()
		{
			if (TSharedPtr<FGPMUI> Me = Self.Pin()) { if (OnClick) OnClick(); Me->Touch(); }
			return FReply::Handled();
		})
		[
			Content
		];
}

TSharedRef<SWidget> FGPMUI::Chip(const FString& Label, const FString& Value, const FLinearColor& Col) const
{
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Rounded(40.f))
		.BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f)))
		.Padding(FMargin(14.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				Txt(Label, 13, CDim)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Txt(Value, 16, Col, true)
			]
		];
}

TSharedRef<SWidget> FGPMUI::Dot(const FLinearColor& Col, float Size) const
{
	return SNew(SBox)
		.WidthOverride(Size)
		.HeightOverride(Size)
		[
			SNew(SBorder)
			.BorderImage(GPMUIStyle::Rounded(40.f))
			.BorderBackgroundColor(FSlateColor(Col))
		];
}

TSharedRef<SWidget> FGPMUI::Bar(float Frac, const FLinearColor& Col, float Height) const
{
	const float F = FMath::Clamp(Frac, 0.f, 1.f);
	return SNew(SBox)
		.HeightOverride(Height)
		[
			SNew(SBorder)
			.BorderImage(GPMUIStyle::Rounded(6.f))
			.BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.1f)))
			.Padding(FMargin(0.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(FMath::Max(F, 0.0001f))
				[
					SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(Col))
				]
				+ SHorizontalBox::Slot().FillWidth(FMath::Max(1.f - F, 0.0001f))
				[
					SNew(SSpacer)
				]
			]
		];
}

TSharedRef<SWidget> FGPMUI::Title(const FString& Kicker, const FString& Main, const TSharedRef<SWidget>& Right) const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 14.f, 0.f))
		[
			SNew(SBox).WidthOverride(6.f).HeightOverride(52.f)
			[
				SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(Acc))
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Txt(Kicker, 14, CDim, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Txt(Main, 32, CText, true) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Right
		];
}

TSharedRef<SWidget> FGPMUI::Chips() const
{
	const UGPMCareerSubsystem* C = CareerSys();
	const UGPMSaveGame* G = C ? C->Save() : nullptr;
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	if (!G) return Box;
	auto Add = [&Box](const TSharedRef<SWidget>& W) { Box->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ W ]; };
	Add(Chip(TEXT("Budget"), FString::Printf(TEXT("%.1f M€"), G->Budget), G->Budget < 0.f ? CBad : CGood));
	Add(Chip(TEXT("Moral"), FString::FromInt(G->Moral), G->Moral < 35 ? CBad : CText));
	Add(Chip(TEXT("Réputation"), FString::FromInt(G->Rep), CText));
	if (C->IsCreated()) Add(Chip(TEXT("Direction"), FString::Printf(TEXT("%d %%"), G->Conf), G->Conf < 35 ? CBad : G->Conf < 55 ? CGold : CGood));
	return Box;
}

TSharedRef<SWidget> FGPMUI::Page(const TSharedRef<SWidget>& Body, const FLinearColor& Bg) const
{
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Rounded(0.f))
		.BorderBackgroundColor(FSlateColor(Bg))
		.Padding(FMargin(36.f, 26.f))
		[
			Body
		];
}

TSharedRef<SWidget> FGPMUI::Row(const FLinearColor& TeamCol, bool bMe, const TArray<TPair<FString, float>>& Cells, const TArray<FLinearColor>& Colors) const
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			SNew(SBox).WidthOverride(5.f).HeightOverride(22.f)
			[
				SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(TeamCol))
			]
		];
	for (int32 k = 0; k < Cells.Num(); ++k)
	{
		const FLinearColor Col = Colors.IsValidIndex(k) ? Colors[k] : CText;
		TSharedRef<SWidget> Cell = Txt(Cells[k].Key, 16, bMe ? CGold : Col, bMe || k == 0);
		if (Cells[k].Value > 0.f)
			Box->AddSlot().AutoWidth().VAlign(VAlign_Center)[ SNew(SBox).WidthOverride(Cells[k].Value)[ Cell ] ];
		else
			Box->AddSlot().FillWidth(1.f).VAlign(VAlign_Center)[ Cell ];
	}
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Rounded(6.f))
		.BorderBackgroundColor(FSlateColor(bMe ? FLinearColor(Acc.R, Acc.G, Acc.B, 0.28f) : FLinearColor(1.f, 1.f, 1.f, 0.035f)))
		.Padding(FMargin(10.f, 5.f))
		[
			Box
		];
}

// ---------- accueil ----------

TSharedRef<SWidget> FGPMUI::HomeScreen(AGPMGameMode* GM)
{
	UGPMCareerSubsystem* C = CareerSys();
	UGPMSeasonSubsystem* S = SeasonSys();
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	auto Add = [&Menu](const TSharedRef<SWidget>& W, float Gap) { Menu->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, Gap))[ W ]; };

	Add(Txt(TEXT("GRAND PRIX"), 64, CText, true), 0.f);
	Add(Txt(TEXT("MANAGER"), 64, Acc, true), 6.f);
	Add(Txt(TEXT("Course de monoplaces et gestion d'écurie"), 18, CDim), 36.f);

	if (C && C->IsCreated() && S && C->Save())
	{
		const FString Sub = FString::Printf(TEXT("%s · saison %d · GP %d / %d"), *C->Save()->TeamName, S->Year(), FMath::Min(S->Round() + 1, S->NumRounds()), S->NumRounds());
		Add(Button(TEXT("CONTINUER LA CARRIÈRE"), Acc, [WGM]() { if (WGM.IsValid()) WGM->ContinueCareer(); }, true, 460.f, 64.f, 22), 4.f);
		Add(Txt(Sub, 15, CDim), 16.f);
		if (bConfirmNew)
			Add(Button(TEXT("CONFIRMER : EFFACER LA CARRIÈRE"), FLinearColor(0.6f, 0.1f, 0.1f), [this, WGM]() { bConfirmNew = false; if (WGM.IsValid()) WGM->NewCareer(); }, true, 460.f), 12.f);
		else
			Add(Button(TEXT("NOUVELLE CARRIÈRE"), CNeutral, [this]() { bConfirmNew = true; }, true, 460.f), 12.f);
	}
	else
		Add(Button(TEXT("CARRIÈRE : CRÉER TON ÉCURIE"), Acc, [WGM]() { if (WGM.IsValid()) WGM->NewCareer(); }, true, 460.f, 64.f, 22), 12.f);
	Add(Button(TEXT("COURSE SOLO"), Acc2, [this, WGM]() { bConfirmNew = false; if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Solo; }, true, 460.f), 12.f);
	Add(Button(TEXT("RÉGLAGES"), CNeutral, [this, WGM]() { bConfirmNew = false; if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Settings; }, true, 460.f), 30.f);
	Add(Txt(TEXT("Univers 100 % fictif : écuries, pilotes et circuits inventés."), 13, CFaint), 0.f);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(GPMUIStyle::Rounded(0.f))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.01f, 0.012f, 0.025f, 0.82f)))
			.Padding(FMargin(70.f, 50.f))
			.VAlign(VAlign_Center)
			[
				Menu
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom).HAlign(HAlign_Right).Padding(FMargin(30.f))
		[
			Card(Txt(TEXT("Clavier : flèches ou ZQSD pour conduire · C caméra · B stand · P pause · Entrée valider · Échap retour"), 14, CDim), FLinearColor(0.f, 0.f, 0.f, 0.55f), 12.f)
		];
}

// ---------- course solo ----------

TSharedRef<SWidget> FGPMUI::SoloScreen(AGPMGameMode* GM)
{
	UGPMDataSubsystem* Data = DataSys();
	if (!Data) return SNullWidget::NullWidget;
	const TArray<FString> Ids = Data->GetCircuitIds();
	if (Ids.Num() == 0) return SNullWidget::NullWidget;
	GM->SoloIdx = (GM->SoloIdx % Ids.Num() + Ids.Num()) % Ids.Num();
	const FGPMTrackData* T = Data->GetTrack(Ids[GM->SoloIdx]);
	TWeakObjectPtr<AGPMGameMode> WGM = GM;

	TSharedRef<SHorizontalBox> Laps = SNew(SHorizontalBox);
	static const int32 LapOpts[] = { 3, 5, 10, 20 };
	for (int32 L : LapOpts)
		Laps->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			Button(FString::Printf(TEXT("%d tours"), L), GM->SoloLaps == L ? Acc : CNeutral, [WGM, L]() { if (WGM.IsValid()) WGM->SoloLaps = L; }, true, 120.f, 46.f, 16)
		];

	TSharedRef<SHorizontalBox> Wx = SNew(SHorizontalBox);
	static const TCHAR* WxOpts[] = { TEXT("Au hasard"), TEXT("Sec"), TEXT("Pluie"), TEXT("Averse") };
	for (int32 k = 0; k < 4; ++k)
		Wx->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			Button(WxOpts[k], GM->SoloWeather == k ? Acc : CNeutral, [WGM, k]() { if (WGM.IsValid()) WGM->SoloWeather = k; }, true, 140.f, 46.f, 16)
		];

	const FString TrackLine = T ? FString::Printf(TEXT("Circuit %d / %d · %.2f km · largeur %.0f m"), GM->SoloIdx + 1, Ids.Num(), T->WX.Num() * 2.f / 1000.f, T->Width) : FString();
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 24.f))
		[
			Title(TEXT("SANS EFFET SUR LA CARRIÈRE"), TEXT("Course solo"), SNullWidget::NullWidget)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 18.f))
		[
			Card(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[ Button(TEXT("<"), CNeutral, [WGM]() { if (WGM.IsValid()) --WGM->SoloIdx; }, true, 64.f, 64.f, 26) ]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(20.f, 0.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ Txt(T ? T->Name : Ids[GM->SoloIdx], 34, CGold, true, false, true) ]
					+ SVerticalBox::Slot().AutoHeight()[ Txt(TrackLine, 15, CDim, false, false, true) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[ Button(TEXT(">"), CNeutral, [WGM]() { if (WGM.IsValid()) ++WGM->SoloIdx; }, true, 64.f, 64.f, 26) ],
				CCardBg)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(TEXT("DURÉE"), 14, CDim, true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 18.f))[ Laps ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(TEXT("GRILLE DE DÉPART"), 14, CDim, true) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[ Button(TEXT("Qualifications jouées"), GM->bSoloQuali ? Acc : CNeutral, [WGM]() { if (WGM.IsValid()) WGM->bSoloQuali = true; }, true, 240.f, 46.f, 16) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ Button(TEXT("Départ en milieu de grille"), !GM->bSoloQuali ? Acc : CNeutral, [WGM]() { if (WGM.IsValid()) WGM->bSoloQuali = false; }, true, 240.f, 46.f, 16) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 8.f))[ Txt(TEXT("MÉTÉO"), 14, CDim, true) ]
		+ SVerticalBox::Slot().AutoHeight()[ Wx ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SSpacer) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Home; }, true, 180.f) ]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("DÉPART"), Acc, [WGM]() { if (WGM.IsValid()) WGM->StartSolo(); }, true, 240.f, 58.f, 20) ]
		];
	return SNew(SBox).HAlign(HAlign_Center).Padding(FMargin(0.f, 30.f))[ SNew(SBox).WidthOverride(1000.f)[ Page(Body, FLinearColor(0.01f, 0.012f, 0.025f, 0.86f)) ] ];
}

// ---------- réglages ----------

TSharedRef<SWidget> FGPMUI::SettingsScreen(AGPMGameMode* GM)
{
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	struct FOpt { const TCHAR* Label; const TCHAR* Help; bool* Val; };
	AGPMHUD* H = Hud.Get();
	if (!H) return SNullWidget::NullWidget;
	FOpt Opts[] = {
		{ TEXT("ABS"), TEXT("Les roues ne se bloquent pas au freinage."), &GM->bSetABS },
		{ TEXT("Antipatinage"), TEXT("Limite le patinage des roues arrière à l'accélération."), &GM->bSetTC },
		{ TEXT("Aide à la stabilité"), TEXT("Rattrape les débuts de tête-à-queue."), &GM->bSetStab },
		{ TEXT("Caméra cockpit au départ"), TEXT("Sinon, caméra derrière la voiture (touche C pour changer)."), &GM->bSetCockpit },
		{ TEXT("Boutons tactiles à l'écran"), TEXT("Gauche, droite, frein, gaz et stand (touche T)."), &H->bShowTouch } };
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FOpt& O : Opts)
	{
		bool* Val = O.Val;
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			Card(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ Txt(O.Label, 20, CText, true) ]
					+ SVerticalBox::Slot().AutoHeight()[ Txt(O.Help, 14, CDim) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Button(*Val ? TEXT("OUI") : TEXT("NON"), *Val ? FLinearColor(0.15f, 0.55f, 0.3f) : CNeutral, [WGM, Val]() { *Val = !*Val; if (WGM.IsValid()) WGM->SaveSettings(); }, true, 110.f, 44.f, 16)
				],
				CCardBg, 14.f)
		];
	}
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 24.f))[ Title(TEXT("PILOTAGE ET AFFICHAGE"), TEXT("Réglages"), SNullWidget::NullWidget) ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 16.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Home; }, true, 180.f) ]
		];
	return SNew(SBox).HAlign(HAlign_Center).Padding(FMargin(0.f, 30.f))[ SNew(SBox).WidthOverride(900.f)[ Page(Body, FLinearColor(0.01f, 0.012f, 0.025f, 0.88f)) ] ];
}

// ---------- fenêtres à choix : presse, flash info, histoires, négociations ----------

TSharedRef<SWidget> FGPMUI::DialogScreen(AGPMGameMode* GM)
{
	UGPMCareerSubsystem* C = CareerSys();
	FGPMDialog* Dl = C ? C->Current() : nullptr;
	if (!Dl) return SNullWidget::NullWidget;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Add = [&Box](const TSharedRef<SWidget>& W, float Gap) { Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, Gap))[ W ]; };

	// bandeau du média ou de la rubrique
	Add(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBorder).BorderImage(GPMUIStyle::Rounded(40.f)).BorderBackgroundColor(FSlateColor(Dl->Color)).Padding(FMargin(14.f, 5.f))
			[ Txt(Dl->Kicker.ToUpper(), 14, InkOn(Dl->Color), true) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Chips() ], 18.f);
	if (!Dl->Title.IsEmpty()) Add(Txt(Dl->Title, 30, CText, true, true), 10.f);
	Add(Txt(Dl->Text, 19, FLinearColor(0.86f, 0.88f, 0.92f, 1.f), false, true), 22.f);

	if (Dl->Chosen < 0)
	{
		for (int32 k = 0; k < Dl->Choices.Num(); ++k)
		{
			const FGPMChoice& Ch = Dl->Choices[k];
			TSharedRef<SWidget> Content = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 14.f, 0.f))
				[
					SNew(SBox).WidthOverride(34.f).HeightOverride(34.f)
					[
						SNew(SBorder).BorderImage(GPMUIStyle::Rounded(40.f)).BorderBackgroundColor(FSlateColor(Ch.bOff ? CNeutral : Dl->Color)).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[ Txt(FString::FromInt(k + 1), 16, Ch.bOff ? CFaint : InkOn(Dl->Color), true) ]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[ Txt(Ch.Text, 18, Ch.bOff ? CFaint : CText, false, true) ];
			Add(Pick(Content, false, [this, k]() { if (UGPMCareerSubsystem* CS = CareerSys()) CS->Choose(k); }, !Ch.bOff), 10.f);
		}
		Add(Txt(TEXT("Touche une réponse (ou les touches 1 à 5)."), 13, CFaint), 0.f);
	}
	else
	{
		if (Dl->Choices.IsValidIndex(Dl->Chosen) && Dl->Kind != FGPMDialog::EKind::Nego)
			Add(Card(Txt(Dl->Choices[Dl->Chosen].Text, 18, CGold, true, true), FLinearColor(Acc.R * 0.4f, Acc.G * 0.4f, Acc.B * 0.4f, 0.8f), 14.f), 14.f);
		if (!Dl->Result.IsEmpty()) Add(Txt(Dl->Result, 19, Dl->bBad ? CBad : CText, false, true), 12.f);
		if (!Dl->FxText.IsEmpty()) Add(Card(Txt(Dl->FxText, 16, CGood, true, true), FLinearColor(0.1f, 0.3f, 0.15f, 0.6f), 12.f), 18.f);
		Add(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Button(TEXT("CONTINUER"), Acc, [this, WGM]()
				{
					UGPMCareerSubsystem* CS = CareerSys();
					if (!CS) return;
					CS->Next();
					if (!CS->Current() && WGM.IsValid()) WGM->EndDialogs();
				}, true, 220.f)
			], 0.f);
	}
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Rounded(0.f))
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(30.f))
		[
			SNew(SBox).WidthOverride(1080.f)
			[
				SNew(SBorder)
				.BorderImage(GPMUIStyle::Rounded(14.f))
				.BorderBackgroundColor(FSlateColor(CPanel))
				.Padding(FMargin(34.f, 28.f))
				[
					SNew(SScrollBox) + SScrollBox::Slot()[ Box ]
				]
			]
		];
}

// ---------- licencié ----------

TSharedRef<SWidget> FGPMUI::FiredScreen(AGPMGameMode* GM)
{
	UGPMCareerSubsystem* C = CareerSys();
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	const FString Team = C && C->Save() ? C->Save()->TeamName : FString(TEXT("l'écurie"));
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[ Txt(TEXT("LICENCIÉ !"), 54, CBad, true, false, true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 20.f))
		[ Txt(FString::Printf(TEXT("La direction de %s a perdu confiance et met fin à ton contrat."), *Team), 20, CText, false, true, true) ];
	for (const FString& L : GM->LastLines) Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))[ Txt(L, 16, CDim, false, true, true) ];
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 24.f, 0.f, 0.f))
	[
		Button(TEXT("NOUVELLE CARRIÈRE"), Acc, [WGM]() { if (WGM.IsValid()) WGM->NewCareer(); }, true, 300.f, 58.f, 20)
	];
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Rounded(0.f))
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.1f, 0.f, 0.f, 0.85f)))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(820.f)[ Card(Box, CPanel, 40.f) ]
		];
}
