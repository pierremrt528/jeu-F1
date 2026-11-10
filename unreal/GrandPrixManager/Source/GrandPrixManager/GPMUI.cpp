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
#include "Engine/Texture2D.h"
#include "TextureResource.h"

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

	/** Masque blanc généré au lancement (l'alpha donne la forme, la couleur vient de la teinte du pinceau). */
	UTexture2D* MakeMask(int32 W, int32 H, TFunctionRef<float(float, float)> AlphaAt)
	{
		TArray<FColor> Px;
		Px.SetNumUninitialized(W * H);
		for (int32 Py = 0; Py < H; ++Py)
			for (int32 Qx = 0; Qx < W; ++Qx)
			{
				const float A = FMath::Clamp(AlphaAt(float(Qx) + 0.5f, float(Py) + 0.5f), 0.f, 1.f);
				Px[Py * W + Qx] = FColor(255, 255, 255, uint8(FMath::RoundToInt(A * 255.f)));
			}
		UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
		if (!Tex || !Tex->GetPlatformData() || Tex->GetPlatformData()->Mips.Num() == 0) return nullptr;
		Tex->Filter = TF_Bilinear;
		Tex->AddressX = TA_Clamp;
		Tex->AddressY = TA_Clamp;
		FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
		void* Dst = Mip.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Dst, Px.GetData(), Px.Num() * sizeof(FColor));
		Mip.BulkData.Unlock();
		Tex->UpdateResource();
		Tex->AddToRoot();
		return Tex;
	}

	FSlateBrush MaskBrush(UTexture2D* Tex, float W, float H, bool bBox, const FMargin& Margin)
	{
		FSlateBrush B;
		if (Tex) B.SetResourceObject(Tex);
		B.ImageSize = FVector2D(W, H);
		B.DrawAs = Tex ? (bBox ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image) : ESlateBrushDrawType::NoDrawType;
		B.Margin = Margin;
		B.Tiling = ESlateBrushTileType::NoTile;
		return B;
	}

	/** Parallélogramme penché vers la droite (le haut décalé de Lean pixels), bords lissés. */
	float SlantAlpha(float Qx, float Py, float W, float H, float Lean)
	{
		const float V = Py / H;
		const float XL = Lean * (1.f - V), XR = W - Lean * V;
		return FMath::Clamp(Qx - XL + 0.5f, 0.f, 1.f) * FMath::Clamp(XR - Qx + 0.5f, 0.f, 1.f);
	}

	const FSlateBrush* Slant()
	{
		static FSlateBrush B = MaskBrush(MakeMask(128, 64, [](float Qx, float Py) { return SlantAlpha(Qx, Py, 128.f, 64.f, 20.f); }),
			128.f, 64.f, true, FMargin(22.f / 128.f, 0.f, 22.f / 128.f, 0.f));
		return &B;
	}

	const FSlateBrush* Tick()
	{
		static FSlateBrush B = MaskBrush(MakeMask(32, 64, [](float Qx, float Py) { return SlantAlpha(Qx, Py, 32.f, 64.f, 14.f); }), 32.f, 64.f, false, FMargin(0.f));
		return &B;
	}

	const FSlateBrush* Ramp(int32 Dir)
	{
		auto Fall = [](float U) { return 1.f - FMath::SmoothStep(0.f, 1.f, U); };
		static FSlateBrush R0 = MaskBrush(MakeMask(256, 4, [Fall](float Qx, float) { return Fall(Qx / 256.f); }), 256.f, 4.f, false, FMargin(0.f));
		static FSlateBrush R1 = MaskBrush(MakeMask(4, 256, [Fall](float, float Py) { return Fall(1.f - Py / 256.f); }), 4.f, 256.f, false, FMargin(0.f));
		static FSlateBrush R2 = MaskBrush(MakeMask(256, 4, [Fall](float Qx, float) { return Fall(1.f - Qx / 256.f); }), 256.f, 4.f, false, FMargin(0.f));
		static FSlateBrush R3 = MaskBrush(MakeMask(4, 256, [Fall](float, float Py) { return Fall(Py / 256.f); }), 4.f, 256.f, false, FMargin(0.f));
		return Dir == 1 ? &R1 : Dir == 2 ? &R2 : Dir == 3 ? &R3 : &R0;
	}

	const FSlateBrush* Glow()
	{
		static FSlateBrush B = MaskBrush(MakeMask(128, 128, [](float Qx, float Py)
		{
			const float D = FMath::Min(1.f, FVector2f(Qx - 64.f, Py - 64.f).Size() / 64.f);
			return FMath::Square(1.f - FMath::SmoothStep(0.f, 1.f, D));
		}), 128.f, 128.f, false, FMargin(0.f));
		return &B;
	}

	/** Bouton penché d'une couleur, avec sa couleur de survol (en cache, comme ButtonOf). */
	const FButtonStyle* SlantOf(const FLinearColor& C, const FLinearColor& Hover)
	{
		static TMap<uint64, TSharedPtr<FButtonStyle>> Cache;
		const uint64 Key = (uint64(C.ToFColor(false).DWColor()) << 32) | uint64(Hover.ToFColor(false).DWColor());
		if (const TSharedPtr<FButtonStyle>* Found = Cache.Find(Key)) return Found->Get();
		auto Tinted = [](const FLinearColor& Tint) { FSlateBrush B = *Slant(); B.TintColor = FSlateColor(Tint); return B; };
		TSharedPtr<FButtonStyle> St = MakeShared<FButtonStyle>();
		St->SetNormal(Tinted(C));
		St->SetHovered(Tinted(Hover));
		St->SetPressed(Tinted(Lift(C, 0.8f)));
		St->SetDisabled(Tinted(FLinearColor(0.13f, 0.14f, 0.17f, 0.8f)));
		St->SetNormalPadding(FMargin(0.f));
		St->SetPressedPadding(FMargin(0.f));
		Cache.Add(Key, St);
		return St.Get();
	}

	FString Abbr(const FString& Name)
	{
		// sigle de 3 lettres : initiales des mots, complétées par les lettres du dernier mot (ou les 3 premières d'un seul mot)
		TArray<FString> Words;
		Name.ParseIntoArrayWS(Words);
		FString Out, Rest;
		for (const FString& Wd : Words)
		{
			FString Letters;
			for (TCHAR Ch : Wd) if (FChar::IsAlpha(Ch)) Letters.AppendChar(Ch);
			if (Letters.IsEmpty()) continue;
			if (Out.Len() < 3) Out.AppendChar(Letters[0]);
			Rest = Letters.Mid(1);
		}
		if (Out.Len() == 1) Out += Rest.Left(2);
		else if (Out.Len() == 2) Out += Rest.Left(1);
		return Out.IsEmpty() ? FString(TEXT("GPM")) : Out.ToUpper();
	}

	FSlateFontInfo Face(int32 Kind, int32 Size)
	{
		static const FName Names[4] = { FName("BlackItalic"), FName("BoldItalic"), FName("BoldCondensed"), FName("Light") };
		FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Names[FMath::Clamp(Kind, 0, 3)], Size);
		if (Kind == 2) F.LetterSpacing = 140;
		return F;
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
	H = HashCombine(H, GetTypeHash(QGTab + SeasonTab * 4 + SelInfra * 16 + SelDept * 256 + SelCand * 4096));
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
			for (int32 k = 0; k < Dl->Choices.Num() && k < 6; ++k)
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
		// Tab : classements, calendrier, performance
		if (PC && PC->WasInputKeyJustPressed(EKeys::Tab)) { SeasonTab = (SeasonTab + 1) % 3; Touch(); }
	}
	else if (GM->Phase == EP::Factory && PC)
	{
		// QG au clavier : Tab change d'onglet, flèches pour choisir, Entrée (ou 1-3 pour la durée d'un chef) pour agir
		UGPMFactorySubsystem* F = FactorySys();
		if (!F) return;
		if (PC->WasInputKeyJustPressed(EKeys::Tab)) { QGTab = (QGTab + 1) % 3; Touch(); return; }
		const bool bL = PC->WasInputKeyJustPressed(EKeys::Left), bR = PC->WasInputKeyJustPressed(EKeys::Right);
		const bool bU = PC->WasInputKeyJustPressed(EKeys::Up), bD = PC->WasInputKeyJustPressed(EKeys::Down);
		const bool bEnter = PC->WasInputKeyJustPressed(EKeys::Enter);
		if (QGTab == 0)
		{
			const int32 NT = F->Trees().Num();
			if (NT == 0) return;
			if (bL) { SelTree = (SelTree + NT - 1) % NT; SelPart = 0; }
			if (bR) { SelTree = (SelTree + 1) % NT; SelPart = 0; }
			SelTree = FMath::Clamp(SelTree, 0, NT - 1);
			const TArray<FGPMPart>& Parts = F->Trees()[SelTree].Parts;
			if (bU) SelPart = FMath::Max(0, SelPart - 1);
			if (bD) SelPart = FMath::Min(Parts.Num() - 1, SelPart + 1);
			if (bEnter && Parts.IsValidIndex(SelPart) && F->StateOf(Parts[SelPart]) == UGPMFactorySubsystem::EState::Available) { F->Start(Parts[SelPart].Id); Touch(); }
		}
		else if (QGTab == 1)
		{
			const int32 NB = F->Buildings().Num();
			if (NB == 0) return;
			if (bU) { SelInfra = (SelInfra + NB - 1) % NB; Touch(); }
			if (bD) { SelInfra = (SelInfra + 1) % NB; Touch(); }
			SelInfra = FMath::Clamp(SelInfra, 0, NB - 1);
			const FString BId = F->Buildings()[SelInfra].Id;
			FString Why;
			if (bEnter && F->CanBuild(BId, Why)) { F->Build(BId); Touch(); }
		}
		else
		{
			const int32 ND = UGPMFactorySubsystem::Depts().Num();
			if (ND == 0) return;
			if (bL || bR) { SelDept = (SelDept + (bL ? ND - 1 : 1)) % ND; SelCand = 0; Touch(); }
			SelDept = FMath::Clamp(SelDept, 0, ND - 1);
			const TArray<FGPMChiefOffer> Cands = F->Candidates(UGPMFactorySubsystem::Depts()[SelDept].Key);
			if (bU && SelCand > 0) { --SelCand; Touch(); }
			if (bD && SelCand < Cands.Num() - 1) { ++SelCand; Touch(); }
			if (Cands.IsValidIndex(SelCand))
				for (int32 y = 1; y <= 3; ++y)
					if (AGPMGameMode::DigitPressed(PC, y) && F->Hire(Cands[SelCand], y)) { SelCand = 0; Touch(); break; }
		}
	}
}

