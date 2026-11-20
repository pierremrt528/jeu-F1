// Écrans du week-end en Slate (comme tuning.js et prerace.js du jeu web) : réglages détaillés de la voiture avant les
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
			: T == TCHAR('W') ? FLinearColor(0.3f, 0.6f, 1.f, 1.f) : T == TCHAR('I') ? FLinearColor(0.3f, 0.85f, 0.4f, 1.f) : FLinearColor(1.f, 0.83f, 0.2f, 1.f);
	}

	FString TyreLabel(TCHAR T)
	{
		return T == TCHAR('S') ? TEXT("Tendres") : T == TCHAR('H') ? TEXT("Durs") : T == TCHAR('W') ? TEXT("Pluie") : T == TCHAR('I') ? TEXT("Inters") : TEXT("Mediums");
	}

	// tendres → mediums → durs → intermédiaires → pluie → tendres
	TCHAR NextDry(TCHAR T)
	{
		return T == TCHAR('S') ? TCHAR('M') : T == TCHAR('M') ? TCHAR('H') : T == TCHAR('H') ? TCHAR('I') : T == TCHAR('I') ? TCHAR('W') : TCHAR('S');
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
	GM->EnsureSetup();
	const FGPMCarSetup& S = GM->Setup;
	const FGPMCarSetup& Adv = GM->Advice();
	const int32 Row = FMath::Clamp(GM->SetupRow, 0, FGPMCarSetup::Num - 1);

	// tour estimé : tour de référence × écart du tour simulé avec la voiture de base
	const float Neutral = FMath::Max(1.f, GM->SetupLap(FGPMCarSetup()));
	const float Base = GM->BaseLap() * 0.985f;
	const float Mine = Base * GM->SetupLap(S) / Neutral, Best = Base * GM->SetupLap(Adv) / Neutral;
	const float Delta = Mine - Best;
	const float Pointe = Me ? AGPMCar::VTop * Me->Perf * Me->UpVTop * S.Fx().VTop * 3.6f : 0.f;
	const float Bal = GM->SetupBalance(S);
	const FString BalTxt = Bal < -0.015f ? TEXT("Sous-vireuse") : Bal > 0.015f ? TEXT("Survireuse") : TEXT("Neutre");
	const FLinearColor BalCol = FMath::Abs(Bal) > 0.04f ? CBad : FMath::Abs(Bal) > 0.015f ? CGold : CGood;

	// une ligne par réglage : nom, réglette avec − et +, valeur ; la ligne choisie montre son explication en bas
	auto RowOf = [this, WGM, &S, &Adv, Row](int32 k) -> TSharedRef<SWidget>
	{
		auto Step = [WGM, k](int32 D) { if (WGM.IsValid()) { WGM->Setup.V[k] = FMath::Clamp(WGM->Setup.V[k] + D, 0, 10); WGM->SetupRow = k; } };
		const bool bAdv = S.V[k] == Adv.V[k];
		return Pick(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FGPMCarSetup::Name(k), 15, CText, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(bAdv ? FString(TEXT("comme l'ingénieur")) : FString::Printf(TEXT("ingénieur : %s"), *Adv.Value(k)), 11, bAdv ? CGood : CFaint) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("-"), CNeutral, [Step]() { Step(-1); }, S.V[k] > 0, 34.f, 32.f, 16) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f))
			[
				SNew(SBox).WidthOverride(92.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Txt(S.Value(k), 14, CGold, true) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))[ Bar(S.V[k] / 10.f, CInfo, 5.f) ]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Button(TEXT("+"), CNeutral, [Step]() { Step(1); }, S.V[k] < 10, 34.f, 32.f, 16) ],
			k == Row, [WGM, k]() { if (WGM.IsValid()) WGM->SetupRow = k; });
	};
	struct FGroup { const TCHAR* Name; int32 From, To; };
	static const FGroup Groups[] = { { TEXT("AÉRODYNAMIQUE"), 0, 1 }, { TEXT("TRANSMISSION"), 2, 2 }, { TEXT("FREINS"), 3, 4 }, { TEXT("DIFFÉRENTIEL"), 5, 5 },
		{ TEXT("SUSPENSION"), 6, 8 }, { TEXT("PNEUS"), 9, 10 } };
	TSharedRef<SVerticalBox> ColA = SNew(SVerticalBox), ColB = SNew(SVerticalBox);
	for (int32 g = 0; g < UE_ARRAY_COUNT(Groups); ++g)
	{
		TSharedRef<SVerticalBox> Col = g < 3 ? ColA : ColB;
		Col->AddSlot().AutoHeight().Padding(FMargin(2.f, g == 0 || g == 3 ? 0.f : 6.f, 0.f, 4.f))[ Txt(Groups[g].Name, 11, CDim, true) ];
		for (int32 k = Groups[g].From; k <= Groups[g].To; ++k) Col->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 5.f))[ RowOf(k) ];
	}

	// réglages de base : ailerons et boîte d'un coup ; conseil de l'ingénieur : tous les réglages
	const int32 Pre = GM->PresetOf(S);
	TSharedRef<SHorizontalBox> Presets = SNew(SHorizontalBox);
	for (int32 k = 0; k < 5; ++k)
		Presets->AddSlot().AutoWidth().Padding(FMargin(k ? 5.f : 0.f, 0.f, 0.f, 0.f))
		[ Button(AGPMGameMode::Setups[k].Name, k == Pre ? Acc2 : CNeutral, [WGM, k]() { if (WGM.IsValid()) WGM->ApplyPreset(k); }, true, 0.f, 36.f, 12) ];
	Presets->AddSlot().FillWidth(1.f)[ SNew(SSpacer) ];
	Presets->AddSlot().AutoWidth()
	[ Button(S == Adv ? TEXT("RÉGLAGES DE L'INGÉNIEUR") : TEXT("CONSEIL DE L'INGÉNIEUR"), S == Adv ? CGood : Acc2, [WGM]() { if (WGM.IsValid()) WGM->Setup = WGM->Advice(); }, true, 0.f, 36.f, 12) ];

	UGPMSeasonSubsystem* Ss = SeasonSys();
	const FString Kicker = GM->bCareer && Ss ? FString::Printf(TEXT("GRAND PRIX %d / %d · RÉGLAGES"), Ss->Round() + 1, Ss->NumRounds()) : FString(TEXT("COURSE SOLO · RÉGLAGES"));
	TSharedRef<SWidget> Nav = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[ Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Phase = WGM->bCareer ? AGPMGameMode::EPhase::Season : AGPMGameMode::EPhase::Solo; }, true, 150.f) ]
		+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[ Button(TEXT("SIMULER LES QUALIFS"), Acc2, [WGM]() { if (WGM.IsValid()) WGM->SimQuali(); }, true, 0.f, 56.f, 16) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ Button(TEXT("TOUR CHRONO"), Acc, [WGM]() { if (WGM.IsValid()) WGM->PlayQuali(); }, true, 220.f, 56.f, 19) ];

	const FString DeltaTxt = Delta < 0.0005f ? FString(TEXT("aussi rapide que le conseil")) : FString::Printf(TEXT("+%.3f s sur le conseil"), Delta);
	TSharedRef<SWidget> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			Title(Kicker, T ? T->Name : FString(TEXT("Réglages")),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(TEXT("Météo"), GM->WeatherLabel(), WeatherColor(GM)) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(TEXT("Pointe"), FString::Printf(TEXT("%.0f km/h"), Pointe), CText) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(TEXT("Équilibre"), BalTxt, BalCol) ]
				+ SHorizontalBox::Slot().AutoWidth()[ Chip(TEXT("Tour estimé"), Clock(Mine), Delta < 0.0005f ? CPurple : CGold) ])
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Presets ]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SScrollBox) + SScrollBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ ColA ]
				+ SHorizontalBox::Slot().FillWidth(1.f)[ ColB ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		[
			Card(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)[ Txt(FString::Printf(TEXT("%s · %s / %s"), FGPMCarSetup::Name(Row), FGPMCarSetup::LowLabel(Row), FGPMCarSetup::HighLabel(Row)), 13, CText, true) ]
					+ SHorizontalBox::Slot().AutoWidth()[ Txt(DeltaTxt, 13, Delta < 0.0005f ? CGood : CDim, true) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))[ Txt(FGPMCarSetup::Help(Row), 13, CDim, false, true) ],
				CCardBg, 10.f)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 8.f))[ Txt(TEXT("Flèches haut / bas : choisir un réglage · gauche / droite : le changer · I : conseil de l'ingénieur · Entrée : tour chrono · Parc fermé : les réglages restent pour la course"), 12, CFaint, false, true) ]
		+ SVerticalBox::Slot().AutoHeight()[ Nav ];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.66f)
		[
			SNew(SBorder)
			.BorderImage(GPMUIStyle::Rounded(0.f))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.01f, 0.012f, 0.025f, 0.9f)))
			.Padding(FMargin(30.f, 20.f))
			[
				Left
			]
		]
		+ SHorizontalBox::Slot().FillWidth(0.34f)[ SNew(SSpacer) ];
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
					Button(TEXT("-"), CNeutral, [WGM, i, Other]()
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
	// carburant au départ : plus léger = plus rapide, mais il faut économiser pour finir le relais ; plein à chaque arrêt
	{
		const int32 Fk = FMath::Clamp(GM->FuelK, 0, 4);
		const float Kg = GM->PlayerStartFuel(Fk);
		const float PerLapKg = GM->FuelFor(Me, 1.f);
		const int32 FirstLaps = Mine.Num() > 1 ? Mine[0].Value : Laps;
		static const TCHAR* const FuelHelp[5] = {
			TEXT("Très léger : la voiture est plus vive, mais il faudra rouler en économie pour finir le relais."),
			TEXT("Léger : un peu plus rapide, économie à prévoir en fin de relais."),
			TEXT("Normal : de quoi finir le relais avec 3 % de marge."),
			TEXT("Marge : un peu plus lourd, tu peux attaquer sans compter."),
			TEXT("Plein : réservoir de 110 kg, la voiture la plus lourde.") };
		Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 16.f, 0.f, 6.f))[ Txt(TEXT("CARBURANT AU DÉPART"), 13, CDim, true) ];
		Sumup->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ Button(TEXT("-"), CNeutral, [WGM]() { if (WGM.IsValid() && WGM->FuelK > 0) { --WGM->FuelK; WGM->SaveSettings(); } }, Fk > 0, 46.f, 42.f, 20) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(AGPMGameMode::FuelName(Fk), 17, Fk < 2 ? CGold : CText, true, false, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(PerLapKg > 0.f ? FString::Printf(TEXT("%.0f kg · %.1f tours"), Kg, Kg / PerLapKg) : FString::Printf(TEXT("%.0f kg"), Kg), 13, CDim, false, false, true) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ Button(TEXT("+"), CNeutral, [WGM]() { if (WGM.IsValid() && WGM->FuelK < 4) { ++WGM->FuelK; WGM->SaveSettings(); } }, Fk < 4, 46.f, 42.f, 20) ]
		];
		Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(FuelHelp[Fk], 13, Fk < 2 ? CGold : CDim, false, true) ];
		Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		[
			Txt(Mine.Num() > 1 ? FString::Printf(TEXT("Premier relais : %d tours, ravitaillement à chaque arrêt."), FirstLaps)
				: FString::Printf(TEXT("Sans arrêt : l'essence doit tenir %d tours."), FirstLaps), 12, CFaint, false, true)
		];
	}
	Sumup->AddSlot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))
	[
		Txt(FString::Printf(TEXT("Le muret t'appelle au stand au bon tour. B : arrêt manuel. Chaque arrêt coûte environ %.0f s."), T ? T->PitLoss : 22.f), 13, CFaint, false, true)
	];

	UGPMSeasonSubsystem* S = SeasonSys();
	const FString Kicker = GM->bCareer && S ? FString::Printf(TEXT("GRAND PRIX %d / %d · STRATÉGIE"), S->Round() + 1, S->NumRounds()) : FString(TEXT("COURSE SOLO · STRATÉGIE"));
	const int32 Sk = GM->PresetOf(GM->Setup);
	TSharedRef<SHorizontalBox> Info = SNew(SHorizontalBox);
	auto AddInfo = [&Info](const TSharedRef<SWidget>& W) { Info->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ W ]; };
	AddInfo(Chip(TEXT("Course"), FString::Printf(TEXT("%d tours"), Laps), CText));
	if (GridPos > 0) AddInfo(Chip(TEXT("Départ"), FString::Printf(TEXT("P%d"), GridPos), GridPos <= 3 ? CGold : CText));
	AddInfo(Chip(TEXT("Réglage"), GM->Setup == GM->Advice() ? FString(TEXT("Ingénieur")) : Sk >= 0 ? FString(AGPMGameMode::Setups[Sk].Name) : FString(TEXT("Personnalisé")), CInfo));
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
