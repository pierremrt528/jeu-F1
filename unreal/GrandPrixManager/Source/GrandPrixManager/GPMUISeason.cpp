// Écran de saison en Slate : onglets des classements, du calendrier par mois (calendarHTML de season.js) et des courbes
// de performance (perfChartHTML), carte de la mission du sponsor (missionHTML d'events.js).
#include "GPMUI.h"
#include "GPMHUD.h"
#include "GPMCar.h"
#include "GPMGameMode.h"
#include "GPMSeason.h"
#include "GPMCareer.h"
#include "GPMFactory.h"
#include "GPMDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonValue.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

using namespace GPMUIColor;

namespace GPMUISeasonCal
{
	const TCHAR* const MoisCourt[12] = { TEXT("janv."), TEXT("févr."), TEXT("mars"), TEXT("avril"), TEXT("mai"), TEXT("juin"), TEXT("juil."), TEXT("août"),
		TEXT("sept."), TEXT("oct."), TEXT("nov."), TEXT("déc.") };
	const TCHAR* const MoisLong[12] = { TEXT("Janvier"), TEXT("Février"), TEXT("Mars"), TEXT("Avril"), TEXT("Mai"), TEXT("Juin"), TEXT("Juillet"), TEXT("Août"),
		TEXT("Septembre"), TEXT("Octobre"), TEXT("Novembre"), TEXT("Décembre") };

	/** Premier Grand Prix : le 2e dimanche de mars (raceDate de season.js). */
	FDateTime FirstRace(int32 Year)
	{
		const FDateTime D(Year, 3, 8);
		const int32 JsDay = (int32(D.GetDayOfWeek()) + 1) % 7;   // 0 = dimanche, comme getDay() en JavaScript
		return D + FTimespan::FromDays(double((7 - JsDay) % 7));
	}

	FString FmtDate(const FDateTime& D)
	{
		const FString Day = D.GetDay() == 1 ? FString(TEXT("1er")) : FString::FromInt(D.GetDay());
		return Day + TEXT(" ") + MoisCourt[FMath::Clamp(D.GetMonth(), 1, 12) - 1];
	}

	/** Couleur de la place dans les classements : or, argent, bronze, puis blanc. */
	FLinearColor PodiumCol(int32 Idx)
	{
		return Idx == 0 ? FLinearColor(1.f, 0.8f, 0.22f, 1.f) : Idx == 1 ? FLinearColor(0.8f, 0.83f, 0.9f, 1.f) : Idx == 2 ? FLinearColor(0.88f, 0.56f, 0.32f, 1.f) : FLinearColor(0.95f, 0.96f, 0.98f, 1.f);
	}

	/** Date d'une manche : premier GP puis les écarts du calendrier (GAPS de season.js), comme CalendarPanel. */
	FDateTime RaceDay(UGPMDataSubsystem* Db, int32 Year, int32 Rnd)
	{
		TArray<int32> Gaps;
		if (Db)
			if (const TSharedPtr<FJsonValue> Raw = Db->GetRaw(TEXT("season"), TEXT("GAPS")))
				for (const TSharedPtr<FJsonValue>& V : Raw->AsArray()) Gaps.Add(FMath::Max(1, int32(V->AsNumber())));
		FDateTime When = FirstRace(Year);
		for (int32 k = 0; k < Rnd; ++k) When += FTimespan::FromDays(double(Gaps.IsValidIndex(k) ? Gaps[k] : 14));
		return When;
	}
}