bool FGPMUI::Update(AGPMGameMode* GM)
{
	AGPMHUD* H = Hud.Get();
	if (!H || !GM) return false;
	using EP = AGPMGameMode::EPhase;
	using EC = AGPMGameMode::ECine;
	const bool bCine = GM->Cine != EC::None && GM->Cine != EC::Menu;
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
	// bouton penché : libellé gras italique, s'éclaircit au survol
	return Hit(SNew(SBox).HAlign(HAlign_Center)[ Ital(Label, Size, bEnabled ? InkOn(Col) : CFaint, 1, true) ],
		Col, GPMUIStyle::Lift(Col, 1.22f), OnClick, bEnabled, MinW, Height, FMargin(Label.Len() <= 2 ? 12.f : 26.f, 2.f));
}

TSharedRef<SWidget> FGPMUI::Hit(const TSharedRef<SWidget>& Content, const FLinearColor& Bg, const FLinearColor& Hover, TFunction<void()> OnClick, bool bEnabled, float MinW, float Height, const FMargin& Pad)
{
	TWeakPtr<FGPMUI> Self = AsShared();
	return SNew(SBox)
		.MinDesiredWidth(MinW)
		.HeightOverride(Height > 0.f ? FOptionalSize(Height) : FOptionalSize())
		[
			SNew(SButton)
			.ButtonStyle(GPMUIStyle::SlantOf(Bg, Hover))
			.IsFocusable(false)
			.IsEnabled(bEnabled)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.ContentPadding(Pad)
			.OnClicked_Lambda([Self, OnClick]()
			{
				if (TSharedPtr<FGPMUI> Me = Self.Pin()) { Me->Click(); if (OnClick) OnClick(); Me->Touch(); }
				return FReply::Handled();
			})
			[
				Content
			]
		];
}

