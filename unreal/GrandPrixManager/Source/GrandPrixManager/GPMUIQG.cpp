// QG de l'écurie en Slate : recherche (arbres de pièces), installations (bâtiments à 3 niveaux) et chefs de pôle.
#include "GPMUI.h"
#include "GrandPrixManager.h"
#include "GPMHUD.h"
#include "GPMGameMode.h"
#include "GPMSeason.h"
#include "GPMFactory.h"
#include "Engine/GameInstance.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Rendering/DrawElements.h"

using namespace GPMUIColor;

namespace GPMUIQGText
{
	FString EffectName(const FString& K)
	{
		return K == TEXT("vtop") ? TEXT("Vitesse de pointe") : K == TEXT("acc") ? TEXT("Accélération") : K == TEXT("mu") ? TEXT("Adhérence")
			: K == TEXT("brake") ? TEXT("Freinage") : K == TEXT("kd") ? TEXT("Appui") : K == TEXT("steer") ? TEXT("Agilité")
			: K == TEXT("fuel") ? TEXT("Consommation") : K == TEXT("rel") ? TEXT("Fiabilité") : K == TEXT("tyre") ? TEXT("Usure des pneus")
			: K == TEXT("pit") ? TEXT("Arrêt au stand") : K == TEXT("slip") ? TEXT("Glisse") : K == TEXT("off") ? TEXT("Hors-piste")
			: K == TEXT("wall") ? TEXT("Chocs") : K;
	}

	// violet des installations (#c06cff du jeu web)
	const FLinearColor CInfra(0.53f, 0.16f, 1.f, 1.f);

	FLinearColor Dim(const FLinearColor& C, float K) { return FLinearColor(C.R * K, C.G * K, C.B * K, 1.f); }
}

UGPMFactorySubsystem* FGPMUI::FactorySys() const
{
	const AGPMHUD* H = Hud.Get();
	UGameInstance* GI = H ? H->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UGPMFactorySubsystem>() : nullptr;
}

TSharedRef<SWidget> FGPMUI::Tabs(const TArray<FString>& Labels, int32 Sel, TFunction<void(int32)> OnPick)
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	for (int32 k = 0; k < Labels.Num(); ++k)
	{
		// onglets penchés accolés : l'onglet choisi prend la couleur de l'écurie, les autres restent en verre fumé
		const bool bOn = k == Sel;
		const FLinearColor Bg = bOn ? Acc : FLinearColor(1.f, 1.f, 1.f, 0.07f);
		Box->AddSlot().AutoWidth().Padding(FMargin(k ? -14.f : 0.f, 0.f, 0.f, 0.f))
		[
			Hit(SNew(SBox).HAlign(HAlign_Center)[ Ital(Labels[k], 14, bOn ? InkOn(Acc) : CDim, 1, true) ],
				Bg, bOn ? GPMUIStyle::Lift(Acc, 1.15f) : FLinearColor(1.f, 1.f, 1.f, 0.16f), [OnPick, k]() { if (OnPick) OnPick(k); }, true, 0.f, 40.f, FMargin(28.f, 2.f))
		];
	}
	return Box;
}

// ---------- QG : en-tête et onglets ----------

TSharedRef<SWidget> FGPMUI::FactoryScreen(AGPMGameMode* GM)
{
	UGPMFactorySubsystem* F = FactorySys();
	UGPMSeasonSubsystem* S = SeasonSys();
	if (!F || !S || F->Trees().Num() == 0) return SNullWidget::NullWidget;
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	QGTab = FMath::Clamp(QGTab, 0, 2);
	if (QGTab == 2) F->EnsureStaff();
	GM->bFutCar = QGTab == 0 && FutView();   // l'atelier montre la voiture de la prochaine ère
	const UGPMSaveGame* G = S->State();
	static const TCHAR* const TabTitles[3] = { TEXT("Recherche et développement"), TEXT("Installations"), TEXT("Chefs de pôle") };

	TSharedRef<SHorizontalBox> Right = SNew(SHorizontalBox);
	auto AddRight = [&Right](const TSharedRef<SWidget>& W, float Gap) { Right->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, Gap, 0.f))[ W ]; };
	AddRight(Chip(TEXT("Budget"), FString::Printf(TEXT("%.1f M€"), F->Budget()), F->Budget() < 0.f ? CBad : CGood), 10.f);
	if (QGTab == 0) AddRight(Chip(TEXT("Ateliers"), FString::Printf(TEXT("%d / %d"), F->Running(), F->Workshops()), CText), 16.f);
	else if (QGTab == 1)
	{
		const bool bWip = G && !G->BuildId.IsEmpty();
		AddRight(Chip(TEXT("Chantier"), bWip ? FString::Printf(TEXT("%d j"), G->BuildLeft) : FString(TEXT("libre")), bWip ? CGold : CText), 16.f);
	}
	else
	{
		float Pay = 0.f;
		if (G) for (const TPair<FString, float>& P : G->ChiefSal) Pay += P.Value;
		AddRight(Chip(TEXT("Salaires des chefs"), FString::Printf(TEXT("%.1f M€ / saison"), Pay), CText), 16.f);
	}
	AddRight(Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Season; }, true, 150.f, 46.f, 15), 0.f);

	TSharedRef<SWidget> Content = QGTab == 0 ? ResearchTab() : QGTab == 1 ? InfraTab() : ChiefsTab();
	const FString TeamName = G && G->bCreated ? G->TeamName : FString(TEXT("ton écurie"));
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ Title(FString::Printf(TEXT("QG · %s"), *GPMUp(TeamName)), TabTitles[QGTab], Right) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Tabs({ TEXT("RECHERCHE"), TEXT("INSTALLATIONS"), TEXT("CHEFS DE PÔLE") }, QGTab, [this](int32 k) { QGTab = k; }) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Right)[ Txt(TEXT("Tab : onglet suivant · flèches : choisir · Entrée : lancer · U : saison"), 13, CFaint) ]
		]
		+ SVerticalBox::Slot().FillHeight(1.f)[ Content ];
	return Page(Body, CRoomBg);
}