/** Tracé d'un circuit vu de dessus : halo de la couleur de l'écurie, trait blanc, point de la ligne de départ. */
class SGPMTrackMap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGPMTrackMap) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}

	TArray<FVector2f> MapPts;   // en pixels pour MapSize, remis à l'échelle à l'affichage
	FLinearColor MapCol = FLinearColor::White;
	FVector2D MapSize = FVector2D(200.0, 130.0);

	virtual FVector2D ComputeDesiredSize(float) const override { return MapSize; }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		if (MapPts.Num() < 3) return LayerId;
		const FVector2f Got = FVector2f(AllottedGeometry.GetLocalSize());
		const float K = FMath::Min(Got.X / float(MapSize.X), Got.Y / float(MapSize.Y));
		const FVector2f Off((Got.X - float(MapSize.X) * K) * 0.5f, (Got.Y - float(MapSize.Y) * K) * 0.5f);
		TArray<FVector2f> Halo, Line;
		for (const FVector2f& Q : MapPts) { Halo.Add(Off + Q * K); Line.Add(Off + Q * K); }
		const FVector2f Start = Line[0];
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Halo), ESlateDrawEffect::None,
			FLinearColor(MapCol.R, MapCol.G, MapCol.B, 0.32f), true, 7.f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Line), ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, 0.95f), true, 2.2f);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2f(10.f, 10.f), FSlateLayoutTransform(Start - FVector2f(5.f, 5.f))),
			GPMUIStyle::Rounded(40.f), ESlateDrawEffect::None, MapCol);
		return LayerId + 3;
	}
};

/** Courbes de performance des écuries : grille, valeurs ×100 à gauche, manches en bas, le joueur en épais par-dessus. */
class SGPMPerfChart : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGPMPerfChart) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}

	TArray<TArray<float>> Series;     // une courbe par écurie : départ puis après chaque GP
	TArray<FLinearColor> Colors;
	int32 MeIdx = 0, NX = 1;
	float Lo = 0.9f, Hi = 1.f;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(520.0, 300.0); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const float GW = float(AllottedGeometry.GetLocalSize().X), GH = float(AllottedGeometry.GetLocalSize().Y);
		const float Left = 50.f, Top = 8.f, RightPad = 12.f, Bottom = 24.f;
		const float PW = FMath::Max(10.f, GW - Left - RightPad), PH = FMath::Max(10.f, GH - Top - Bottom);
		const float Span = FMath::Max(1e-5f, Hi - Lo);
		const int32 Cols = FMath::Max(1, NX);
		auto PX = [Left, PW, Cols](float I) { return Left + PW * I / float(Cols); };
		auto PY = [this, Top, PH, Span](float Val) { return Top + PH * (1.f - (Val - Lo) / Span); };
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
		const FLinearColor Ink(0.62f, 0.65f, 0.72f, 1.f);
		// grille et valeurs
		for (int32 k = 0; k <= 4; ++k)
		{
			const float Val = Lo + Span * k / 4.f, Yv = PY(Val);
			TArray<FVector2f> Grid;
			Grid.Add(FVector2f(Left, Yv));
			Grid.Add(FVector2f(GW - RightPad, Yv));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Grid), ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.08f), true, 1.f);
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(Left - 6.f, 16.f), FSlateLayoutTransform(FVector2f(2.f, Yv - 8.f))),
				FString::Printf(TEXT("%.1f"), Val * 100.f), Font, ESlateDrawEffect::None, Ink);
		}
		// manches : D = départ de la saison
		const int32 Step = Cols > 12 ? 2 : 1;
		for (int32 i = 0; i <= Cols; i += Step)
			FSlateDrawElement::MakeText(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2f(26.f, 16.f), FSlateLayoutTransform(FVector2f(PX(float(i)) - 5.f, GH - Bottom + 6.f))),
				i ? FString::FromInt(i) : FString(TEXT("D")), Font, ESlateDrawEffect::None, Ink);
		// courbes : les rivaux d'abord, le joueur par-dessus
		for (int32 Pass = 0; Pass < 2; ++Pass)
			for (int32 t = 0; t < Series.Num(); ++t)
			{
				if ((t == MeIdx) != (Pass == 1) || Series[t].Num() == 0) continue;
				TArray<FVector2f> Pts;
				for (int32 i = 0; i < Series[t].Num(); ++i) Pts.Add(FVector2f(PX(float(i)), PY(Series[t][i])));
				if (Pts.Num() == 1) Pts.Add(FVector2f(Pts[0].X + 2.f, Pts[0].Y));
				const FLinearColor Col = Colors.IsValidIndex(t) ? Colors[t] : FLinearColor::White;
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), MoveTemp(Pts), ESlateDrawEffect::None,
					Pass == 1 ? Col : FLinearColor(Col.R, Col.G, Col.B, 0.8f), true, Pass == 1 ? 3.2f : 1.4f);
			}
		// point blanc sur le dernier relevé du joueur
		if (Series.IsValidIndex(MeIdx) && Series[MeIdx].Num())
		{
			const int32 Last = Series[MeIdx].Num() - 1;
			const float Dx = PX(float(Last)), Dy = PY(Series[MeIdx][Last]);
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2f(10.f, 10.f), FSlateLayoutTransform(FVector2f(Dx - 5.f, Dy - 5.f))),
				GPMUIStyle::Rounded(40.f), ESlateDrawEffect::None, FLinearColor::White);
		}
		return LayerId + 3;
	}
};