void FGPMUI::Click() const
{
	const AGPMHUD* H = Hud.Get();
	UWorld* W = H ? H->GetWorld() : nullptr;
	if (AGPMGameMode* GM = W ? W->GetAuthGameMode<AGPMGameMode>() : nullptr) GM->UiClick();
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
			if (TSharedPtr<FGPMUI> Me = Self.Pin()) { Me->Click(); if (OnClick) OnClick(); Me->Touch(); }
			return FReply::Handled();
		})
		[
			Content
		];
}

TSharedRef<SWidget> FGPMUI::Chip(const FString& Label, const FString& Value, const FLinearColor& Col) const
{
	return SNew(SBorder)
		.BorderImage(GPMUIStyle::Slant())
		.BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.08f)))
		.Padding(FMargin(22.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				SNew(STextBlock).Text(FText::FromString(Label.ToUpper())).Font(GPMUIStyle::Face(2, 11)).ColorAndOpacity(FSlateColor(CDim))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Ital(Value, 16, Col, 1)
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
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
		[
			Ticks(Acc, 3, 9.f, 50.f)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Kicker.ToUpper())).Font(GPMUIStyle::Face(2, 13)).ColorAndOpacity(FSlateColor(Bright()))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -4.f, 0.f, 0.f))[ Ital(Main, 36, CText) ]
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
	// fond de l'écran, filet aux couleurs de l'écurie en haut qui s'efface vers la droite
	return SNew(SOverlay)
		+ SOverlay::Slot()[ Layer(GPMUIStyle::Rounded(0.f), Bg) ]
		+ SOverlay::Slot().VAlign(VAlign_Top)[ SNew(SBox).HeightOverride(4.f)[ Layer(GPMUIStyle::Ramp(0), Acc) ] ]
		+ SOverlay::Slot().Padding(FMargin(36.f, 26.f))[ Body ];
}

