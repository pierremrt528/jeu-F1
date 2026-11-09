// Écrans du week-end en Slate (comme tuning.js et prerace.js du jeu web) : réglages de la voiture avant les
// qualifications, puis stratégie de course avant le départ (3 propositions, relais modifiables).
#include "GPMUI.h"
#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
#include "GPMTypes.h"
#include "GPMSeason.h"
#include "Engine/GameInstance.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

using namespace GPMUIColor;

namespace GPMUIWeekendText
{
	FLinearColor TyreColor(TCHAR T)
	{
		return T == TCHAR('S') ? FLinearColor(1.f, 0.27f, 0.25f, 1.f) : T == TCHAR('H') ? FLinearColor(0.9f, 0.91f, 0.94f, 1.f)
			: T == TCHAR('W') ? FLinearColor(0.3f, 0.6f, 1.f, 1.f) : FLinearColor(1.f, 0.83f, 0.2f, 1.f);
	}

	FString TyreLabel(TCHAR T)
	{
		return T == TCHAR('S') ? TEXT("Tendres") : T == TCHAR('H') ? TEXT("Durs") : T == TCHAR('W') ? TEXT("Pluie") : TEXT("Mediums");
	}

	// tendres → mediums → durs → pluie → tendres
	TCHAR NextDry(TCHAR T)
	{
		return T == TCHAR('S') ? TCHAR('M') : T == TCHAR('M') ? TCHAR('H') : T == TCHAR('H') ? TCHAR('W') : TCHAR('S');
	}

	FLinearColor WeatherColor(const AGPMGameMode* GM)
	{
		return GM->bRainStart || GM->WxChangeAt >= 0.f ? FLinearColor(0.35f, 0.65f, 1.f, 1.f) : FLinearColor(1.f, 0.85f, 0.3f, 1.f);
	}
}

// ---------- réglages ----------

TSharedRef<SWidget> FGPMUI::TuningScreen(AGPMGameMode* GM)
{
	using namespace GPMUIWeekendText;
	const FGPMTrackData* T = GM->Circuit();
	AGPMHUD* H = Hud.Get();
	const AGPMCar* Me = H ? Cast<AGPMCar>(H->GetOwningPawn()) : nullptr;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	const int32 Best = GM->BestSetup();
	const int32 Sel = GM->SetupK >= 0 ? FMath::Clamp(GM->SetupK, 0, 4) : Best;

	// tour estimé avec chaque réglage (tour de référence × écart du tour simulé)
	float Sim[5];
	for (int32 k = 0; k < 5; ++k) Sim[k] = GM->SetupLap(k);
	const float Ref = FMath::Max(1.f, Sim[2]), Base = GM->BaseLap() * 0.985f;
	float Est[5], Fastest = 1e9f;
	for (int32 k = 0; k < 5; ++k) { Est[k] = Base * Sim[k] / Ref; Fastest = FMath::Min(Fastest, Est[k]); }

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (int32 k = 0; k < 5; ++k)
	{
		const AGPMGameMode::FSetup& St = AGPMGameMode::Setups[k];
		const bool bBest = k == Best;
		const float Pointe = Me ? AGPMCar::VTop * Me->Perf * Me->UpVTop * (1.f + St.VTop) * 3.6f : 0.f;
		TSharedRef<SHorizontalBox> NameRow = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Txt(St.Name, 19, CText, true) ];
		if (bBest)
			NameRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 0.f, 0.f))
			[
				SNew(SBorder).BorderImage(GPMUIStyle::Rounded(40.f)).BorderBackgroundColor(FSlateColor(CGood)).Padding(FMargin(10.f, 2.f))
				[ Txt(TEXT("CONSEIL DE L'INGÉNIEUR"), 11, InkOn(CGood), true) ]
			];
		const float Delta = Est[k] - Fastest;
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
		[
			Pick(SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ NameRow ]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))[ Txt(St.Desc, 14, CDim, false, true) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(16.f, 0.f))
				[
					SNew(SBox).WidthOverride(170.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ Txt(TEXT("Appui"), 12, CFaint) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 6.f))[ Bar(0.5f + St.Kd / 0.1f, CInfo, 6.f) ]
						+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Pointe %.0f km/h"), Pointe), 12, CFaint) ]
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))[ Bar(0.5f + St.VTop / 0.08f, CGold, 6.f) ]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(120.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ Txt(Clock(Est[k]), 18, Delta < 0.0005f ? CPurple : CText, true) ]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ Txt(Delta < 0.0005f ? FString(TEXT("le plus rapide")) : FString::Printf(TEXT("+%.3f"), Delta), 13, Delta < 0.0005f ? CGood : CDim) ]
					]
				],
				k == Sel, [WGM, k]() { if (WGM.IsValid()) WGM->SetupK = k; })
		];
	}

	UGPMSeasonSubsystem* S = SeasonSys();
	const FString Kicker = GM->bCareer && S ? FString::Printf(TEXT("GRAND PRIX %d / %d · RÉGLAGES"), S->Round() + 1, S->NumRounds()) : FString(TEXT("COURSE SOLO · RÉGLAGES"));
	TSharedRef<SWidget> Nav = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[ Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Phase = WGM->bCareer ? AGPMGameMode::EPhase::Season : AGPMGameMode::EPhase::Solo; }, true, 150.f) ]
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[ Button(TEXT("SIMULER LES QUALIFS"), Acc2, [WGM, Sel]() { if (WGM.IsValid()) WGM->SimQuali(Sel); }, true, 0.f, 56.f, 16) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ Button(TEXT("TOUR CHRONO"), Acc, [WGM, Sel]() { if (WGM.IsValid()) WGM->PlayQuali(Sel); }, true, 220.f, 56.f, 19) ];

	TSharedRef<SWidget> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
		[
			Title(Kicker, T ? T->Name : FString(TEXT("Réglages")),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(TEXT("Météo"), GM->WeatherLabel(), WeatherColor(GM)) ]
				+ SHorizontalBox::Slot().AutoWidth()[ Chip(TEXT("Tour estimé"), Clock(Est[Sel]), CGold) ])
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			Txt(FString::Printf(TEXT("Plus d'appui = plus vite dans les virages, plus de vitesse = plus vite en ligne droite. Sur ce circuit, l'ingénieur conseille « %s ». Parc fermé : le réglage choisi reste pour la course."), AGPMGameMode::Setups[Best].Name), 15, CDim, false, true)
		]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 10.f))[ Txt(TEXT("Flèches : changer de réglage · Entrée : tour chrono · Échap : retour"), 13, CFaint) ]
		+ SVerticalBox::Slot().AutoHeight()[ Nav ];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.62f)
		[
			SNew(SBorder)
			.BorderImage(GPMUIStyle::Rounded(0.f))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.01f, 0.012f, 0.025f, 0.9f)))
			.Padding(FMargin(34.f, 24.f))
			[
				Left
			]
		]
		+ SHorizontalBox::Slot().FillWidth(0.38f)[ SNew(SSpacer) ];
}