// ---------- tracé d'un circuit et carte du prochain Grand Prix ----------

TSharedRef<SWidget> FGPMUI::TrackMap(const FString& CircuitId, const FLinearColor& Col, float W, float H)
{
	UGPMDataSubsystem* Db = DataSys();
	const FGPMTrackData* T = Db && !CircuitId.IsEmpty() ? Db->GetTrack(CircuitId) : nullptr;
	TSharedRef<SGPMTrackMap> Map = SNew(SGPMTrackMap);
	Map->MapSize = FVector2D(W, H);
	Map->MapCol = Col;
	const int32 N = T ? FMath::Min(T->WX.Num(), T->WZ.Num()) : 0;
	if (N > 8)
	{
		// repère du jeu web, comme la mini-carte de la course : x vers la droite, z vers le bas
		const int32 Step = FMath::Max(1, N / 240);
		float MinX = 1e9f, MaxX = -1e9f, MinZ = 1e9f, MaxZ = -1e9f;
		for (int32 i = 0; i < N; i += Step)
		{
			MinX = FMath::Min(MinX, T->WX[i]); MaxX = FMath::Max(MaxX, T->WX[i]);
			MinZ = FMath::Min(MinZ, T->WZ[i]); MaxZ = FMath::Max(MaxZ, T->WZ[i]);
		}
		const float Pad = 8.f;
		const float K = FMath::Min((W - 2.f * Pad) / FMath::Max(1.f, MaxX - MinX), (H - 2.f * Pad) / FMath::Max(1.f, MaxZ - MinZ));
		const float Ox = (W - (MaxX - MinX) * K) * 0.5f, Oz = (H - (MaxZ - MinZ) * K) * 0.5f;
		for (int32 i = 0; i < N; i += Step) Map->MapPts.Add(FVector2f(Ox + (T->WX[i] - MinX) * K, Oz + (T->WZ[i] - MinZ) * K));
		Map->MapPts.Add(Map->MapPts[0]);
	}
	return Map;
}