TSharedRef<SWidget> FGPMUI::Row(const FLinearColor& TeamCol, bool bMe, const TArray<TPair<FString, float>>& Cells, const TArray<FLinearColor>& Colors) const
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			SNew(SBox).WidthOverride(9.f).HeightOverride(24.f)[ Layer(GPMUIStyle::Tick(), TeamCol) ]
		];
	for (int32 k = 0; k < Cells.Num(); ++k)
	{
		const FLinearColor Col = Colors.IsValidIndex(k) ? Colors[k] : CText;
		// première colonne (place) en gras italique, comme sur un tableau de chronométrage
		TSharedRef<SWidget> Cell = k == 0 ? Ital(Cells[k].Key, 17, bMe ? FLinearColor::White : Col, 0) : Txt(Cells[k].Key, 16, bMe ? FLinearColor::White : Col, bMe);
		if (Cells[k].Value > 0.f)
			Box->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				// colonne de largeur fixe : un nom trop long est coupé au bord au lieu d'empiéter sur la colonne suivante
				SNew(SBox).WidthOverride(Cells[k].Value).Clipping(EWidgetClipping::ClipToBounds).Padding(FMargin(0.f, 0.f, 8.f, 0.f))[ Cell ]
			];
		else
			Box->AddSlot().FillWidth(1.f).VAlign(VAlign_Center)[ Cell ];
	}
	TSharedRef<SOverlay> Ov = SNew(SOverlay) + SOverlay::Slot()[ Layer(GPMUIStyle::Rounded(0.f), FLinearColor(1.f, 1.f, 1.f, bMe ? 0.07f : 0.035f)) ];
	if (bMe) Ov->AddSlot()[ Layer(GPMUIStyle::Ramp(0), FLinearColor(Acc.R, Acc.G, Acc.B, 0.62f)) ];
	Ov->AddSlot().Padding(FMargin(10.f, 5.f))[ Box ];
	return Ov;
}

// ---------- charte « course » ----------

FLinearColor FGPMUI::Bright() const
{
	// accent lisible sur fond sombre : une couleur trop foncée est éclaircie
	FLinearColor C = Acc;
	for (int32 k = 0; k < 4 && C.GetLuminance() < 0.22f; ++k) C = GPMUIStyle::Lift(C, 1.45f);
	return C;
}

TSharedRef<SWidget> FGPMUI::Ital(const FString& S, int32 Size, const FLinearColor& Col, int32 Kind, bool bCenter) const
{
	return SNew(STextBlock)
		.Text(FText::FromString(S))
		.Font(GPMUIStyle::Face(Kind, Size))
		.ColorAndOpacity(FSlateColor(Col))
		.Justification(bCenter ? ETextJustify::Center : ETextJustify::Left);
}

TSharedRef<SWidget> FGPMUI::Layer(const FSlateBrush* Brush, const FLinearColor& Col) const
{
	return SNew(SBorder)
		.BorderImage(Brush)
		.BorderBackgroundColor(FSlateColor(Col))
		.Padding(FMargin(0.f))
		.Visibility(EVisibility::HitTestInvisible);
}