// ---------- stratégie ----------

TSharedRef<SWidget> FGPMUI::PreraceScreen(AGPMGameMode* GM)
{
	using namespace GPMUIWeekendText;
	const FGPMTrackData* T = GM->Circuit();
	AGPMHUD* H = Hud.Get();
	AGPMCar* Me = H ? Cast<AGPMCar>(H->GetOwningPawn()) : nullptr;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	const int32 Laps = GM->LapsFor();
	const float LapT = GM->BaseLap();
	const float BestT = GM->Plans.Num() ? GM->Plans[0].Time : 0.f;
	const TArray<TPair<TCHAR, int32>>& Mine = GM->Stints;
	float MyWear = 0.f;
	const float MyT = Mine.Num() ? GM->PlanTime(Mine, LapT, &MyWear) : 0.f;
	int32 Sum = 0;
	for (const TPair<TCHAR, int32>& Sx : Mine) Sum += Sx.Value;
	const int32 GridPos = Me && GM->QOrder.Num() ? GM->QOrder.IndexOfByKey(Me) + 1 : 0;

	// frise d'une stratégie : un bloc par relais, largeur selon le nombre de tours
	auto StintBar = [this](const TArray<TPair<TCHAR, int32>>& St) -> TSharedRef<SWidget>
	{
		TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
		for (int32 i = 0; i < St.Num(); ++i)
		{
			const FLinearColor Col = TyreColor(St[i].Key);
			Box->AddSlot().FillWidth(float(FMath::Max(1, St[i].Value))).Padding(FMargin(0.f, 0.f, i < St.Num() - 1 ? 3.f : 0.f, 0.f))
			[
				SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(Col)).HAlign(HAlign_Center).Padding(FMargin(4.f, 3.f))
				[ Txt(FString::Chr(St[i].Key) + FString::Printf(TEXT(" %d"), St[i].Value), 13, InkOn(Col), true) ]
			];
		}
		return Box;
	};

	// les 3 stratégies proposées
	TSharedRef<SHorizontalBox> PlanRow = SNew(SHorizontalBox);
	for (int32 k = 0; k < GM->Plans.Num(); ++k)
	{
		const AGPMGameMode::FPlan& P = GM->Plans[k];
		const TArray<TPair<TCHAR, int32>> Copy = P.Stints;
		const int32 NStops = P.Stints.Num() - 1;
		PlanRow->AddSlot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, k < GM->Plans.Num() - 1 ? 10.f : 0.f, 0.f))
		[
			Pick(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[ Txt(P.Name, 17, k == 0 ? CGold : CText, true) ]
					+ SHorizontalBox::Slot().AutoWidth()[ Txt(k == 0 ? FString(TEXT("la plus rapide")) : FString::Printf(TEXT("+%.1f s"), P.Time - BestT), 14, k == 0 ? CGood : CDim, true) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f))[ StintBar(P.Stints) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(NStops == 0 ? FString(TEXT("Sans arrêt")) : FString::Printf(TEXT("%d arrêt%s"), NStops, NStops > 1 ? TEXT("s") : TEXT("")), 14, CDim, true) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))[ Txt(P.Desc, 13, CFaint, false, true) ],
				P.Stints == Mine, [WGM, Copy]() { if (WGM.IsValid()) WGM->Stints = Copy; })
		];
	}

	// éditeur : gomme et nombre de tours de chaque relais, ajout ou retrait d'un arrêt
	TSharedRef<SVerticalBox> Editor = SNew(SVerticalBox);
	const int32 NStint = Mine.Num();
	for (int32 i = 0; i < NStint; ++i)
	{
		const TCHAR Ty = Mine[i].Key;
		const int32 L = Mine[i].Value;
		const float Wear = L * AGPMCar::TyreWpl(Ty) * 1.05f;
		const FLinearColor WearCol = Wear > 0.85f ? CBad : Wear > 0.6f ? CGold : CGood;
		const int32 Other = i == NStint - 1 ? i - 1 : NStint - 1;   // les tours ajoutés ici sont pris au dernier relais (ou à l'avant-dernier)
		const bool bCanMore = NStint > 1 && Mine.IsValidIndex(Other) && Mine[Other].Value > 1;
		const bool bCanLess = NStint > 1 && L > 1;
		Editor->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
		[
			SNew(SBorder).BorderImage(GPMUIStyle::Rounded(10.f)).BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.045f))).Padding(FMargin(12.f, 8.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[ SNew(SBox).WidthOverride(90.f)[ Txt(FString::Printf(TEXT("Relais %d"), i + 1), 15, CDim, true) ] ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
				[
					Button(TyreLabel(Ty), TyreColor(Ty), [WGM, i]()
					{
						if (WGM.IsValid() && WGM->Stints.IsValidIndex(i)) WGM->Stints[i].Key = NextDry(WGM->Stints[i].Key);
					}, true, 130.f, 42.f, 15)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					Button(TEXT("−"), CNeutral, [WGM, i, Other]()
					{
						if (!WGM.IsValid() || !WGM->Stints.IsValidIndex(i) || !WGM->Stints.IsValidIndex(Other) || WGM->Stints[i].Value <= 1) return;
						--WGM->Stints[i].Value; ++WGM->Stints[Other].Value;
					}, bCanLess, 46.f, 42.f, 20)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f))
				[ SNew(SBox).WidthOverride(80.f).HAlign(HAlign_Center)[ Txt(FString::Printf(TEXT("%d tours"), L), 16, CText, true) ] ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
				[
					Button(TEXT("+"), CNeutral, [WGM, i, Other]()
					{
						if (!WGM.IsValid() || !WGM->Stints.IsValidIndex(i) || !WGM->Stints.IsValidIndex(Other) || WGM->Stints[Other].Value <= 1) return;
						++WGM->Stints[i].Value; --WGM->Stints[Other].Value;
					}, bCanMore, 46.f, 42.f, 20)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("usure à la fin du relais : %d %%"), FMath::RoundToInt(FMath::Min(Wear, 1.f) * 100.f)), 13, WearCol) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))[ Bar(Wear, WearCol, 6.f) ]
				]
			]
		];
	}
	TSharedRef<SHorizontalBox> StopBtns = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			Button(TEXT("+ ARRÊT"), Acc2, [WGM]()
			{
				if (!WGM.IsValid() || WGM->Stints.Num() == 0 || WGM->Stints.Num() >= 4) return;
				TPair<TCHAR, int32>& Last = WGM->Stints.Last();
				if (Last.Value < 2) return;
				const int32 Half = Last.Value / 2;
				Last.Value -= Half;
				WGM->Stints.Emplace(TCHAR('M'), Half);
			}, NStint > 0 && NStint < 4 && Mine.Last().Value >= 2, 0.f, 42.f, 14)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			Button(TEXT("− ARRÊT"), CNeutral, [WGM]()
			{
				if (!WGM.IsValid() || WGM->Stints.Num() < 2) return;
				const int32 Gone = WGM->Stints.Last().Value;
				WGM->Stints.Pop();
				WGM->Stints.Last().Value += Gone;
			}, NStint > 1, 0.f, 42.f, 14)
		];

	// bilan de la stratégie choisie, alertes
	TSharedRef<SVerticalBox> Sumup = SNew(SVerticalBox);
	Sumup->AddSlot().AutoHeight()[ Txt(TEXT("TA STRATÉGIE"), 13, CDim, true) ];
	Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 4.f))[ StintBar(Mine) ];
	Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(FString::Printf(TEXT("Temps estimé : %s"), *Clock(MyT)), 17, CText, true) ];
	if (BestT > 0.f && MyT > BestT + 0.05f)
		Sumup->AddSlot().AutoHeight()[ Txt(FString::Printf(TEXT("+%.1f s sur la stratégie recommandée"), MyT - BestT), 14, CGold) ];
	else
		Sumup->AddSlot().AutoHeight()[ Txt(TEXT("Aussi rapide que la recommandée"), 14, CGood) ];
	if (MyWear > 0.85f)
		Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))[ Txt(TEXT("Attention : un relais use les pneus jusqu'à la corde, risque de crevaison."), 14, CBad, true, true) ];
	else if (MyWear > 0.6f)
		Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))[ Txt(TEXT("Un relais finit sous 40 % de pneus : le grip chute en fin de relais."), 14, CGold, false, true) ];
	if (Sum != Laps)
		Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))[ Txt(FString::Printf(TEXT("Les relais font %d tours pour une course de %d tours."), Sum, Laps), 14, CBad, true, true) ];
	Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
	[
		Txt(FString::Printf(TEXT("Le muret t'appelle au stand au bon tour. B : arrêt manuel. Chaque arrêt coûte environ %.0f s."), T ? T->PitLoss : 22.f), 13, CFaint, false, true)
	];

	UGPMSeasonSubsystem* S = SeasonSys();
	const FString Kicker = GM->bCareer && S ? FString::Printf(TEXT("GRAND PRIX %d / %d · STRATÉGIE"), S->Round() + 1, S->NumRounds()) : FString(TEXT("COURSE SOLO · STRATÉGIE"));
	const int32 Sk = GM->SetupK >= 0 ? FMath::Clamp(GM->SetupK, 0, 4) : 2;
	TSharedRef<SHorizontalBox> Info = SNew(SHorizontalBox);
	auto AddInfo = [&Info](const TSharedRef<SWidget>& W) { Info->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ W ]; };
	AddInfo(Chip(TEXT("Course"), FString::Printf(TEXT("%d tours"), Laps), CText));
	if (GridPos > 0) AddInfo(Chip(TEXT("Départ"), FString::Printf(TEXT("P%d"), GridPos), GridPos <= 3 ? CGold : CText));
	AddInfo(Chip(TEXT("Réglage"), AGPMGameMode::Setups[Sk].Name, CInfo));
	AddInfo(Chip(TEXT("Arrêt"), FString::Printf(TEXT("~%.0f s"), T ? T->PitLoss : 22.f), CDim));
	AddInfo(Chip(TEXT("Météo"), GM->WeatherLabel(), WeatherColor(GM)));

	TSharedRef<SWidget> Nav = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[ Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Phase = WGM->bCareer ? AGPMGameMode::EPhase::Season : AGPMGameMode::EPhase::Solo; }, true, 150.f) ]
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[ Button(TEXT("SIMULER LA COURSE"), Acc2, [WGM]() { if (WGM.IsValid()) WGM->Go(true); }, Sum == Laps, 0.f, 56.f, 16) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ Button(TEXT("DÉPART"), Acc, [WGM]() { if (WGM.IsValid()) WGM->Go(false); }, Sum == Laps, 220.f, 56.f, 20) ];

	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
		[ Title(Kicker, T ? T->Name : FString(TEXT("Stratégie")), Info) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(TEXT("STRATÉGIES PROPOSÉES PAR LE MURET"), 13, CDim, true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))[ PlanRow ]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(TEXT("MODIFIER LES RELAIS"), 13, CDim, true) ]
					+ SVerticalBox::Slot().AutoHeight()[ Editor ]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[ StopBtns ]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(16.f, 0.f, 0.f, 0.f))
			[ SNew(SBox).WidthOverride(340.f)[ Card(Sumup, CCardBg, 16.f) ] ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 10.f))[ Txt(TEXT("1 à 3 : choisir une stratégie · Entrée : départ · Échap : retour"), 13, CFaint) ]
		+ SVerticalBox::Slot().AutoHeight()[ Nav ];
	return SNew(SBox).HAlign(HAlign_Center)[ SNew(SBox).WidthOverride(1240.f)[ Page(Body, FLinearColor(0.01f, 0.012f, 0.025f, 0.88f)) ] ];
}
