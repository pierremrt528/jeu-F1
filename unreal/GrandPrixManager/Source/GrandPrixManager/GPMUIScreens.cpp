// Grands écrans de gestion en Slate : saison, usine, mercato, création de l'écurie, qualifications, résultats, sacre.
#include "GPMUI.h"
#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
#include "GPMDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

using namespace GPMUIColor;

namespace GPMUIScreensText
{
	const TCHAR* SponsorGoal(const FString& G)
	{
		return G == TEXT("finish") ? TEXT("vois l'arrivée") : G == TEXT("beatMate") ? TEXT("bats ton équipier") : G == TEXT("gain") ? TEXT("gagnes des places")
			: G == TEXT("pts") ? TEXT("marques des points") : G == TEXT("podium") ? TEXT("montes sur le podium") : TEXT("fais le meilleur tour");
	}

	FString RoleName(const FString& R)
	{
		return R == TEXT("n1") ? TEXT("N°1") : R == TEXT("n2") ? TEXT("N°2") : TEXT("Égalité");
	}
}

// ---------- saison ----------

TSharedRef<SWidget> FGPMUI::SeasonScreen(AGPMGameMode* GM)
{
	UGPMSeasonSubsystem* S = SeasonSys();
	UGPMCareerSubsystem* C = CareerSys();
	UGPMDataSubsystem* Data = DataSys();
	if (!S) return SNullWidget::NullWidget;
	if (S->IsOver() && !GM->bChampShown) return ChampScreen(GM);
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	const UGPMSaveGame* G = S->State();
	// mission du sponsor proposée avant les qualifications, premier relevé des courbes de performance
	if (C && C->IsCreated() && !S->IsOver() && !GM->bGridReady) C->EnsureMission();
	if (UGPMFactorySubsystem* F = FactorySys()) F->EnsurePerfStart();

	// en-tête : saison, boutons de navigation
	TSharedRef<SHorizontalBox> Nav = SNew(SHorizontalBox);
	Nav->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))[ Button(TEXT("ACCUEIL"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->GoHome(); }, true, 0.f, 46.f, 15) ];
	if (C && C->IsCreated())
		Nav->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))[ Button(TEXT("MERCATO"), Acc2, [WGM]() { if (WGM.IsValid()) WGM->OpenMercato(); }, true, 0.f, 46.f, 15) ];
	Nav->AddSlot().AutoWidth()[ Button(TEXT("QG · USINE"), Acc2, [WGM]() { if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Factory; }, true, 0.f, 46.f, 15) ];

	// prochain Grand Prix
	TSharedRef<SVerticalBox> NextBox = SNew(SVerticalBox);
	if (!S->IsOver())
	{
		const FGPMTrackData* T = Data ? Data->GetTrack(S->CurrentCircuit()) : nullptr;
		NextBox->AddSlot().AutoHeight()[ Txt(FString::Printf(TEXT("PROCHAIN GRAND PRIX · %d / %d"), S->Round() + 1, S->NumRounds()), 14, CDim, true) ];
		NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 2.f))[ Txt(T ? T->Name : S->CurrentCircuit(), 30, CText, true, true) ];
		if (T) NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			Txt(GM->Circuit() == T ? FString::Printf(TEXT("%.2f km · largeur %.0f m · météo : %s"), T->WX.Num() * 2.f / 1000.f, T->Width, *GM->WeatherLabel().ToLower())
				: FString::Printf(TEXT("%.2f km · largeur %.0f m"), T->WX.Num() * 2.f / 1000.f, T->Width), 15, GM->bRainStart || GM->WxChangeAt >= 0.f ? CInfo : CDim)
		];
		// format de course : Sprint (~11 min) ou Long (~22 min)
		const int32 NLaps = GM->Circuit() == T ? GM->LapsFor() : 0;
		NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[ Button(TEXT("Sprint · 11 min"), !GM->bLongRace ? Acc2 : CNeutral, [WGM]() { if (WGM.IsValid()) { WGM->bLongRace = false; WGM->SaveSettings(); } }, !GM->bGridReady || !GM->bLongRace, 0.f, 40.f, 14) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[ Button(TEXT("Long · 22 min"), GM->bLongRace ? Acc2 : CNeutral, [WGM]() { if (WGM.IsValid()) { WGM->bLongRace = true; WGM->SaveSettings(); } }, !GM->bGridReady || GM->bLongRace, 0.f, 40.f, 14) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Txt(NLaps > 0 ? FString::Printf(TEXT("%d tours"), NLaps) : FString(), 15, CDim, true) ]
		];
		if (!GM->bGridReady)
		{
			NextBox->AddSlot().AutoHeight()[ Button(TEXT("RÉGLAGES ET QUALIFICATIONS"), Acc, [WGM]() { if (WGM.IsValid()) WGM->OpenTuning(); }, true, 0.f, 60.f, 20) ];
			NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))[ Button(TEXT("SIMULER LE GRAND PRIX"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->SimGrandPrix(); }, true, 0.f, 46.f, 15) ];
			NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(TEXT("Entrée : réglages · le Grand Prix simulé saute les qualifications et la course"), 13, CFaint, false, true) ];
		}
		else
		{
			AGPMHUD* HudP = Hud.Get();
			AGPMCar* MeCar = HudP ? Cast<AGPMCar>(HudP->GetOwningPawn()) : nullptr;
			const int32 MyGrid = MeCar ? GM->QOrder.IndexOfByKey(MeCar) : INDEX_NONE;
			NextBox->AddSlot().AutoHeight()[ Button(TEXT("STRATÉGIE ET DÉPART"), Acc, [WGM]() { if (WGM.IsValid()) WGM->OpenPrerace(); }, true, 0.f, 60.f, 20) ];
			NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))[ Button(TEXT("SIMULER LA COURSE"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Go(true); }, true, 0.f, 46.f, 15) ];
			NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(MyGrid >= 0 ? FString::Printf(TEXT("Qualifications faites : tu pars en P%d"), MyGrid + 1) : FString(TEXT("Qualifications faites")), 13, CFaint) ];
		}
	}
	else
	{
		NextBox->AddSlot().AutoHeight()[ Txt(TEXT("SAISON TERMINÉE"), 14, CDim, true) ];
		NextBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 14.f))[ Txt(TEXT("Place à l'intersaison"), 28, CText, true) ];
		NextBox->AddSlot().AutoHeight()[ Button(TEXT("SAISON SUIVANTE"), Acc, [WGM]() { if (WGM.IsValid()) WGM->Continue(); }, true, 0.f, 60.f, 22) ];
	}

	// direction : confiance et objectifs, bilan de la dernière course, actualités de l'usine
	TSharedRef<SVerticalBox> Board = SNew(SVerticalBox);
	if (C && C->Save() && C->IsCreated())
	{
		const int32 Conf = C->Save()->Conf;
		const FLinearColor ConfCol = Conf < 35 ? CBad : Conf < 55 ? CGold : CGood;
		Board->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[ Txt(TEXT("DIRECTION"), 14, CDim, true) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Txt(FString::Printf(TEXT("confiance %d %% · %s"), Conf, *UGPMCareerSubsystem::ConfText(Conf)), 14, ConfCol, true) ]
		];
		Board->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 10.f))[ Bar(Conf / 100.f, ConfCol, 6.f) ];
		for (const TPair<FString, bool>& O : C->Objectives())
			Board->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))[ Dot(O.Value ? CGood : FLinearColor(1.f, 1.f, 1.f, 0.2f), 12.f) ]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Txt(O.Key, 15, O.Value ? CText : CDim, false, true) ]
			];
	}
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	for (const FString& L : GM->LastLines) Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[ Txt(L, 14, CDim, false, true) ];
	if (G) for (int32 k = 0; k < G->News.Num() && k < 5; ++k) Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[ Txt(G->News[k], 14, CGood, false, true) ];
	const TArray<FGPMRaceResult>& Res = S->Results();
	if (Res.Num())
	{
		TSharedRef<SHorizontalBox> Form = SNew(SHorizontalBox);
		for (int32 k = FMath::Max(0, Res.Num() - 6); k < Res.Num(); ++k)
		{
			const int32 P = Res[k].Pos;
			const FLinearColor PC = P == 1 ? CGold : P <= 3 ? FLinearColor(0.75f, 0.78f, 0.85f) : P <= 10 ? CGood : CNeutral;
			Form->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SBox).WidthOverride(46.f).HeightOverride(34.f)
				[
					SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(PC)).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[ Txt(FString::Printf(TEXT("P%d"), P), 14, InkOn(PC), true) ]
				]
			];
		}
		Lines->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 4.f))[ Txt(TEXT("TES DERNIERS GRANDS PRIX"), 13, CDim, true) ];
		Lines->AddSlot().AutoHeight()[ Form ];
	}

	SeasonTab = FMath::Clamp(SeasonTab, 0, 2);
	TSharedRef<SWidget> Panel = SeasonTab == 1 ? CalendarPanel() : SeasonTab == 2 ? PerfPanel() : StandingsPanel();
	const FString TeamName = C && C->Save() && C->IsCreated() ? C->Save()->TeamName : FString(TEXT("Championnat"));
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ Title(FString::Printf(TEXT("SAISON %d"), S->Year()), TeamName, Nav) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 16.f))[ Chips() ]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.95f).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[ SNew(SBox).Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ Card(NextBox, FLinearColor(Acc.R * 0.3f, Acc.G * 0.3f, Acc.B * 0.3f, 0.75f), 20.f) ] ]
				+ SScrollBox::Slot()[ MissionCard() ]
				+ SScrollBox::Slot()[ SNew(SBox).Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ Card(Board, CCardBg, 16.f) ] ]
				+ SScrollBox::Slot()[ Card(Lines, CCardBg, 16.f) ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.95f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[ Tabs({ TEXT("CLASSEMENTS"), TEXT("CALENDRIER"), TEXT("PERFORMANCE") }, SeasonTab, [this](int32 k) { SeasonTab = k; }) ]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Right)[ Txt(TEXT("Tab : onglet suivant"), 13, CFaint) ]
				]
				+ SVerticalBox::Slot().FillHeight(1.f)[ Panel ]
			]
		];
	return Page(Body, CPageBg);
}