TSharedRef<SWidget> FGPMUI::Ticks(const FLinearColor& Col, int32 Count, float W, float H) const
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	for (int32 k = 0; k < Count; ++k)
		Box->AddSlot().AutoWidth().Padding(FMargin(k ? -1.f : 0.f, 0.f, 0.f, 0.f))
		[
			SNew(SBox).WidthOverride(W).HeightOverride(H)[ Layer(GPMUIStyle::Tick(), FLinearColor(Col.R, Col.G, Col.B, 1.f - 0.32f * k)) ]
		];
	return Box;
}

TSharedRef<SWidget> FGPMUI::Keycap(const FString& Key, const FString& What) const
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 7.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(GPMUIStyle::Rounded(6.f))
			.BorderBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.14f)))
			.Padding(FMargin(8.f, 2.f))
			[
				SNew(STextBlock).Text(FText::FromString(Key)).Font(GPMUIStyle::Face(2, 12)).ColorAndOpacity(FSlateColor(CText))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			Txt(What, 13, CDim)
		];
}

TSharedRef<SWidget> FGPMUI::StatBlock(const FString& Label, const FString& Value, const FString& Sub, const FLinearColor& Col, float Frac) const
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(Label.ToUpper())).Font(GPMUIStyle::Face(2, 11)).ColorAndOpacity(FSlateColor(CDim))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -2.f, 0.f, 0.f))[ Ital(Value, 28, Col) ];
	if (!Sub.IsEmpty()) Box->AddSlot().AutoHeight().Padding(FMargin(0.f, -2.f, 0.f, 0.f))[ Txt(Sub, 12, CFaint) ];
	if (Frac >= 0.f) Box->AddSlot().AutoHeight().Padding(FMargin(0.f, 5.f, 0.f, 0.f))[ Bar(Frac, Col, 4.f) ];
	return SNew(SOverlay)
		+ SOverlay::Slot()[ Layer(GPMUIStyle::Rounded(0.f), FLinearColor(0.02f, 0.025f, 0.045f, 0.7f)) ]
		+ SOverlay::Slot()[ Layer(GPMUIStyle::Ramp(3), FLinearColor(1.f, 1.f, 1.f, 0.05f)) ]
		+ SOverlay::Slot().HAlign(HAlign_Left)[ SNew(SBox).WidthOverride(3.f)[ Layer(GPMUIStyle::Rounded(0.f), Col) ] ]
		+ SOverlay::Slot().Padding(FMargin(14.f, 8.f, 12.f, 9.f))[ Box ];
}

TSharedRef<SWidget> FGPMUI::MenuTile(const FString& Num, const FString& Label, const FString& Sub, const FLinearColor& Col, TFunction<void()> OnClick, bool bPrimary)
{
	// grande entrée du menu : numéro, libellé penché, sous-titre ; la couleur de l'écurie apparaît au survol
	const FLinearColor Bg = bPrimary ? Col : FLinearColor(0.03f, 0.035f, 0.06f, 0.78f);
	const FLinearColor Hover = bPrimary ? GPMUIStyle::Lift(Col, 1.18f) : FLinearColor(Col.R * 0.5f, Col.G * 0.5f, Col.B * 0.5f, 0.92f);
	const FLinearColor Ink = bPrimary ? InkOn(Col) : CText;
	const FLinearColor Soft(Ink.R, Ink.G, Ink.B, 0.66f);
	TSharedRef<SWidget> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 18.f, 0.f))
		[
			SNew(SBox).WidthOverride(44.f)[ Ital(Num, 22, bPrimary ? Soft : Bright(), 0) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Ital(Label, bPrimary ? 26 : 22, Ink, 0) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -2.f, 0.f, 0.f))[ Txt(Sub, 13, Soft) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
		[
			Ital(TEXT("›"), 34, bPrimary ? Ink : Soft, 1)
		];
	return Hit(Content, Bg, Hover, OnClick, true, 0.f, bPrimary ? 86.f : 70.f, FMargin(34.f, 4.f, 30.f, 4.f));
}

// ---------- accueil ----------

