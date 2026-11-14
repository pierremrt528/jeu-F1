#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SWidget.h"
#include "Fonts/SlateFontInfo.h"

class AGPMHUD;
class AGPMGameMode;
class UGPMCareerSubsystem;
class UGPMSeasonSubsystem;
class SBox;
class UTexture;
struct FSlateBrush;
struct FButtonStyle;

/** Pinceaux arrondis et styles de boutons partagés (GPMUI.cpp). */
namespace GPMUIStyle
{
	const FSlateBrush* Rounded(float Radius);
	FLinearColor Lift(const FLinearColor& C, float K);
	const FButtonStyle* ButtonOf(const FLinearColor& C, bool bOutline);
	// Formes générées au lancement (masques blancs teintés par la bordure) : charte « course » des écrans
	const FSlateBrush* Slant();            // parallélogramme penché, bouts fixes (boutons, étiquettes, badges)
	const FSlateBrush* Tick();             // trait penché étiré (accents des titres, couleur d'écurie)
	const FSlateBrush* Ramp(int32 Dir);    // dégradé : 0 opaque à gauche, 1 opaque en bas, 2 opaque à droite, 3 opaque en haut
	const FSlateBrush* Glow();             // halo rond
	const FButtonStyle* SlantOf(const FLinearColor& C, const FLinearColor& Hover);
	/** Polices : 0 très gras italique (titres), 1 gras italique, 2 étiquette condensée espacée, 3 fin. */
	FSlateFontInfo Face(int32 Kind, int32 Size);
	FString Abbr(const FString& TeamName);   // sigle de 3 lettres de l'écurie (badge)
	const FSlateBrush* Disc();             // disque plein (voyants, icônes)
	/** Textures des masques pour le HUD dessiné (Canvas) : 0 penché (petit, pour les bandeaux du HUD), 1 trait penché,
	 *  2 dégradé opaque à gauche, 3 halo, 4 disque, 5 dégradé opaque en bas. */
	UTexture* MaskTex(int32 Which);
}

/** Couleurs communes des écrans (thème sombre ; l'accent suit la couleur principale de l'écurie). */
namespace GPMUIColor
{
	inline const FLinearColor CPageBg(0.012f, 0.016f, 0.03f, 0.9f);
	inline const FLinearColor CRoomBg(0.012f, 0.016f, 0.03f, 0.74f);   // écrans devant l'atelier 3D : la voiture se devine derrière
	inline const FLinearColor CPanel(0.04f, 0.05f, 0.08f, 0.94f);
	inline const FLinearColor CCardBg(1.f, 1.f, 1.f, 0.06f);
	inline const FLinearColor CCardHi(1.f, 1.f, 1.f, 0.11f);
	inline const FLinearColor CText(0.95f, 0.96f, 0.98f, 1.f);
	inline const FLinearColor CDim(0.72f, 0.75f, 0.82f, 1.f);
	inline const FLinearColor CFaint(0.5f, 0.53f, 0.6f, 1.f);
	inline const FLinearColor CGold(1.f, 0.8f, 0.22f, 1.f);
	inline const FLinearColor CGood(0.35f, 0.9f, 0.5f, 1.f);
	inline const FLinearColor CBad(1.f, 0.38f, 0.3f, 1.f);
	inline const FLinearColor CInfo(0.45f, 0.75f, 1.f, 1.f);
	inline const FLinearColor CPurple(0.72f, 0.42f, 1.f, 1.f);
	inline const FLinearColor CNeutral(0.2f, 0.23f, 0.3f, 1.f);
}

/**
 * Écrans de gestion en Slate (l'interface native d'Unreal, écrite en C++, sans fichier à créer dans l'éditeur) :
 * vraie police, cartes arrondies, boutons qui réagissent au survol et au toucher, listes qui défilent.
 * L'écran de la phase en cours est reconstruit quand l'état change (clic, touche, changement de phase).
 * La course, les cinématiques et le menu pause restent dessinés par AGPMHUD.
 */
class FGPMUI : public TSharedFromThis<FGPMUI>
{
public:
	explicit FGPMUI(AGPMHUD* InHud);
	/** À chaque image : affiche (ou masque) l'écran de la phase en cours. Renvoie vrai si un écran est affiché. */
	bool Update(AGPMGameMode* GM);
	void Shutdown();

private:
	TWeakObjectPtr<AGPMHUD> Hud;
	TSharedPtr<SBox> Root;
	uint32 Shown = 0;
	int32 Version = 0;
	bool bAdded = false;
	bool bConfirmNew = false;
	int32 SelTree = 0, SelPart = 0, SeasonTab = 0;
	int32 QGTab = 0, SelInfra = 0, SelDept = 0, SelCand = 0;   // QG : onglet, bâtiment, pôle et candidat choisis
	int32 KeyPhase = -1;   // phase vue à l'image précédente : une touche qui change d'écran ne compte pas deux fois
	FLinearColor Acc = FLinearColor(0.85f, 0.12f, 0.12f, 1.f);
	FLinearColor Acc2 = FLinearColor(0.15f, 0.4f, 0.75f, 1.f);