// ---------- sacre de fin de saison ----------

TSharedRef<SWidget> FGPMUI::ChampScreen(AGPMGameMode* GM)
{
	UGPMSeasonSubsystem* S = SeasonSys();
	UGPMCareerSubsystem* C = CareerSys();
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	if (!S) return SNullWidget::NullWidget;
	const TArray<FGPMStanding> Drv = S->Drivers(), Tms = S->Teams();
	const bool bDrv = Drv.Num() && Drv[0].bPlayer, bTeam = Tms.Num() && Tms[0].bPlayer;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("SAISON %d"), S->Year()), 18, CDim, true, false, true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 18.f))[ Txt(bDrv ? TEXT("CHAMPION DU MONDE !") : TEXT("CHAMPIONS DES ÉCURIES !"), 48, CGold, true, false, true) ];
	if (bDrv) Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))[ Txt(FString::Printf(TEXT("%s · %d points"), *Nm(Drv[0].Name), Drv[0].Pts), 22, CText, true, false, true) ];
	if (bTeam) Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))[ Txt(FString::Printf(TEXT("%s, champion des écuries · %d points"), *Tms[0].Name, Tms[0].Pts), 18, CText, false, false, true) ];
	if (C && C->Save()) Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(FString::Printf(TEXT("Réputation %d · budget %.1f M€"), C->Save()->Rep, C->Save()->Budget), 16, CDim, false, false, true) ];
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 26.f, 0.f, 0.f))
	[
		Button(TEXT("CONTINUER"), CGold, [WGM]() { if (WGM.IsValid()) WGM->bChampShown = true; }, true, 260.f, 56.f, 20)
	];
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Rounded(0.f))
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.7f)))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(820.f)
			[
				SNew(SBorder)
				.BorderImage(GPMUIStyle::Rounded(16.f))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.14f, 0.11f, 0.02f, 0.97f)))
				.Padding(FMargin(40.f))
				[
					Box
				]
			]
		];
}