TSharedRef<SWidget> FGPMUI::HeroNext(AGPMGameMode* GM)
{
	UGPMSeasonSubsystem* S = SeasonSys();
	UGPMDataSubsystem* Db = DataSys();
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Tag = [this](const FString& Label) -> TSharedRef<SWidget>
	{
		return SNew(SBorder).BorderImage(GPMUIStyle::Slant()).BorderBackgroundColor(FSlateColor(Acc)).Padding(FMargin(20.f, 3.f, 22.f, 3.f))
			[ Ital(Label, 14, InkOn(Acc), 0) ];
	};
	auto Kicker = [](const FString& Label, const FLinearColor& Col) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock).Text(FText::FromString(Label)).Font(GPMUIStyle::Face(2, 12)).ColorAndOpacity(FSlateColor(Col));
	};
	if (S && !S->IsOver())
	{
		const FString Id = S->CurrentCircuit();
		const FGPMTrackData* T = Db ? Db->GetTrack(Id) : nullptr;
		const FDateTime Day = GPMUISeasonCal::RaceDay(Db, S->Year(), S->Round());
		const bool bHere = GM->Circuit() == T;
		FString Info = T ? FString::Printf(TEXT("%s%s%.2f km  ·  largeur %.0f m"), *T->Type, T->Type.IsEmpty() ? TEXT("") : TEXT("  ·  "), T->WX.Num() * 2.f / 1000.f, T->Width) : FString();
		if (bHere) Info += FString::Printf(TEXT("  ·  météo : %s"), *GM->WeatherLabel().ToLower());
		// manche, date, nom du circuit et son tracé
		Box->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Tag(FString::Printf(TEXT("MANCHE %02d"), S->Round() + 1)) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
					[ Kicker(FString::Printf(TEXT("%s  ·  SUR %d"), *GPMUISeasonCal::FmtDate(Day).ToUpper(), S->NumRounds()), CDim) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 0.f))
				[
					SNew(STextBlock).Text(FText::FromString((T ? T->Name : Id).ToUpper())).Font(GPMUIStyle::Face(0, 34)).ColorAndOpacity(FSlateColor(CText)).AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))
				[
					Txt(Info, 14, GM->bRainStart || GM->WxChangeAt >= 0.f ? CInfo : CDim, false, true)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(14.f, 0.f, 0.f, 0.f))[ TrackMap(T ? Id : FString(), Bright(), 160.f, 112.f) ]
		];
		// format de course : Sprint (~11 min) ou Long (~22 min)
		const int32 NLaps = bHere ? GM->LapsFor() : 0;
		Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 14.f, 0.f, 12.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))[ Kicker(TEXT("FORMAT"), CDim) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ Tabs({ TEXT("Sprint · 11 min"), TEXT("Long · 22 min") }, GM->bLongRace ? 1 : 0, [WGM](int32 k) { if (WGM.IsValid() && !WGM->bGridReady) { WGM->bLongRace = k == 1; WGM->SaveSettings(); } }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.f, 0.f, 0.f, 0.f))
			[ Ital(NLaps > 0 ? FString::Printf(TEXT("%d TOURS"), NLaps) : FString(), 16, CText, 1) ]
		];
		if (!GM->bGridReady)
		{
			Box->AddSlot().AutoHeight()[ Button(TEXT("RÉGLAGES ET QUALIFICATIONS"), Acc, [WGM]() { if (WGM.IsValid()) WGM->OpenTuning(); }, true, 0.f, 60.f, 20) ];
			Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))[ Button(TEXT("SIMULER LE GRAND PRIX"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->SimGrandPrix(); }, true, 0.f, 44.f, 15) ];
			Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(TEXT("Entrée : réglages · le Grand Prix simulé saute les qualifications et la course"), 12, CFaint, false, true) ];
		}
		else
		{
			AGPMHUD* HudP = Hud.Get();
			AGPMCar* MeCar = HudP ? Cast<AGPMCar>(HudP->GetOwningPawn()) : nullptr;
			const int32 MyGrid = MeCar ? GM->QOrder.IndexOfByKey(MeCar) : INDEX_NONE;
			Box->AddSlot().AutoHeight()[ Button(TEXT("STRATÉGIE ET DÉPART"), Acc, [WGM]() { if (WGM.IsValid()) WGM->OpenPrerace(); }, true, 0.f, 60.f, 20) ];
			Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))[ Button(TEXT("SIMULER LA COURSE"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Go(true); }, true, 0.f, 44.f, 15) ];
			Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))[ Txt(MyGrid >= 0 ? FString::Printf(TEXT("Qualifications faites : tu pars en P%d"), MyGrid + 1) : FString(TEXT("Qualifications faites")), 12, CFaint) ];
		}
	}
	else
	{
		Box->AddSlot().AutoHeight()[ SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[ Tag(TEXT("SAISON TERMINÉE")) ] ];
		Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 14.f))[ Ital(TEXT("PLACE À L'INTERSAISON"), 30, CText) ];
		Box->AddSlot().AutoHeight()[ Button(TEXT("SAISON SUIVANTE"), Acc, [WGM]() { if (WGM.IsValid()) WGM->Continue(); }, true, 0.f, 60.f, 22) ];
	}
	return SNew(SOverlay)
		+ SOverlay::Slot()[ Layer(GPMUIStyle::Rounded(0.f), FLinearColor(0.018f, 0.022f, 0.042f, 0.88f)) ]
		+ SOverlay::Slot()[ Layer(GPMUIStyle::Ramp(0), FLinearColor(Acc.R, Acc.G, Acc.B, 0.3f)) ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top)[ SNew(SBox).WidthOverride(300.f).HeightOverride(220.f)[ Layer(GPMUIStyle::Glow(), FLinearColor(Acc.R, Acc.G, Acc.B, 0.22f)) ] ]
		+ SOverlay::Slot().VAlign(VAlign_Top)[ SNew(SBox).HeightOverride(3.f)[ Layer(GPMUIStyle::Ramp(0), Acc) ] ]
		+ SOverlay::Slot().Padding(FMargin(22.f, 18.f, 22.f, 18.f))[ Box ];
}