// ---------- onglet Recherche : l'arbre de chaque pôle, avec ses branches ----------

namespace GPMUITree
{
	struct FLink { FVector2D A, B; FLinearColor Col; float Thick; };
	/** Liens entre les pièces d'un arbre (prérequis), dessinés sous les fiches : positions en fraction de la zone. */
	class SLinks : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SLinks) {}
		SLATE_END_ARGS()
		TArray<FLink> Links;
		void Construct(const FArguments&) {}
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			const FVector2D Sz = Geo.GetLocalSize();
			for (const FLink& L : Links)
			{
				// coude : descente verticale, palier horizontal à mi-hauteur, puis descente
				const FVector2D A(L.A.X * Sz.X, L.A.Y * Sz.Y), B(L.B.X * Sz.X, L.B.Y * Sz.Y);
				const float MidY = (A.Y + B.Y) * 0.5f;
				TArray<FVector2D> Pts = { A, FVector2D(A.X, MidY), FVector2D(B.X, MidY), B };
				FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, L.Col, true, L.Thick);
			}
			return Layer + 1;
		}
	};
}

bool FGPMUI::FutView() const
{
	const UGPMFactorySubsystem* F = FactorySys();
	return bFutView && F && F->FutureOpen();
}

TSharedRef<SWidget> FGPMUI::ResearchTab()
{
	UGPMFactorySubsystem* F = FactorySys();
	if (!F || F->Trees().Num() == 0) return SNullWidget::NullWidget;
	TWeakObjectPtr<UGPMFactorySubsystem> WF = F;
	const bool bFut = FutView();
	const TArray<FGPMTree>& Trees = F->Trees(bFut);
	SelTree = FMath::Clamp(SelTree, 0, Trees.Num() - 1);
	SelPart = FMath::Clamp(SelPart, 0, FMath::Max(0, Trees[SelTree].Parts.Num() - 1));
	if (Trees[SelTree].Parts.Num() == 0) return SNullWidget::NullWidget;
	using ES = UGPMFactorySubsystem::EState;
	const FString King = F->KingDept(bFut);
	const int32 At = F->ReformYear();

	// onglets des pôles (pôle roi de l'ère en or), puis la voiture de la prochaine ère quand son règlement est publié
	TSharedRef<SHorizontalBox> TabRow = SNew(SHorizontalBox);
	for (int32 t = 0; t < Trees.Num(); ++t)
	{
		const FGPMTree& Tr = Trees[t];
		int32 Have = 0;
		for (const FGPMPart& P : Tr.Parts) if (F->StateOf(P) == ES::Owned) ++Have;
		const bool bOn = t == SelTree, bKing = Tr.Id == King;
		const bool bWip = F->RunningIn(Tr.Id, bFut) > 0;
		const FLinearColor Bg = bOn ? GPMUIQGText::Dim(Tr.Color, 0.75f) : FLinearColor(1.f, 1.f, 1.f, 0.07f);
		TSharedRef<SVerticalBox> Lab = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Ital(GPMUp(Tr.Id == TEXT("stand") ? FString(TEXT("Pit stop")) : Tr.Name), 14, bOn ? CText : Tr.Color, 1, true) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[ Txt(FString::Printf(TEXT("%s%d/%d%s"), bKing ? TEXT("PÔLE ROI · ") : TEXT(""), Have, Tr.Parts.Num(), bWip ? TEXT(" · en cours") : TEXT("")), 11, bKing ? CGold : bOn ? CText : CDim, bKing) ];
		TabRow->AddSlot().AutoWidth().Padding(FMargin(t ? -10.f : 0.f, 0.f, 0.f, 0.f))
		[ Hit(Lab, Bg, bOn ? GPMUIStyle::Lift(Bg, 1.15f) : FLinearColor(1.f, 1.f, 1.f, 0.16f), [this, t]() { SelTree = t; SelPart = 0; }, true, 150.f, 56.f, FMargin(26.f, 2.f)) ];
	}
	if (F->FutureOpen())
	{
		const FLinearColor Gold(0.72f, 0.55f, 0.1f, 1.f);
		const FLinearColor Bg = bFut ? Gold : FLinearColor(0.72f, 0.55f, 0.1f, 0.22f);
		TSharedRef<SVerticalBox> Lab = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Ital(bFut ? FString::Printf(TEXT("VOITURE %d"), F->Year()) : FString::Printf(TEXT("VOITURE %d"), At), 14, bFut ? InkOn(Gold) : CGold, 1, true) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Txt(bFut ? TEXT("revenir à l'ère actuelle") : TEXT("nouveau règlement"), 11, bFut ? InkOn(Gold) : CGold) ];
		TabRow->AddSlot().AutoWidth().Padding(FMargin(18.f, 0.f, 0.f, 0.f))
		[ Hit(Lab, Bg, GPMUIStyle::Lift(Bg, 1.2f), [this]() { bFutView = !bFutView; SelPart = 0; }, true, 190.f, 56.f, FMargin(26.f, 2.f)) ];
	}

	// l'arbre : une rangée par niveau, trois colonnes, liens des prérequis
	const FGPMTree& Tr = Trees[SelTree];
	int32 Rows = 1;
	for (const FGPMPart& P : Tr.Parts) Rows = FMath::Max(Rows, P.Row);
	auto Pos = [Rows](const FGPMPart& P) { return FVector2D((FMath::Clamp(P.Col, 1, 3) - 0.5f) / 3.f, (P.Row - 0.5f) / Rows); };
	TSharedRef<GPMUITree::SLinks> Links = SNew(GPMUITree::SLinks);
	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
	for (int32 k = 0; k < Tr.Parts.Num(); ++k)
	{
		const FGPMPart& P = Tr.Parts[k];
		const ES St = F->StateOf(P);
		auto Link = [&](const FString& ReqId, bool bAny)
		{
			for (const FGPMPart& Q : Tr.Parts)
				if (Q.Id == ReqId)
				{
					const bool bLit = F->StateOf(Q) == ES::Owned;
					FLinearColor C = bLit ? Tr.Color : FLinearColor(1.f, 1.f, 1.f, 0.16f);
					if (bAny && !bLit) C.A = 0.1f;
					Links->Links.Add({ Pos(Q), Pos(P), C, bLit ? 3.f : 2.f });
				}
		};
		for (const FString& R : P.Req) Link(R, false);
		for (const FString& R : P.ReqAny) Link(R, true);
		FString InfoTxt;
		FLinearColor InfoCol = CDim, NameCol = CText;
		switch (St)
		{
		case ES::Owned: InfoTxt = bFut ? TEXT("prête") : TEXT("installée"); InfoCol = CGood; break;
		case ES::Running: InfoTxt = FString::Printf(TEXT("en recherche · %.0f j"), F->DaysLeft(P)); InfoCol = CGold; break;
		case ES::Available: InfoTxt = FString::Printf(TEXT("%.1f M€ · %d j"), F->PriceOf(P), F->DelayOf(P)); InfoCol = CInfo; break;
		case ES::TooExpensive: InfoTxt = FString::Printf(TEXT("%.1f M€ · trop cher"), F->PriceOf(P)); InfoCol = CBad; NameCol = CDim; break;
		case ES::Busy: InfoTxt = TEXT("ateliers occupés"); NameCol = CDim; break;
		default: InfoTxt = P.ReqAny.Num() ? TEXT("verrouillée · une branche suffit") : TEXT("verrouillée"); InfoCol = CFaint; NameCol = CFaint; break;
		}
		TSharedRef<SVerticalBox> Tile = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(P.Name)).Font(GPMUIStyle::Face(1, 14)).ColorAndOpacity(FSlateColor(NameCol)).AutoWrapText(true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Txt(InfoTxt, 12, InfoCol) ];
		if (St == ES::Running)
			Tile->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[ Bar(1.f - F->DaysLeft(P) / FMath::Max(1.f, F->DaysTotal(P)), CGold, 4.f) ];
		// liseré de la couleur du pôle sur les pièces installées
		TSharedRef<SWidget> Node = SNew(SOverlay)
			+ SOverlay::Slot()[ Pick(Tile, k == SelPart, [this, k]() { SelPart = k; }) ]
			+ SOverlay::Slot().HAlign(HAlign_Left)
			[ SNew(SBox).WidthOverride(4.f)[ Layer(GPMUIStyle::Rounded(2.f), St == ES::Owned ? Tr.Color : St == ES::Running ? CGold : FLinearColor(0.f, 0.f, 0.f, 0.f)) ] ];
		const FVector2D A = Pos(P);
		Canvas->AddSlot()
			.Anchors(FAnchors(A.X, A.Y))
			.Alignment(FVector2D(0.5f, 0.5f))
			.Offset(FMargin(0.f, 0.f, 260.f, 66.f))
			[ Node ];
	}
	const FGPMChief Boss = F->ChiefOf(Tr.Id);
	int32 Have = 0;
	for (const FGPMPart& P : Tr.Parts) if (F->StateOf(P) == ES::Owned) ++Have;
	TSharedRef<SWidget> TreeCard = Card(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[ Ital(FString::Printf(TEXT("%s · %s"), *GPMUp(Tr.Name), bFut ? *FString::Printf(TEXT("VOITURE %d"), At) : *GPMUp(F->CurrentEraName())), 18, Tr.Color, 1) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ Txt(FString::Printf(TEXT("%d/%d pièces · %s"), Have, Tr.Parts.Num(), Boss.IsValid() ? *FString::Printf(TEXT("chef : %s"), *Boss.Name) : TEXT("pas de chef de pôle")), 13, CDim) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 6.f))[ Bar(Tr.Parts.Num() ? float(Have) / Tr.Parts.Num() : 0.f, Tr.Color, 4.f) ]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBox).MinDesiredHeight(Rows * 86.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot().Padding(FMargin(0.f, 6.f))[ Links ]
				+ SOverlay::Slot().Padding(FMargin(0.f, 6.f))[ Canvas ]
			]
		], CCardBg, 14.f);

	// fiche de la pièce choisie
	const FGPMPart& P = Tr.Parts[SelPart];
	const ES St = F->StateOf(P);
	TSharedRef<SWrapBox> Fx = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f, 6.f));
	for (const TPair<FString, float>& E : P.Fx)
	{
		const bool bAbs = E.Key == TEXT("rel") || E.Key == TEXT("pit");
		const float V = bAbs ? E.Value : E.Value * 100.f;
		const bool bLowGood = E.Key == TEXT("fuel") || E.Key == TEXT("tyre") || E.Key == TEXT("pit") || E.Key == TEXT("wall");
		Fx->AddSlot()
		[
			Chip(GPMUIQGText::EffectName(E.Key), FString::Printf(TEXT("%+.1f%s"), V, E.Key == TEXT("pit") ? TEXT(" s") : bAbs ? TEXT("") : TEXT(" %")),
				bLowGood ? (V < 0.f ? CGood : CBad) : (V >= 0.f ? CGood : CBad))
		];
	}
	FString StateTxt;
	switch (St)
	{
	case ES::Owned: StateTxt = bFut ? FString::Printf(TEXT("Prête : elle sera sur la voiture %d."), At) : FString(TEXT("Déjà installée sur la voiture.")); break;
	case ES::Running: StateTxt = FString::Printf(TEXT("En cours de recherche : %.0f jours restants."), F->DaysLeft(P)); break;
	case ES::Available: StateTxt = TEXT("Prête à être lancée."); break;
	case ES::TooExpensive: StateTxt = TEXT("Budget insuffisant."); break;
	case ES::Busy: StateTxt = F->PerDept() > 1 ? TEXT("Ce pôle a déjà deux pièces en cours, ou tous les ateliers sont pris.") : TEXT("Ce pôle travaille déjà sur une pièce, ou tous les ateliers sont pris."); break;
	default: StateTxt = P.ReqAny.Num() ? TEXT("Il faut d'abord une des pièces juste au-dessus (une seule branche suffit).") : TEXT("Il faut d'abord les pièces juste au-dessus dans l'arbre."); break;
	}
	const float Base = UGPMFactorySubsystem::Price(P), Now = F->PriceOf(P);
	FString CostTxt = FString::Printf(TEXT("%.1f M€%s · %d jours · échec %d %% · retard %d %%"), Now,
		Now < Base - 0.05f ? *FString::Printf(TEXT(" (au lieu de %.1f)"), Base) : TEXT(""), F->DelayOf(P), F->FailOf(P), F->LateOf(P));
	if (Boss.IsValid()) CostTxt += FString::Printf(TEXT("\nChef %s : prix -%d %%, délai -%d %%, risque -%d %%"), *Boss.Name, Boss.Cost, Boss.Time, Boss.Risk);
	const FString PartId = P.Id;
	const FLinearColor TC = Tr.Color;
	TSharedRef<SVerticalBox> Side = SNew(SVerticalBox);
	Side->AddSlot().AutoHeight()
	[
		Card(SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Txt(bFut ? FString::Printf(TEXT("VOITURE %d · %s"), At, *GPMUp(Tr.Name)) : GPMUp(Tr.Name), 12, TC, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(P.Name)).Font(GPMUIStyle::Face(0, 24)).ColorAndOpacity(FSlateColor(CText)).AutoWrapText(true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 8.f))[ Txt(P.Desc, 15, CDim, false, true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Fx ]
			+ SVerticalBox::Slot().AutoHeight()[ Txt(CostTxt, 13, CDim, false, true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 10.f))[ Txt(StateTxt, 14, St == ES::Available ? CGood : St == ES::Running ? CGold : CDim, true, true) ]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[ Button(TEXT("LANCER LA RECHERCHE"), Acc, [WF, PartId]() { if (WF.IsValid()) WF->Start(PartId); }, St == ES::Available, 260.f, 54.f, 16) ],
			FLinearColor(TC.R * 0.22f, TC.G * 0.22f, TC.B * 0.22f, 0.88f), 18.f)
	];
	// l'ère : pôle roi, et la voiture de la prochaine ère (même échelle d'indice que les écuries)
	{
		const int32 EI = bFut ? F->NextEraIndex() : F->EraIndex();
		const UGPMFactorySubsystem::FEra& E = UGPMFactorySubsystem::EraDef(EI);
		TSharedRef<SVerticalBox> EB = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Txt(bFut ? FString::Printf(TEXT("RÈGLEMENT %d"), At) : FString(TEXT("RÈGLEMENT EN VIGUEUR")), 12, CPurple, true) ]
			+ SVerticalBox::Slot().AutoHeight()[ Ital(bFut ? F->NextEraName() : F->CurrentEraName(), 20, CText, 1) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f))[ Txt(E.Focus, 14, CGold, false, true) ];
		if (bFut)
		{
			EB->AddSlot().AutoHeight()[ Txt(E.Desc, 13, CDim, false, true) ];
			EB->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[ Txt(FString::Printf(TEXT("Voiture %d : indice %.1f · %d pièce%s prête%s · voiture %d : %.1f"), At, F->PlayerPerf(true) * 100.f, F->FutCount(), F->FutCount() > 1 ? TEXT("s") : TEXT(""), F->FutCount() > 1 ? TEXT("s") : TEXT(""), F->Year(), F->PlayerPerf(false) * 100.f), 14, CText, true, true) ];
		}
		else if (F->FutureOpen())
			EB->AddSlot().AutoHeight()[ Txt(FString::Printf(TEXT("Refonte totale en %d : développe dès maintenant la voiture %d (onglet doré)."), At, At), 13, CDim, false, true) ];
		else
			EB->AddSlot().AutoHeight()[ Txt(FString::Printf(TEXT("Prochaine grande refonte : pas avant %d."), At - 1), 13, CFaint, false, true) ];
		Side->AddSlot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))[ Card(EB, FLinearColor(0.07f, 0.05f, 0.11f, 0.85f), 16.f) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ TabRow ]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.55f).Padding(FMargin(0.f, 0.f, 14.f, 0.f))[ TreeCard ]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ Side ] ]
		];
}