// ---------- mercato ----------

TSharedRef<SWidget> FGPMUI::MercatoScreen(AGPMGameMode* GM)
{
	UGPMCareerSubsystem* C = CareerSys();
	const UGPMSaveGame* G = C ? C->Save() : nullptr;
	if (!G) return SNullWidget::NullWidget;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;

	TSharedRef<SWidget> Mate = Card(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Txt(TEXT("TON ÉQUIPIER"), 13, CDim, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Txt(G->MateName, 24, CGold, true) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(TEXT("Vitesse"), FString::Printf(TEXT("%.0f"), G->MateSpd), CText) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(TEXT("Salaire"), FString::Printf(TEXT("%.1f M€"), G->MateSal), CText) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(TEXT("Contrat"), FString::Printf(TEXT("jusqu'en %d"), G->MateSince + G->MateYears - 1), CText) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Chip(TEXT("Statut"), GPMUIScreensText::RoleName(G->MateRole), CText) ],
		FLinearColor(Acc.R * 0.3f, Acc.G * 0.3f, Acc.B * 0.3f, 0.75f), 16.f);

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	const TArray<FGPMMarketDriver> Drivers = C->MarketList();
	for (const FGPMMarketDriver& D : Drivers)
	{
		const float Rating = D.Rating();
		const FLinearColor RCol = Rating >= 80.f ? CGold : Rating >= 72.f ? CGood : CText;
		TSharedRef<SWidget> Content = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 14.f, 0.f))
			[
				SNew(SBox).WidthOverride(52.f).HeightOverride(40.f)
				[
					SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[ Txt(FString::Printf(TEXT("%.0f"), Rating), 20, RCol, true) ]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(D.Name, 18, CText, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("%s · %d ans"), D.Seat >= 0 ? *D.Team : *D.From, D.Age), 13, CDim) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f))
			[ Txt(FString::Printf(TEXT("vit. %.0f · strat. %.0f · pneus %.0f · pot. %.0f"), D.Spd, D.Strat, D.Tyre, D.Pot), 14, CDim) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 0.f, 0.f))
			[ SNew(SBox).WidthOverride(200.f)[ Txt(C->Status(D), 14, FLinearColor(1.f, 0.75f, 0.4f), true, true) ] ];
		const FGPMMarketDriver Copy = D;
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		[
			Pick(Content, false, [this, WGM, Copy]()
			{
				UGPMCareerSubsystem* CS = CareerSys();
				if (!CS || !WGM.IsValid()) return;
				CS->OpenNego(Copy);
				WGM->bNegoFromMercato = true;
				WGM->Phase = AGPMGameMode::EPhase::Press;
			})
		];
	}

	TSharedRef<SHorizontalBox> Right = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 16.f, 0.f))[ Chip(TEXT("Budget"), FString::Printf(TEXT("%.1f M€"), G->Budget), CGood) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[ Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) { WGM->bNegoFromMercato = false; WGM->Phase = AGPMGameMode::EPhase::Season; } }, true, 150.f, 46.f, 15) ];

	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))[ Title(TEXT("MERCATO"), TEXT("Marché des pilotes"), Right) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ Mate ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
		[ Txt(TEXT("Touche un pilote pour négocier avec son agent. Pendant la saison, les indemnités de transfert sont multipliées par 2,5."), 14, CDim, false, true) ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ];
	return Page(Body, CPageBg);
}