// ---------- classements ----------

TSharedRef<SWidget> FGPMUI::StandingsPanel()
{
	UGPMSeasonSubsystem* S = SeasonSys();
	if (!S) return SNullWidget::NullWidget;
	const TArray<FGPMStanding> Drv = S->Drivers(), Tms = S->Teams();
	TSharedRef<SVerticalBox> DrvList = SNew(SVerticalBox);
	for (int32 k = 0; k < Drv.Num(); ++k)
	{
		TArray<TPair<FString, float>> Cells;
		Cells.Emplace(FString::FromInt(k + 1), 34.f);
		Cells.Emplace(Nm(Drv[k].Name), 0.f);
		Cells.Emplace(Drv[k].Team, 170.f);
		Cells.Emplace(FString::Printf(TEXT("%d"), Drv[k].Pts), 50.f);
		DrvList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 3.f))[ Row(Drv[k].Color, Drv[k].bPlayer, Cells, { GPMUISeasonCal::PodiumCol(k), CText, CDim, CText }) ];
	}
	TSharedRef<SVerticalBox> TeamList = SNew(SVerticalBox);
	for (int32 k = 0; k < Tms.Num(); ++k)
	{
		TArray<TPair<FString, float>> Cells;
		Cells.Emplace(FString::FromInt(k + 1), 34.f);
		Cells.Emplace(Tms[k].Name, 0.f);
		Cells.Emplace(FString::Printf(TEXT("%d"), Tms[k].Pts), 50.f);
		TeamList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 3.f))[ Row(Tms[k].Color, Tms[k].bPlayer, Cells, { GPMUISeasonCal::PodiumCol(k), CText, CText }) ];
	}
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.15f).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
		[
			Card(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[ Txt(TEXT("PILOTES"), 14, CDim, true) ]
				+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ DrvList ] ],
				CCardBg, 14.f)
		]
		+ SHorizontalBox::Slot().FillWidth(0.8f)
		[
			Card(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[ Txt(TEXT("ÉCURIES"), 14, CDim, true) ]
				+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ TeamList ] ],
				CCardBg, 14.f)
		];
}

// ---------- calendrier par mois : Grands Prix datés et rendez-vous de la saison ----------