TSharedRef<SWidget> FGPMUI::HomeScreen(AGPMGameMode* GM)
{
	// accueil : la voiture de l'écurie dans l'atelier à droite, logo et grandes entrées numérotées à gauche
	UGPMCareerSubsystem* C = CareerSys();
	UGPMSeasonSubsystem* S = SeasonSys();
	UGPMDataSubsystem* Data = DataSys();
	TWeakObjectPtr<AGPMGameMode> WGM = GM;
	const UGPMSaveGame* Sv = C ? C->Save() : nullptr;
	const bool bHasCareer = C && C->IsCreated() && S && Sv;
	const FLinearColor Shade(0.006f, 0.008f, 0.018f, 0.92f);

	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	int32 NumTile = 0;
	auto Add = [&Menu, &NumTile, this](const FString& Label, const FString& Sub, const FLinearColor& Col, TFunction<void()> OnClick, bool bPrimary)
	{
		Menu->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			SNew(SBox).WidthOverride(540.f)[ MenuTile(FString::Printf(TEXT("%02d"), ++NumTile), Label, Sub, Col, MoveTemp(OnClick), bPrimary) ]
		];
	};
	if (bHasCareer)
	{
		Add(TEXT("CONTINUER"), FString::Printf(TEXT("%s · Grand Prix %d / %d"), *Sv->TeamName, FMath::Min(S->Round() + 1, S->NumRounds()), S->NumRounds()), Acc,
			[this, WGM]() { bConfirmNew = false; if (WGM.IsValid()) WGM->ContinueCareer(); }, true);
		if (bConfirmNew)
			Add(TEXT("EFFACER ET RECOMMENCER"), TEXT("Confirme : la carrière en cours sera perdue"), FLinearColor(0.62f, 0.08f, 0.08f, 1.f),
				[this, WGM]() { bConfirmNew = false; if (WGM.IsValid()) WGM->NewCareer(); }, true);
		else
			Add(TEXT("NOUVELLE CARRIÈRE"), TEXT("Repartir de zéro avec une autre écurie"), Acc, [this]() { bConfirmNew = true; }, false);
	}
	else
		Add(TEXT("CARRIÈRE"), TEXT("Crée ton écurie, développe ta voiture et vise le titre"), Acc, [WGM]() { if (WGM.IsValid()) WGM->NewCareer(); }, true);
	Add(TEXT("COURSE SOLO"), TEXT("Un Grand Prix sur le circuit de ton choix"), Acc2, [this, WGM]() { bConfirmNew = false; if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Solo; }, false);
	Add(TEXT("RÉGLAGES"), TEXT("Graphismes, son, aides au pilotage, niveau des IA"), Acc2, [this, WGM]() { bConfirmNew = false; if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Settings; }, false);

	// logo : deux lignes en très gras italique serrées l'une contre l'autre
	const FString Kick = bHasCareer ? FString::Printf(TEXT("SAISON %d  ·  %s"), S->Year(), *Sv->TeamName.ToUpper()) : FString(TEXT("NOUVELLE SAISON"));
	TSharedRef<SWidget> Logo = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))[ Ticks(Acc, 3, 8.f, 22.f) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(Kick)).Font(GPMUIStyle::Face(2, 14)).ColorAndOpacity(FSlateColor(Bright()))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f, 0.f, 0.f))[ Ital(TEXT("GRAND PRIX"), 74, CText) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -27.f, 0.f, 0.f))[ Ital(TEXT("MANAGER"), 74, Bright()) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(6.f, -4.f, 0.f, 0.f))
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("COURSE DE MONOPLACES  ·  GESTION D'ÉCURIE"))).Font(GPMUIStyle::Face(2, 13)).ColorAndOpacity(FSlateColor(CDim))
		];

	TSharedRef<SHorizontalBox> Keys = SNew(SHorizontalBox);
	const TCHAR* const KeyList[][2] = { { TEXT("ZQSD"), TEXT("conduire") }, { TEXT("C"), TEXT("caméra") }, { TEXT("B"), TEXT("stand") }, { TEXT("P"), TEXT("pause") },
		{ TEXT("ENTRÉE"), TEXT("valider") }, { TEXT("ÉCHAP"), TEXT("retour") } };
	for (const auto& Kc : KeyList) Keys->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 16.f, 0.f))[ Keycap(Kc[0], Kc[1]) ];

	TSharedRef<SWidget> Left = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Logo ]
		+ SVerticalBox::Slot().FillHeight(1.f).VAlign(VAlign_Center).Padding(FMargin(0.f, 26.f, 0.f, 20.f))[ Menu ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[ Keys ]
		+ SVerticalBox::Slot().AutoHeight()[ Txt(TEXT("Univers 100 % fictif : écuries, pilotes et circuits inventés."), 12, CFaint) ];

	// carte de la carrière en bas à droite : écurie, places au championnat, prochain Grand Prix et son tracé
	TSharedRef<SWidget> Right = SNullWidget::NullWidget;
	if (bHasCareer)
	{
		int32 MyPos = 0, MyPts = 0, TeamPos = 0, TeamPts = 0;
		const TArray<FGPMStanding> Drv = S->Drivers(), Tms = S->Teams();
		for (int32 k = 0; k < Drv.Num(); ++k) if (Drv[k].bPlayer) { MyPos = k + 1; MyPts = Drv[k].Pts; break; }
		for (int32 k = 0; k < Tms.Num(); ++k) if (Tms[k].bPlayer) { TeamPos = k + 1; TeamPts = Tms[k].Pts; break; }
		const FString NextId = S->IsOver() ? FString() : S->CurrentCircuit();
		const FGPMTrackData* T = Data && !NextId.IsEmpty() ? Data->GetTrack(NextId) : nullptr;
		auto Mini = [this](const FString& Label, const FString& Value, const FString& Sub) -> TSharedRef<SWidget>
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(Label)).Font(GPMUIStyle::Face(2, 11)).ColorAndOpacity(FSlateColor(CDim)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -3.f, 0.f, 0.f))[ Ital(Value, 30, CText) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -3.f, 0.f, 0.f))[ Txt(Sub, 12, CFaint) ];
		};
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 14.f, 0.f))
				[
					SNew(SBox).WidthOverride(76.f).HeightOverride(40.f)
					[
						SNew(SBorder).BorderImage(GPMUIStyle::Slant()).BorderBackgroundColor(FSlateColor(Acc)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f))
						[ Ital(GPMUIStyle::Abbr(Sv->TeamName), 18, InkOn(Acc)) ]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(TEXT("TON ÉCURIE"))).Font(GPMUIStyle::Face(2, 11)).ColorAndOpacity(FSlateColor(CDim)) ]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -3.f, 0.f, 0.f))[ Ital(Sv->TeamName, 24, CText) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)[ Mini(TEXT("PILOTE"), MyPos ? FString::Printf(TEXT("P%d"), MyPos) : FString(TEXT("—")), FString::Printf(TEXT("%d pts"), MyPts)) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)[ Mini(TEXT("ÉCURIE"), TeamPos ? FString::Printf(TEXT("P%d"), TeamPos) : FString(TEXT("—")), FString::Printf(TEXT("%d pts"), TeamPts)) ]
				+ SHorizontalBox::Slot().FillWidth(1.f)[ Mini(TEXT("BUDGET"), FString::Printf(TEXT("%.1f"), Sv->Budget), TEXT("M€")) ]
			];
		if (T)
			Box->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("PROCHAIN GRAND PRIX · %d / %d"), S->Round() + 1, S->NumRounds())))
						.Font(GPMUIStyle::Face(2, 11)).ColorAndOpacity(FSlateColor(Bright()))
					]
					+ SVerticalBox::Slot().AutoHeight()[ Ital(T->Name.ToUpper(), 20, CText) ]
					+ SVerticalBox::Slot().AutoHeight()[ Txt(FString::Printf(TEXT("%s%s%.2f km"), *T->Type, T->Type.IsEmpty() ? TEXT("") : TEXT(" · "), T->WX.Num() * 2.f / 1000.f), 12, CDim) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))[ TrackMap(NextId, Bright(), 130.f, 86.f) ]
			];
		Right = SNew(SBox).WidthOverride(430.f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ Layer(GPMUIStyle::Rounded(0.f), FLinearColor(0.015f, 0.02f, 0.04f, 0.8f)) ]
				+ SOverlay::Slot()[ Layer(GPMUIStyle::Ramp(0), FLinearColor(Acc.R, Acc.G, Acc.B, 0.22f)) ]
				+ SOverlay::Slot().VAlign(VAlign_Top)[ SNew(SBox).HeightOverride(3.f)[ Layer(GPMUIStyle::Rounded(0.f), Acc) ] ]
				+ SOverlay::Slot().Padding(FMargin(22.f, 18.f, 22.f, 16.f))[ Box ]
			];
	}

	return SNew(SOverlay)
		// ombre à gauche derrière le menu, la voiture reste bien visible à droite ; voile en bas pour la lisibilité
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.36f)[ Layer(GPMUIStyle::Rounded(0.f), Shade) ]
			+ SHorizontalBox::Slot().FillWidth(0.34f)[ Layer(GPMUIStyle::Ramp(0), Shade) ]
			+ SHorizontalBox::Slot().FillWidth(0.3f)[ SNew(SSpacer) ]
		]
		+ SOverlay::Slot().VAlign(VAlign_Bottom)[ SNew(SBox).HeightOverride(260.f)[ Layer(GPMUIStyle::Ramp(1), FLinearColor(0.f, 0.f, 0.01f, 0.78f)) ] ]
		+ SOverlay::Slot().VAlign(VAlign_Top)[ SNew(SBox).HeightOverride(5.f)[ Layer(GPMUIStyle::Ramp(0), Acc) ] ]
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(76.f, 52.f, 40.f, 30.f))[ Left ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Bottom).HAlign(HAlign_Right).Padding(FMargin(30.f, 30.f, 44.f, 40.f))[ Right ]
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
	// météo : au hasard, sec, pluie forte, pluie fine (intermédiaires), averse en cours de course
	static const TCHAR* WxOpts[] = { TEXT("Au hasard"), TEXT("Sec"), TEXT("Pluie"), TEXT("Pluie fine"), TEXT("Averse") };
	static const int32 WxVals[] = { 0, 1, 2, 5, 3 };
	for (int32 k = 0; k < 5; ++k)
	{
		const int32 Wv = WxVals[k];
		Wx->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))
		[
			Button(WxOpts[k], GM->SoloWeather == Wv ? Acc : CNeutral, [WGM, Wv]() { if (WGM.IsValid()) WGM->SoloWeather = Wv; }, true, 130.f, 46.f, 16)
		];
	}

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
		{ TEXT("Boutons tactiles à l'écran"), TEXT("Gauche, droite, frein, gaz et stand (touche T)."), &H->bShowTouch },
		{ TEXT("Son"), TEXT("Moteurs, radio du muret, chocs, foule et clics des menus."), &GM->bSetSound } };
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
	// niveau des pilotes IA : 5 crans, du débutant à l'expert
	TSharedRef<SHorizontalBox> Levels = SNew(SHorizontalBox);
	for (int32 k = 0; k < 5; ++k)
		Levels->AddSlot().AutoWidth().Padding(FMargin(k ? 6.f : 0.f, 0.f, 0.f, 0.f))
		[
			Button(AGPMGameMode::AILevelName(k), GM->AILevel == k ? Acc : CNeutral, [WGM, k]() { if (WGM.IsValid()) WGM->SetAILevel(k); }, true, 100.f, 44.f, 14)
		];
	static const TCHAR* const LevelHelp[5] = {
		TEXT("Les IA roulent nettement moins vite : idéal pour apprendre les circuits."),
		TEXT("Les IA laissent un peu de marge."),
		TEXT("Le niveau prévu pour chaque pilote."),
		TEXT("Les IA attaquent plus fort, chaque dixième compte."),
		TEXT("Les IA roulent à la limite : pour les meilleurs pilotes.") };
	List->AddSlot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))
	[
		Card(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Txt(TEXT("Niveau des pilotes IA"), 20, CText, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ Txt(LevelHelp[FMath::Clamp(GM->AILevel, 0, 4)], 14, CDim, false, true) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Levels ],
			CCardBg, 14.f)
	];
	TSharedRef<SWidget> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 24.f))[ Title(TEXT("PILOTAGE ET AFFICHAGE"), TEXT("Réglages"), SNullWidget::NullWidget) ]
		+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SScrollBox) + SScrollBox::Slot()[ List ] ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 16.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Button(TEXT("RETOUR"), CNeutral, [WGM]() { if (WGM.IsValid()) WGM->Phase = AGPMGameMode::EPhase::Home; }, true, 180.f) ]
		];
	return SNew(SBox).HAlign(HAlign_Center).Padding(FMargin(0.f, 30.f))[ SNew(SBox).WidthOverride(1060.f)[ Page(Body, FLinearColor(0.01f, 0.012f, 0.025f, 0.88f)) ] ];
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