// ---------- création de l'écurie (6 étapes) ----------

TSharedRef<SWidget> FGPMUI::SetupScreen(AGPMGameMode* GM)
{
	UGPMCareerSubsystem* C = CareerSys();
	if (!C) return SNullWidget::NullWidget;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	TWeakObjectPtr<AGPMHUD> WHud = Hud;
	FGPMPick& P = C->Pick;
	static const TCHAR* Steps[] = { TEXT("Identité"), TEXT("Niveau"), TEXT("Couleurs"), TEXT("Motoriste"), TEXT("Équipier"), TEXT("Sponsor") };
	static const TCHAR* Hints[] = {
		TEXT("Choisis le nom de ton écurie et ton nom de pilote."),
		TEXT("Le niveau fixe ton budget, la vitesse de départ de la voiture et les attentes de la direction."),
		TEXT("Les couleurs de la livrée : la voiture à droite change tout de suite."),
		TEXT("Le moteur coûte chaque saison : puissance, fiabilité et consommation."),
		TEXT("Ton coéquipier marque des points pour l'écurie. Les jeunes progressent, les vétérans coûtent cher."),
		TEXT("Le sponsor principal paie chaque saison, plus un bonus quand son objectif de course est rempli.") };
	const int32 St = FMath::Clamp(GM->SetupStep, 0, 5);

	// fil des étapes
	TSharedRef<SHorizontalBox> Trail = SNew(SHorizontalBox);
	for (int32 k = 0; k < 6; ++k)
	{
		const FLinearColor Col = k == St ? Acc : k < St ? FLinearColor(0.2f, 0.5f, 0.3f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.08f);
		Trail->AddSlot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, k < 5 ? 6.f : 0.f, 0.f))
		[
			SNew(SBorder).BorderImage(GPMUIStyle::Rounded(40.f)).BorderBackgroundColor(FSlateColor(Col)).HAlign(HAlign_Center).Padding(FMargin(6.f, 6.f))
			[ Txt(FString::Printf(TEXT("%d. %s"), k + 1, Steps[k]), 13, k == St ? InkOn(Acc) : (k < St ? CText : CDim), true) ]
		];
	}

	auto Grid = [](int32) { return SNew(SUniformGridPanel).SlotPadding(FMargin(5.f)); };
	TSharedRef<SWidget> Content = SNullWidget::NullWidget;
	if (St == 0)
	{
		TSharedRef<SUniformGridPanel> Names = Grid(3), Pilots = Grid(3);
		for (int32 k = 0; k < C->TeamNames.Num(); ++k)
			Names->AddSlot(k % 3, k / 3)[ Pick(Txt(C->TeamNames[k], 17, CText, P.Name == k), P.Name == k, [this, k]() { if (UGPMCareerSubsystem* CS = CareerSys()) CS->Pick.Name = k; }) ];
		for (int32 k = 0; k < C->PilotNames.Num(); ++k)
			Pilots->AddSlot(k % 3, k / 3)[ Pick(Txt(C->PilotNames[k], 17, CText, P.Pilot == k), P.Pilot == k, [this, k]() { if (UGPMCareerSubsystem* CS = CareerSys()) CS->Pick.Pilot = k; }) ];
		Content = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.f, 0.f, 0.f, 4.f))[ Txt(TEXT("NOM DE L'ÉCURIE"), 13, CDim, true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))[ Names ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(5.f, 0.f, 0.f, 4.f))[ Txt(TEXT("TON NOM DE PILOTE"), 13, CDim, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Pilots ];
	}
	else if (St == 1)
	{
		TSharedRef<SUniformGridPanel> Cards = Grid(3);
		for (int32 k = 0; k < C->Tiers.Num(); ++k)
		{
			const FGPMTierOpt& T = C->Tiers[k];
			TSharedRef<SWidget> W = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(T.Name, 20, CGold, true, true) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 10.f))[ Txt(T.Desc, 14, CText, false, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Budget %.0f M€"), T.Budget), 16, CGood, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Voiture %.1f %%"), T.Perf * 100.f), 14, CDim) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Réputation %d · moral %d"), T.Rep, T.Moral), 14, CDim) ];
			Cards->AddSlot(k, 0)[ Pick(W, P.Tier == k, [this, k]()
			{
				UGPMCareerSubsystem* CS = CareerSys();
				if (!CS) return;
				CS->Pick.Tier = k;
				if (CS->Sponsors.IsValidIndex(CS->Pick.Sponsor) && CS->Tiers.IsValidIndex(k) && CS->Sponsors[CS->Pick.Sponsor].Rep > CS->Tiers[k].Rep) CS->Pick.Sponsor = 0;
			}) ];
		}
		Content = Cards;
	}
	else if (St == 2)
	{
		TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
		for (int32 Line = 0; Line < 2; ++Line)
		{
			TSharedRef<SUniformGridPanel> Sw = Grid(5);
			const int32 Sel = Line == 0 ? P.Col1 : P.Col2;
			for (int32 k = 0; k < C->Colors.Num(); ++k)
			{
				const FLinearColor Col(C->Colors[k].Color.R, C->Colors[k].Color.G, C->Colors[k].Color.B, 1.f);
				TSharedRef<SWidget> W = SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(34.f)[ SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(Col)) ]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[ Txt(C->Colors[k].Name, 12, CDim) ];
				Sw->AddSlot(k % 5, k / 5)[ Pick(W, Sel == k, [this, WHud, Line, k]()
				{
					UGPMCareerSubsystem* CS = CareerSys();
					if (!CS) return;
					(Line == 0 ? CS->Pick.Col1 : CS->Pick.Col2) = k;
					// aperçu : la monoplace prend les couleurs tout de suite
					if (WHud.IsValid())
						if (AGPMCar* Car = Cast<AGPMCar>(WHud->GetOwningPawn()))
							if (CS->Colors.IsValidIndex(CS->Pick.Col1) && CS->Colors.IsValidIndex(CS->Pick.Col2)) Car->SetLivery(CS->Colors[CS->Pick.Col1].Color, CS->Colors[CS->Pick.Col2].Color);
				}) ];
			}
			Rows->AddSlot().AutoHeight().Padding(FMargin(5.f, Line ? 12.f : 0.f, 0.f, 4.f))[ Txt(Line == 0 ? TEXT("COULEUR PRINCIPALE") : TEXT("COULEUR SECONDAIRE"), 13, CDim, true) ];
			Rows->AddSlot().AutoHeight()[ Sw ];
		}
		if (P.Col1 == P.Col2) Rows->AddSlot().AutoHeight().Padding(FMargin(5.f, 10.f, 0.f, 0.f))[ Txt(TEXT("Choisis deux couleurs différentes."), 15, CBad, true) ];
		Content = Rows;
	}
	else if (St == 3)
	{
		TSharedRef<SUniformGridPanel> Cards = Grid(3);
		for (int32 k = 0; k < C->Engines.Num(); ++k)
		{
			const FGPMEngineOpt& E = C->Engines[k];
			TSharedRef<SWidget> W = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(E.Name, 19, CGold, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Puissance %d"), E.Perf), 14, CText) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 6.f))[ Bar(E.Perf / 100.f, CGood, 4.f) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Fiabilité %d"), E.Fiab), 14, CText) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 6.f))[ Bar(E.Fiab / 100.f, CInfo, 4.f) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Consommation %d %%"), E.Conso), 14, CDim) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(FString::Printf(TEXT("%.0f M€ / saison"), E.Price), 16, FLinearColor(1.f, 0.75f, 0.4f), true) ];
			Cards->AddSlot(k % 3, k / 3)[ Pick(W, P.Engine == k, [this, k]() { if (UGPMCareerSubsystem* CS = CareerSys()) CS->Pick.Engine = k; }) ];
		}
		Content = Cards;
	}
	else if (St == 4)
	{
		TSharedRef<SUniformGridPanel> Cards = Grid(3);
		for (int32 k = 0; k < C->Market.Num(); ++k)
		{
			const FGPMMarketDriver& D = C->Market[k];
			const float Sal = UGPMCareerSubsystem::Salary(D);
			TSharedRef<SWidget> W = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[ Txt(D.Name, 17, CGold, true) ]
					+ SHorizontalBox::Slot().AutoWidth()[ Txt(FString::Printf(TEXT("%.0f"), D.Rating()), 20, CText, true) ]
				]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("%s · %d ans"), *D.From, D.Age), 13, CDim) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f))[ Txt(FString::Printf(TEXT("vitesse %.0f · potentiel %.0f"), D.Spd, D.Pot), 14, CText) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(Sal < 0 ? FString::Printf(TEXT("apporte %.1f M€"), -Sal) : FString::Printf(TEXT("%.1f M€ / saison"), Sal), 15, FLinearColor(1.f, 0.75f, 0.4f), true) ];
			Cards->AddSlot(k % 3, k / 3)[ Pick(W, P.Mate == k, [this, k]() { if (UGPMCareerSubsystem* CS = CareerSys()) CS->Pick.Mate = k; }) ];
		}
		Content = Cards;
	}
	else
	{
		TSharedRef<SUniformGridPanel> Cards = Grid(3);
		const int32 TierRep = C->Tiers.IsValidIndex(P.Tier) ? C->Tiers[P.Tier].Rep : 50;
		for (int32 k = 0; k < C->Sponsors.Num(); ++k)
		{
			const FGPMSponsorOpt& Sp = C->Sponsors[k];
			const bool bOk = Sp.Rep <= TierRep;
			TSharedRef<SVerticalBox> W = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(Sp.Name, 18, bOk ? CGold : CDim, true) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))[ Txt(Sp.Sector, 13, CDim) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("%.0f M€ / saison · signature +%.1f M€"), Sp.Year, Sp.Sign), 14, CText) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[ Txt(FString::Printf(TEXT("Bonus %.1f M€ par GP si tu %s"), Sp.Bonus, GPMUIScreensText::SponsorGoal(Sp.Goal)), 13, CDim, false, true) ];
			if (!bOk) W->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(FString::Printf(TEXT("Réputation %d requise"), Sp.Rep), 13, CBad, true) ];
			Cards->AddSlot(k % 3, k / 3)[ Pick(W, P.Sponsor == k, [this, k]() { if (UGPMCareerSubsystem* CS = CareerSys()) CS->Pick.Sponsor = k; }, bOk) ];
		}
		Content = Cards;
	}

	FString Why;
	const bool bValid = St == 2 ? P.Col1 != P.Col2 : St < 5 ? true : C->PickValid(Why);
	const float Budget = C->PickBudget();
	TSharedRef<SWidget> Nav = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			Button(St == 0 ? TEXT("ACCUEIL") : TEXT("RETOUR"), CNeutral, [WGM, St]()
			{
				if (!WGM.IsValid()) return;
				if (St == 0) WGM->Phase = AGPMGameMode::EPhase::Home; else --WGM->SetupStep;
			}, true, 160.f)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(16.f, 0.f))[ Txt(Why, 14, CBad, true, true) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			Button(St < 5 ? TEXT("SUIVANT") : TEXT("C'EST PARTI !"), Acc, [WGM, St]()
			{
				if (!WGM.IsValid()) return;
				if (St < 5) ++WGM->SetupStep; else WGM->FinishSetup();
			}, bValid, 220.f, 56.f, 19)
		];

	TSharedRef<SWidget> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			Title(FString::Printf(TEXT("CRÉE TON ÉCURIE · ÉTAPE %d / 6"), St + 1), Steps[St],
				Chip(TEXT("Budget de départ"), FString::Printf(TEXT("%.1f M€"), Budget), Budget >= 0.f ? CGood : CBad))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[ Trail ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ Txt(Hints[St], 15, CDim, false, true) ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ Content ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 14.f, 0.f, 0.f))[ Nav ];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.66f)
		[
			SNew(SBorder)
			.BorderImage(GPMUIStyle::Rounded(0.f))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.01f, 0.012f, 0.025f, 0.9f)))
			.Padding(FMargin(34.f, 24.f))
			[
				Left
			]
		]
		+ SHorizontalBox::Slot().FillWidth(0.34f)[ SNew(SSpacer) ];
}