TSharedRef<SWidget> FGPMUI::CalendarPanel()
{
	UGPMSeasonSubsystem* S = SeasonSys();
	UGPMDataSubsystem* Data = DataSys();
	const UGPMSaveGame* G = S ? S->State() : nullptr;
	if (!G || !Data || S->NumRounds() == 0) return SNullWidget::NullWidget;
	const int32 N = S->NumRounds(), Yr = S->Year();
	TArray<int32> Gaps;
	if (const TSharedPtr<FJsonValue> Raw = Data->GetRaw(TEXT("season"), TEXT("GAPS")))
		for (const TSharedPtr<FJsonValue>& V : Raw->AsArray()) Gaps.Add(FMath::Max(1, int32(V->AsNumber())));
	while (Gaps.Num() < N) Gaps.Add(14);
	TArray<FDateTime> Dates;
	FDateTime When = GPMUISeasonCal::FirstRace(Yr);
	for (int32 k = 0; k < N; ++k)
	{
		if (k) When += FTimespan::FromDays(double(Gaps[k - 1]));
		Dates.Add(When);
	}

	struct FItem { FDateTime At; int32 K = -1; FString Head, Sub; };
	TArray<FItem> Items;
	for (int32 k = 0; k < N; ++k) { FItem It; It.At = Dates[k]; It.K = k; Items.Add(It); }
	auto AddEvent = [&Items](const FDateTime& At, const FString& Head, const FString& Sub) { FItem It; It.At = At; It.Head = Head; It.Sub = Sub; Items.Add(It); };
	const FDateTime First = Dates[0], Last = Dates[N - 1];
	const int32 Mid = N / 2;
	AddEvent(FDateTime(Yr, 1, 1), TEXT("Début de la saison"), TEXT("premières pièces, recrutements, budget de l'année"));
	AddEvent(First - FTimespan::FromDays(30.0), TEXT("Présentation de la voiture"), TEXT("nouvelle livrée devant la presse"));
	AddEvent(First - FTimespan::FromDays(16.0), TEXT("Essais hivernaux"), TEXT("3 jours de roulage avant le premier Grand Prix"));
	if (Mid >= 1 && Mid < N)
		AddEvent(Dates[Mid - 1] + FTimespan::FromDays(1.0), TEXT("Pause estivale · mercato d'été"), FString::Printf(TEXT("%d jours sans course, transferts possibles"), Gaps[Mid - 1]));
	AddEvent(Last + FTimespan::FromDays(7.0), TEXT("Gala de fin de saison"), TEXT("remise des trophées, puis intersaison"));
	AddEvent(FDateTime(Yr, 12, 31), TEXT("Fin de la saison"), TEXT("contrats, moteur et transferts pour l'an prochain"));
	Items.StableSort([](const FItem& A, const FItem& B) { return A.At < B.At || (A.At == B.At && A.K < 0 && B.K >= 0); });

	// date du jour dans la carrière : avant le 1er GP, le lendemain du dernier GP couru, ou décembre
	FDateTime Today;
	if (S->IsOver()) Today = FDateTime(Yr, 12, 15);
	else if (G->Round <= 0) Today = First - FTimespan::FromDays(3.0);
	else Today = Dates[FMath::Clamp(G->Round - 1, 0, N - 1)] + FTimespan::FromDays(1.0);

	const TArray<FGPMRaceResult>& Res = S->Results();
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	int32 Month = 0;
	for (const FItem& It : Items)
	{
		if (It.At.GetMonth() != Month)
		{
			const bool bFirstMonth = Month == 0;
			Month = It.At.GetMonth();
			List->AddSlot().AutoHeight().Padding(FMargin(2.f, bFirstMonth ? 0.f : 14.f, 0.f, 6.f))
			[
				Txt(FString::Printf(TEXT("%s %d"), GPMUISeasonCal::MoisLong[FMath::Clamp(Month, 1, 12) - 1], Yr).ToUpper(), 14, CDim, true)
			];
		}
		if (It.K < 0)
		{
			const bool bPast = It.At < Today;
			List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
			[
				SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(FLinearColor(0.5f, 0.3f, 0.95f, bPast ? 0.05f : 0.12f))).Padding(FMargin(10.f, 6.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 20.f, 0.f))[ Dot(bPast ? CNeutral : CPurple, 10.f) ]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ Txt(It.Head, 15, bPast ? CDim : CText, true) ]
						+ SVerticalBox::Slot().AutoHeight()[ Txt(GPMUISeasonCal::FmtDate(It.At) + TEXT(" · ") + It.Sub, 12, bPast ? CFaint : CDim, false, true) ]
					]
				]
			];
			continue;
		}
		const int32 k = It.K;
		const FString Id = G->Calendar.IsValidIndex(k) ? G->Calendar[k] : FString();
		const FGPMTrackData* T = Data->GetTrack(Id);
		const bool bNext = k == G->Round && !S->IsOver();
		const bool bDone = Res.IsValidIndex(k);
		const bool bPodium = bDone && Res[k].Pos >= 1 && Res[k].Pos <= 3;
		FString Right;
		FLinearColor RightCol = CDim;
		if (bDone)
		{
			Right = FString::Printf(TEXT("P%d · +%d pts"), Res[k].Pos, Res[k].Pts);
			RightCol = bPodium ? CGold : Res[k].Pos <= 10 ? CGood : CDim;
		}
		else if (bNext) { Right = TEXT("À SUIVRE"); RightCol = Acc; }
		else
		{
			const int32 Gp = k ? Gaps[k - 1] : 0;
			Right = !k ? FString(TEXT("ouverture")) : Gp == 7 ? FString(TEXT("enchaîné")) : Gp >= 28 ? FString::Printf(TEXT("pause %d sem."), Gp / 7) : FString::Printf(TEXT("%d sem."), Gp / 7);
		}
		const FLinearColor NumBg = bNext ? Acc : bPodium ? CGold : bDone ? FLinearColor(0.2f, 0.23f, 0.3f, 1.f) : FLinearColor(0.12f, 0.13f, 0.17f, 1.f);
		const FString Sub = GPMUISeasonCal::FmtDate(It.At) + (T && !T->Type.IsEmpty() ? FString(TEXT(" · ")) + T->Type : FString());
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 4.f))
		[
			SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f))
			.BorderBackgroundColor(FSlateColor(bNext ? FLinearColor(Acc.R, Acc.G, Acc.B, 0.25f) : FLinearColor(1.f, 1.f, 1.f, 0.035f)))
			.Padding(FMargin(10.f, 6.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
				[
					SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)
					[
						SNew(SBorder).BorderImage(GPMUIStyle::Rounded(40.f)).BorderBackgroundColor(FSlateColor(NumBg)).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[ Txt(FString::FromInt(k + 1), 13, InkOn(NumBg), true) ]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ Txt(T ? T->Name : Id, 15, bDone && !bNext ? CDim : CText, true) ]
					+ SVerticalBox::Slot().AutoHeight()[ Txt(Sub, 12, CFaint) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 0.f, 0.f))[ Txt(Right, 14, RightCol, bDone || bNext) ]
			]
		];
	}
	return Card(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[ Txt(FString::Printf(TEXT("CALENDRIER %d · %d GRANDS PRIX"), Yr, N), 14, CDim, true) ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ],
		CCardBg, 14.f);
}