	void Touch() { ++Version; }
	void Click() const;   // petit clic sonore des boutons
	uint32 Signature(AGPMGameMode* GM) const;
	void HandleKeys(AGPMGameMode* GM);
	TSharedRef<SWidget> Build(AGPMGameMode* GM);
	UGPMCareerSubsystem* CareerSys() const;
	UGPMSeasonSubsystem* SeasonSys() const;
	class UGPMDataSubsystem* DataSys() const;
	class UGPMFactorySubsystem* FactorySys() const;
	FString Nm(const FString& Driver) const;
	static FString Clock(float Seconds);

	// --- briques ---
	static FLinearColor InkOn(const FLinearColor& Bg);
	TSharedRef<SWidget> Txt(const FString& S, int32 Size, const FLinearColor& Col, bool bBold = false, bool bWrap = false, bool bCenter = false) const;
	TSharedRef<SWidget> Card(const TSharedRef<SWidget>& Content, const FLinearColor& Bg, float Pad = 18.f) const;
	TSharedRef<SWidget> Button(const FString& Label, const FLinearColor& Col, TFunction<void()> OnClick, bool bEnabled = true, float MinW = 0.f, float Height = 52.f, int32 Size = 18);
	TSharedRef<SWidget> Pick(const TSharedRef<SWidget>& Content, bool bSel, TFunction<void()> OnClick, bool bEnabled = true);
	TSharedRef<SWidget> Chip(const FString& Label, const FString& Value, const FLinearColor& Col) const;
	TSharedRef<SWidget> Dot(const FLinearColor& Col, float Size = 12.f) const;
	TSharedRef<SWidget> Bar(float Frac, const FLinearColor& Col, float Height = 6.f) const;
	TSharedRef<SWidget> Title(const FString& Kicker, const FString& Main, const TSharedRef<SWidget>& Right) const;
	TSharedRef<SWidget> Chips() const;
	TSharedRef<SWidget> Page(const TSharedRef<SWidget>& Body, const FLinearColor& Bg) const;
	TSharedRef<SWidget> Row(const FLinearColor& TeamCol, bool bMe, const TArray<TPair<FString, float>>& Cells, const TArray<FLinearColor>& Colors) const;
	TSharedRef<SWidget> Tabs(const TArray<FString>& Labels, int32 Sel, TFunction<void(int32)> OnPick);
	// --- charte « course » : textes penchés, formes obliques, dégradés ---
	TSharedRef<SWidget> Ital(const FString& S, int32 Size, const FLinearColor& Col, int32 Kind = 0, bool bCenter = false) const;
	TSharedRef<SWidget> Layer(const FSlateBrush* Brush, const FLinearColor& Col) const;
	TSharedRef<SWidget> Ticks(const FLinearColor& Col, int32 Count, float W, float H) const;
	TSharedRef<SWidget> Keycap(const FString& Key, const FString& What) const;
	TSharedRef<SWidget> StatBlock(const FString& Label, const FString& Value, const FString& Sub, const FLinearColor& Col, float Frac = -1.f) const;
	TSharedRef<SWidget> Hit(const TSharedRef<SWidget>& Content, const FLinearColor& Bg, const FLinearColor& Hover, TFunction<void()> OnClick, bool bEnabled, float MinW, float Height, const FMargin& Pad);
	TSharedRef<SWidget> MenuTile(const FString& Num, const FString& Label, const FString& Sub, const FLinearColor& Col, TFunction<void()> OnClick, bool bPrimary);
	TSharedRef<SWidget> TrackMap(const FString& CircuitId, const FLinearColor& Col, float W, float H);
	TSharedRef<SWidget> HeroNext(AGPMGameMode* GM);
	FLinearColor Bright() const;   // accent assez clair pour du texte sur fond sombre

	// --- écrans ---
	TSharedRef<SWidget> HomeScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> SetupScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> SoloScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> SettingsScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> DialogScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> MercatoScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> FiredScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> FactoryScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> ResearchTab();
	TSharedRef<SWidget> InfraTab();
	TSharedRef<SWidget> ChiefsTab();
	TSharedRef<SWidget> SeasonScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> StandingsPanel();
	TSharedRef<SWidget> CalendarPanel();
	TSharedRef<SWidget> PerfPanel();
	TSharedRef<SWidget> MissionCard();
	TSharedRef<SWidget> ChampScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> QualiScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> ResultsScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> TuningScreen(AGPMGameMode* GM);
	TSharedRef<SWidget> PreraceScreen(AGPMGameMode* GM);
};