// ---------- qualifications ----------

TSharedRef<SWidget> FGPMUI::QualiScreen(AGPMGameMode* GM)
{
	AGPMHUD* H = Hud.Get();
	const AGPMCar* Me = H ? Cast<AGPMCar>(H->GetOwningPawn()) : nullptr;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	const TArray<AGPMCar*>& Q = GM->QOrder;
	const float Pole = Q.Num() ? GM->QTimes.FindRef(Q[0]) : 0.f;
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	int32 MyPos = 0;
	for (int32 k = 0; k < Q.Num(); ++k)
	{
		AGPMCar* Cr = Q[k];
		const float Tm = GM->QTimes.FindRef(Cr);
		if (Cr == Me) MyPos = k + 1;
		TArray<TPair<FString, float>> Cells;
		Cells.Emplace(FString::FromInt(k + 1), 34.f);
		Cells.Emplace(Nm(Cr->DriverName), 0.f);
		Cells.Emplace(Cr->TeamName, 200.f);
		Cells.Emplace(Tm >= 999.f ? FString(TEXT("pas de temps")) : Clock(Tm), 120.f);
		Cells.Emplace(k > 0 && Tm < 999.f ? FString::Printf(TEXT("+%.3f"), Tm - Pole) : FString(), 90.f);
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 3.f))[ Row(Cr->TeamColor, Cr == Me, Cells, { CText, CText, CDim, k == 0 ? CPurple : CText, CDim }) ];
	}
	TSharedRef<SWidget> Buttons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[ Button(TEXT("REFAIRE UN TOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->StartQuali(); }, true, 200.f, 50.f, 16) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ Button(TEXT("STRATÉGIE DE COURSE"), Acc, [WGM]() { if (WGM.IsValid()) WGM->OpenPrerace(); }, true, 240.f, 50.f, 18) ];
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[ Title(TEXT("QUALIFICATIONS"), MyPos > 0 ? FString::Printf(TEXT("Tu pars en P%d"), MyPos) : FString(TEXT("Grille de départ")), Buttons) ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 0.f))[ Txt(TEXT("Entrée : stratégie de course · Retour arrière : refaire un tour (ton meilleur temps compte)"), 13, CFaint) ];
	return SNew(SBox).HAlign(HAlign_Center)[ SNew(SBox).WidthOverride(1240.f)[ Page(Body, FLinearColor(0.01f, 0.012f, 0.025f, 0.88f)) ] ];
}

// ---------- résultats ----------

TSharedRef<SWidget> FGPMUI::ResultsScreen(AGPMGameMode* GM)
{
	AGPMHUD* H = Hud.Get();
	const AGPMCar* Me = H ? Cast<AGPMCar>(H->GetOwningPawn()) : nullptr;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	const TArray<AGPMCar*>& Ord = GM->Standings();
	const float Lead = Ord.Num() ? GM->ResultTime(Ord[0]) : 0.f;
	float Fastest = 1e9f;
	for (const AGPMCar* Cr : Ord) if (Cr->BestLap > 0.f) Fastest = FMath::Min(Fastest, Cr->BestLap);
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	int32 MyPos = 0;
	for (int32 k = 0; k < Ord.Num(); ++k)
	{
		const AGPMCar* Cr = Ord[k];
		if (Cr == Me) MyPos = k + 1;
		const float Tm = GM->ResultTime(Cr);
		TArray<TPair<FString, float>> Cells;
		Cells.Emplace(FString::FromInt(k + 1), 34.f);
		Cells.Emplace(Nm(Cr->DriverName), 0.f);
		Cells.Emplace(Cr->TeamName, 190.f);
		Cells.Emplace(k == 0 ? Clock(Tm) : FString::Printf(TEXT("+%.3f%s"), Tm - Lead, Cr->bFinished ? TEXT("") : TEXT(" (est.)")), 140.f);
		Cells.Emplace(Clock(Cr->BestLap), 110.f);
		Cells.Emplace(k < 10 ? FString::Printf(TEXT("%d pts"), AGPMGameMode::Points[k]) : FString(), 70.f);
		const bool bBest = Cr->BestLap > 0.f && FMath::IsNearlyEqual(Cr->BestLap, Fastest);
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 3.f))[ Row(Cr->TeamColor, Cr == Me, Cells, { CText, CText, CDim, CText, bBest ? CPurple : CDim, CGood }) ];
	}
	TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (GM->bCareer)
		Buttons->AddSlot().AutoWidth()[ Button(TEXT("CONFÉRENCE DE PRESSE"), Acc, [WGM]() { if (WGM.IsValid()) WGM->Continue(); }, true, 260.f, 52.f, 17) ];
	else
	{
		Buttons->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))[ Button(TEXT("MENU"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->GoHome(); }, true, 140.f, 52.f, 16) ];
		Buttons->AddSlot().AutoWidth()[ Button(TEXT("RECOMMENCER"), Acc, [WGM]() { if (WGM.IsValid()) WGM->Continue(); }, true, 200.f, 52.f, 17) ];
	}
	TSharedRef<SVerticalBox> Side = SNew(SVerticalBox);
	if (GM->bCareer)
	{
		Side->AddSlot().AutoHeight()[ Txt(TEXT("BILAN"), 13, CDim, true) ];
		Side->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f))[ Txt(FString::Printf(TEXT("+%d points au championnat"), GM->LastPts), 18, CGold, true) ];
		Side->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(FString::Printf(TEXT("+%.1f M€ de primes (dont %.1f M€ des partenaires)"), GM->LastPrize, GM->LastPartner), 16, CGood, true, true) ];
		for (const FString& L : GM->LastLines) Side->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f))[ Txt(L, 14, CDim, false, true) ];
	}
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[ Title(TEXT("ARRIVÉE"), MyPos > 0 ? FString::Printf(TEXT("Tu termines P%d"), MyPos) : FString(TEXT("Résultats")), Buttons) ]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(GM->bCareer ? 16.f : 0.f, 0.f, 0.f, 0.f))
			[
				SNew(SBox).WidthOverride(GM->bCareer ? 320.f : 0.f).Visibility(GM->bCareer ? EVisibility::Visible : EVisibility::Collapsed)
				[ Card(Side, CCardBg, 16.f) ]
			]
		];
	return Page(Body, FLinearColor(0.01f, 0.012f, 0.025f, 0.86f));
}