// ---------- courbes de performance ----------

TSharedRef<SWidget> FGPMUI::PerfPanel()
{
	UGPMSeasonSubsystem* S = SeasonSys();
	UGPMDataSubsystem* Data = DataSys();
	const UGPMSaveGame* G = S ? S->State() : nullptr;
	if (!G || !Data) return SNullWidget::NullWidget;
	const int32 N = G->PerfN;
	const int32 NRec = N > 0 ? G->PerfHist.Num() / N : 0;
	if (NRec == 0 || N != Data->Teams.Num())
		return Card(Txt(TEXT("Les courbes apparaîtront après la première course."), 16, CDim, false, true), CCardBg, 18.f);
	TSharedRef<SGPMPerfChart> Chart = SNew(SGPMPerfChart);
	float Min = 9.f, Max = -9.f;
	for (int32 i = 0; i < NRec * N; ++i) { Min = FMath::Min(Min, G->PerfHist[i]); Max = FMath::Max(Max, G->PerfHist[i]); }
	Chart->Lo = Min - 0.003f;
	Chart->Hi = Max + 0.003f;
	Chart->NX = FMath::Max(S->NumRounds(), NRec - 1);
	Chart->MeIdx = 0;
	for (int32 t = 0; t < N; ++t)
	{
		TArray<float> Curve;
		for (int32 r = 0; r < NRec; ++r) Curve.Add(G->PerfHist[r * N + t]);
		Chart->Series.Add(Curve);
		FLinearColor Col = Data->Teams[t].Color;
		Col.A = 1.f;
		if (Col.GetLuminance() < 0.04f) Col = GPMUIStyle::Lift(Col, 1.8f);
		Chart->Colors.Add(Col);
	}
	// légende : écuries de la plus rapide à la plus lente, progression depuis le départ
	TArray<int32> Ord;
	for (int32 t = 0; t < N; ++t) Ord.Add(t);
	const TArray<TArray<float>>& Ser = Chart->Series;
	Ord.Sort([&Ser](int32 A, int32 B) { return Ser[A].Last() > Ser[B].Last(); });
	TSharedRef<SVerticalBox> Leg1 = SNew(SVerticalBox), Leg2 = SNew(SVerticalBox);
	for (int32 k = 0; k < Ord.Num(); ++k)
	{
		const int32 Ti = Ord[k];
		const float Now = Ser[Ti].Last(), Gain = (Ser[Ti].Last() - Ser[Ti][0]) * 100.f;
		TSharedRef<SWidget> Item = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))[ Txt(FString::FromInt(k + 1), 12, CFaint) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Dot(Chart->Colors[Ti], 10.f) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[ Txt(Data->Teams[Ti].Name, 14, Ti == 0 ? CGold : CText, Ti == 0) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f, 0.f, 0.f, 0.f))[ Txt(FString::Printf(TEXT("%.1f"), Now * 100.f), 14, CText, true) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f, 0.f, 0.f))
			[
				SNew(SBox).WidthOverride(44.f)[ Txt(FString::Printf(TEXT("%+.1f"), Gain), 12, Gain >= 0.f ? CGood : CBad) ]
			];
		(k % 2 == 0 ? Leg1 : Leg2)->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[ Item ];
	}
	return Card(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(TEXT("INDICE DE PERFORMANCE DES VOITURES"), 14, CDim, true) ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ Chart ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f))
		[
			Txt(TEXT("D = départ de la saison, puis un relevé après chaque Grand Prix. En petit : progression depuis le départ. Ta voiture est la courbe épaisse."), 12, CFaint, false, true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 18.f, 0.f))[ Leg1 ]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ Leg2 ]
		],
		CCardBg, 16.f);
}