// ---------- onglet Installations : 7 bâtiments à 3 niveaux, un chantier à la fois ----------

TSharedRef<SWidget> FGPMUI::InfraTab()
{
	UGPMFactorySubsystem* F = FactorySys();
	UGPMSeasonSubsystem* S = SeasonSys();
	const UGPMSaveGame* G = S ? S->State() : nullptr;
	if (!F || !G || F->Buildings().Num() == 0) return SNullWidget::NullWidget;
	TWeakObjectPtr<UGPMFactorySubsystem> WF = F;
	const TArray<FGPMBuilding>& Bs = F->Buildings();
	SelInfra = FMath::Clamp(SelInfra, 0, Bs.Num() - 1);
	const FLinearColor CVio = GPMUIQGText::CInfra;

	auto Badge = [this, CVio](const FString& BTag, int32 BLv) -> TSharedRef<SWidget>
	{
		const FLinearColor Bg = BLv > 0 ? GPMUIQGText::Dim(CVio, 0.55f) : FLinearColor(0.16f, 0.17f, 0.22f, 1.f);
		return SNew(SBox).WidthOverride(64.f).HeightOverride(44.f)
			[
				SNew(SBorder).BorderImage(GPMUIStyle::Rounded(12.f)).BorderBackgroundColor(FSlateColor(Bg)).HAlign(HAlign_Center).VAlign(VAlign_Center)
				[ Txt(BTag, 13, CText, true) ]
			];
	};

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (int32 k = 0; k < Bs.Num(); ++k)
	{
		const FGPMBuilding& B = Bs[k];
		const int32 Lv = F->Level(B.Id);
		const bool bWip = G->BuildId == B.Id;
		FString Info;
		FLinearColor InfoCol = CDim;
		if (bWip) { Info = FString::Printf(TEXT("En construction : niveau %d dans %d jours"), Lv + 1, G->BuildLeft); InfoCol = CGold; }
		else if (Lv >= 3) { Info = TEXT("Niveau maximum"); InfoCol = CGood; }
		else
		{
			const float Cost = UGPMFactorySubsystem::BuildCost(B.Id, Lv);
			Info = FString::Printf(TEXT("Niveau %d : %.1f M€ · %d jours"), Lv + 1, Cost, UGPMFactorySubsystem::BuildDays(Lv));
			InfoCol = G->Budget >= Cost && G->BuildId.IsEmpty() ? CInfo : CDim;
		}
		TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
		for (int32 p = 0; p < 3; ++p)
		{
			const FLinearColor PipCol = p < Lv ? CVio : bWip && p == Lv ? CGold : FLinearColor(1.f, 1.f, 1.f, 0.12f);
			Pips->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 5.f, 0.f))
			[
				SNew(SBox).WidthOverride(22.f).HeightOverride(8.f)
				[
					SNew(SBorder).BorderImage(GPMUIStyle::Rounded(6.f)).BorderBackgroundColor(FSlateColor(PipCol))
				]
			];
		}
		TSharedRef<SWidget> Line = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 14.f, 0.f))[ Badge(B.Tag, Lv) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(B.Name, 16, CText, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(Info, 13, InfoCol) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f, 0.f, 0.f, 0.f))[ Pips ];
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Pick(Line, k == SelInfra, [this, k]() { SelInfra = k; }) ];
	}

	// fiche du bâtiment choisi
	const FGPMBuilding& Sel = Bs[SelInfra];
	const int32 SLv = F->Level(Sel.Id);
	FString Why;
	const bool bCan = F->CanBuild(Sel.Id, Why);
	TSharedRef<SVerticalBox> Det = SNew(SVerticalBox);
	Det->AddSlot().AutoHeight()[ Txt(FString::Printf(TEXT("INSTALLATION · NIVEAU %d / 3"), SLv), 13, GPMUIStyle::Lift(CVio, 1.3f), true) ];
	Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))[ Txt(Sel.Name, 28, CText, true) ];
	Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 14.f))[ Txt(Sel.Desc, 15, CDim, false, true) ];
	for (int32 k = 0; k < 3 && k < Sel.Lv.Num(); ++k)
	{
		const bool bDone = k < SLv, bNext = k == SLv;
		const FLinearColor Ring = bDone ? FLinearColor(0.12f, 0.42f, 0.22f, 1.f) : bNext ? FLinearColor(0.45f, 0.33f, 0.06f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.08f);
		Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
			[
				SNew(SBox).WidthOverride(30.f).HeightOverride(30.f)
				[
					SNew(SBorder).BorderImage(GPMUIStyle::Rounded(40.f)).BorderBackgroundColor(FSlateColor(Ring)).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[ Txt(FString::FromInt(k + 1), 14, bDone ? CGood : bNext ? CGold : CFaint, true) ]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[ Txt(Sel.Lv[k] + (bDone ? FString(TEXT("  (fait)")) : FString()), 16, bDone ? CGood : bNext ? CText : CFaint, bNext, true) ]
		];
	}
	if (G->BuildId == Sel.Id)
	{
		Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 6.f))[ Txt(FString::Printf(TEXT("En construction : prêt dans %d jours."), G->BuildLeft), 17, CGold, true) ];
		Det->AddSlot().AutoHeight()[ Bar(1.f - float(G->BuildLeft) / float(FMath::Max(1, G->BuildTot)), CGold, 6.f) ];
	}
	else if (SLv < 3)
	{
		Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 4.f))
		[
			Txt(FString::Printf(TEXT("Niveau %d : %.1f M€, payés tout de suite · chantier de %d jours"), SLv + 1, UGPMFactorySubsystem::BuildCost(Sel.Id, SLv), UGPMFactorySubsystem::BuildDays(SLv)), 15, CText, false, true)
		];
		if (!bCan && !Why.IsEmpty()) Det->AddSlot().AutoHeight()[ Txt(Why, 14, CBad, false, true) ];
		const FString BuildIdSel = Sel.Id;
		Det->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 12.f, 0.f, 0.f))
		[
			Button(TEXT("CONSTRUIRE"), Acc, [WF, BuildIdSel]() { if (WF.IsValid()) WF->Build(BuildIdSel); }, bCan, 240.f, 54.f, 17)
		];
	}
	else Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 0.f))[ Txt(TEXT("Niveau maximum atteint."), 17, CGood, true) ];

	// effets de tous les bâtiments
	const float Disc = 1.f - 0.1f * F->Level(TEXT("rd"));
	static const int32 WindPct[4] = { 0, 15, 25, 30 };
	TSharedRef<SHorizontalBox> Fx1 = SNew(SHorizontalBox), Fx2 = SNew(SHorizontalBox);
	auto AddFx = [this](const TSharedRef<SHorizontalBox>& To, const FString& Label, const FString& Value)
	{
		To->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Chip(Label, Value, CText) ];
	};
	AddFx(Fx1, TEXT("Partenaires"), FString::Printf(TEXT("+%.1f M€ / GP"), F->PartnerIncome()));
	AddFx(Fx1, TEXT("Primes"), FString::Printf(TEXT("x%.1f"), F->PrizeMul()));
	AddFx(Fx1, TEXT("Prix des pièces"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Disc * 100.f)));
	AddFx(Fx2, TEXT("Délais"), FString::Printf(TEXT("-%d %%"), WindPct[FMath::Clamp(F->Level(TEXT("wind")), 0, 3)]));
	AddFx(Fx2, TEXT("Ateliers"), FString::FromInt(F->Workshops()));
	AddFx(Fx2, TEXT("Arrêts"), F->PitBonus() < 0.f ? FString::Printf(TEXT("%.1f s"), F->PitBonus()) : FString(TEXT("normaux")));
	AddFx(Fx2, TEXT("Équipier"), FString::Printf(TEXT("x%.1f"), F->MateGrowth()));
	TSharedRef<SWidget> Effects = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(TEXT("EFFETS DE TES BÂTIMENTS"), 13, CDim, true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Fx1 ]
		+ SVerticalBox::Slot().AutoHeight()[ Fx2 ];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
		[
			Card(SNew(SScrollBox) + SScrollBox::Slot()[ List ], CCardBg, 12.f)
		]
		+ SHorizontalBox::Slot().FillWidth(1.1f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 0.f, 0.f, 12.f))
			[
				Card(SNew(SScrollBox) + SScrollBox::Slot()[ Det ], FLinearColor(0.11f, 0.06f, 0.2f, 0.85f), 20.f)
			]
			+ SVerticalBox::Slot().AutoHeight()[ Card(Effects, CCardBg, 14.f) ]
		];
}