// ---------- mission du sponsor pour le prochain Grand Prix ----------

TSharedRef<SWidget> FGPMUI::MissionCard()
{
	UGPMCareerSubsystem* C = CareerSys();
	UGPMSeasonSubsystem* S = SeasonSys();
	const UGPMSaveGame* G = C ? C->Save() : nullptr;
	if (!G || !S || !C->IsCreated() || S->IsOver() || G->MisRound != G->Round || G->MisKind.IsEmpty()) return SNullWidget::NullWidget;
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("MISSION DU SPONSOR  ·  %s"), *G->MisSponsor.ToUpper()))).Font(GPMUIStyle::Face(2, 12)).ColorAndOpacity(FSlateColor(CGold))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f))
		[
			SNew(STextBlock).Text(FText::FromString(G->MisText)).Font(GPMUIStyle::Face(1, 18)).ColorAndOpacity(FSlateColor(CText)).AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[ Txt(FString::Printf(TEXT("Prime +%.1f M€ si tu réussis · en cas d'échec : réputation -3"), G->MisGain), 13, CDim, false, true) ];
	if (G->bMisAcc) Box->AddSlot().AutoHeight()[ Txt(TEXT("Mission acceptée : le sponsor compte sur toi."), 15, CGood, true) ];
	else
		Box->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[
			Button(TEXT("ACCEPTER LA MISSION"), FLinearColor(0.72f, 0.55f, 0.1f, 1.f), [this]() { if (UGPMCareerSubsystem* CS = CareerSys()) CS->AcceptMission(); }, true, 0.f, 44.f, 15)
		];
	return SNew(SBox).Padding(FMargin(0.f, 0.f, 0.f, 12.f))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()[ Layer(GPMUIStyle::Rounded(0.f), FLinearColor(0.03f, 0.028f, 0.02f, 0.82f)) ]
			+ SOverlay::Slot()[ Layer(GPMUIStyle::Ramp(0), FLinearColor(0.72f, 0.55f, 0.1f, 0.3f)) ]
			+ SOverlay::Slot().HAlign(HAlign_Left)[ SNew(SBox).WidthOverride(3.f)[ Layer(GPMUIStyle::Rounded(0.f), CGold) ] ]
			+ SOverlay::Slot().Padding(FMargin(18.f, 14.f, 16.f, 14.f))[ Box ]
		];
}