// ---------- onglet Chefs de pôle : contrats, chefs libres et chefs des écuries rivales ----------

TSharedRef<SWidget> FGPMUI::ChiefsTab()
{
	UGPMFactorySubsystem* F = FactorySys();
	UGPMSeasonSubsystem* S = SeasonSys();
	const UGPMSaveGame* G = S ? S->State() : nullptr;
	if (!F || !G) return SNullWidget::NullWidget;
	TWeakObjectPtr<UGPMFactorySubsystem> WF = F;
	const TArray<TPair<FString, FString>>& Ds = UGPMFactorySubsystem::Depts();
	SelDept = FMath::Clamp(SelDept, 0, Ds.Num() - 1);
	const FString Dept = Ds[SelDept].Key;

	auto DeptColor = [F](const FString& DeptId)
	{
		for (const FGPMTree& Tr : F->Trees()) if (Tr.Id == DeptId) return Tr.Color;
		return FLinearColor(0.6f, 0.62f, 0.7f, 1.f);
	};
	auto Stats = [this](const FGPMChief& Ch) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 12.f, 0.f))[ Txt(FString::Printf(TEXT("Prix -%d %%"), Ch.Cost), 13, CGood) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 12.f, 0.f))[ Txt(FString::Printf(TEXT("Délai -%d %%"), Ch.Time), 13, CInfo) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Txt(FString::Printf(TEXT("Risque -%d %%"), Ch.Risk), 13, CGold) ];
	};

	// les 5 pôles et leur chef
	TSharedRef<SVerticalBox> DList = SNew(SVerticalBox);
	for (int32 k = 0; k < Ds.Num(); ++k)
	{
		const FGPMChief Cur = F->ChiefOf(Ds[k].Key);
		TSharedRef<SVerticalBox> Tile = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Txt(GPMUp(Ds[k].Value), 13, DeptColor(Ds[k].Key), true) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))[ Txt(Cur.IsValid() ? Cur.Name : FString(TEXT("Poste vacant")), 17, Cur.IsValid() ? CText : CFaint, true) ];
		if (Cur.IsValid())
		{
			Tile->AddSlot().AutoHeight()[ Stats(Cur) ];
			Tile->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))
			[
				Txt(FString::Printf(TEXT("Contrat jusqu'en %d · %.1f M€ / saison"), G->ChiefUntil.FindRef(Ds[k].Key), G->ChiefSal.FindRef(Ds[k].Key)), 12, CDim)
			];
		}
		else Tile->AddSlot().AutoHeight()[ Txt(TEXT("Un chef rend les pièces moins chères, plus rapides et plus fiables."), 12, CFaint, false, true) ];
		DList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Pick(Tile, k == SelDept, [this, k]() { SelDept = k; SelCand = 0; }) ];
	}

	// chef en poste du pôle choisi
	const FGPMChief Mine = F->ChiefOf(Dept);
	TSharedRef<SVerticalBox> Top = SNew(SVerticalBox);
	Top->AddSlot().AutoHeight()[ Txt(FString::Printf(TEXT("PÔLE %s · TON CHEF"), *GPMUp(Ds[SelDept].Value)), 13, DeptColor(Dept), true) ];
	if (Mine.IsValid())
	{
		const int32 Until = G->ChiefUntil.FindRef(Dept);
		const FString DeptSel = Dept;
		Top->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("%s · %s"), *Mine.Name, *Mine.From), 20, CText, true) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f))[ Stats(Mine) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("Contrat jusqu'en %d, %.1f M€ par saison"), Until, G->ChiefSal.FindRef(Dept)), 13, CDim) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Button(TEXT("PROLONGER D'UNE SAISON"), CNeutral, [WF, DeptSel]() { if (WF.IsValid()) WF->Extend(DeptSel); }, Until - G->Year < 3, 0.f, 44.f, 14)
			]
		];
	}
	else Top->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))[ Txt(TEXT("Poste vacant : engage un chef parmi les candidats."), 18, CFaint, true, true) ];

	// candidats : chefs libres (nouveaux venus en or) puis chefs des écuries rivales
	const TArray<FGPMChiefOffer> Cands = F->Candidates(Dept);
	SelCand = FMath::Clamp(SelCand, 0, FMath::Max(0, Cands.Num() - 1));
	TSharedRef<SVerticalBox> CList = SNew(SVerticalBox);
	for (int32 k = 0; k < Cands.Num(); ++k)
	{
		const FGPMChiefOffer& O = Cands[k];
		const FString Tag = O.Club.IsEmpty() ? (O.bFresh ? FString(TEXT("NOUVEAU")) : FString(TEXT("LIBRE"))) : FString::Printf(TEXT("CHEZ %s"), *GPMUp(O.Club));
		const FLinearColor TagCol = O.Club.IsEmpty() ? (O.bFresh ? CGold : CGood) : CInfo;
		TSharedRef<SWidget> Line = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))[ Txt(O.C.Name, 16, CText, true) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Txt(Tag, 11, TagCol, true) ]
				]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("%s · note %d"), *O.C.From, O.C.Quality()), 12, CDim) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 3.f, 0.f, 0.f))[ Stats(O.C) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[ Txt(FString::Printf(TEXT("%.1f M€ / saison"), O.C.Fee), 14, CText, true) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[ Txt(O.Club.IsEmpty() ? FString() : FString::Printf(TEXT("indemnité %.1f M€"), UGPMFactorySubsystem::ChiefFee(O.C)), 12, CDim) ]
			];
		CList->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))[ Pick(Line, k == SelCand, [this, k]() { SelCand = k; }) ];
	}
	if (Cands.Num() == 0) CList->AddSlot().AutoHeight()[ Txt(TEXT("Aucun candidat pour ce pôle en ce moment. Reviens après le prochain Grand Prix."), 15, CDim, false, true) ];

	// signature du candidat choisi : 1, 2 ou 3 saisons
	TSharedRef<SVerticalBox> Det = SNew(SVerticalBox);
	if (Cands.IsValidIndex(SelCand))
	{
		const FGPMChiefOffer Offer = Cands[SelCand];
		const FString Cmp = Mine.IsValid()
			? FString::Printf(TEXT("Face à %s : prix %+d, délai %+d, risque %+d points de réduction."), *Mine.Name, Offer.C.Cost - Mine.Cost, Offer.C.Time - Mine.Time, Offer.C.Risk - Mine.Risk)
			: FString(TEXT("Le poste est vacant : n'importe quel chef améliore le pôle."));
		FString CostWhy = TEXT("Coût immédiat : la première saison de salaire");
		if (!Offer.Club.IsEmpty()) CostWhy += FString::Printf(TEXT(", %.1f M€ d'indemnité à %s"), UGPMFactorySubsystem::ChiefFee(Offer.C), *Offer.Club);
		const float Rel = F->ReleaseCost(Dept);
		if (Rel > 0.f) CostWhy += FString::Printf(TEXT(", %.1f M€ pour libérer %s"), Rel, *Mine.Name);
		Det->AddSlot().AutoHeight()[ Txt(FString::Printf(TEXT("ENGAGER %s"), *GPMUp(Offer.C.Name)), 14, CGold, true) ];
		Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[ Txt(Cmp, 14, CText, false, true) ];
		Det->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 10.f))[ Txt(CostWhy + TEXT("."), 13, CDim, false, true) ];
		TSharedRef<SHorizontalBox> Btns = SNew(SHorizontalBox);
		for (int32 y = 1; y <= 3; ++y)
		{
			const float Cost = F->HireCost(Offer, y);
			const FString Label = FString::Printf(TEXT("%d SAISON%s · %.1f M€/an · %.1f M€"), y, y > 1 ? TEXT("S") : TEXT(""), UGPMFactorySubsystem::ChiefAsk(Offer.C, y), Cost);
			Btns->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				Button(Label, y == 2 ? Acc : Acc2, [WF, Offer, y]() { if (WF.IsValid()) WF->Hire(Offer, y); }, Cost <= G->Budget + 1e-3f, 0.f, 50.f, 14)
			];
		}
		Det->AddSlot().AutoHeight()[ Btns ];
	}
	else Det->AddSlot().AutoHeight()[ Txt(TEXT("Choisis un candidat dans la liste."), 15, CDim) ];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 16.f, 0.f))
		[
			SNew(SBox).WidthOverride(430.f)[ Card(SNew(SScrollBox) + SScrollBox::Slot()[ DList ], CCardBg, 12.f) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))[ Card(Top, GPMUIQGText::Dim(DeptColor(Dept), 0.22f), 16.f) ]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 0.f, 0.f, 12.f))
			[
				Card(SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[ Txt(TEXT("CANDIDATS"), 13, CDim, true) ]
					+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ CList ] ],
					CCardBg, 14.f)
			]
			+ SVerticalBox::Slot().AutoHeight()[ Card(Det, FLinearColor(0.12f, 0.1f, 0.03f, 0.85f), 16.f) ]
		];
}
