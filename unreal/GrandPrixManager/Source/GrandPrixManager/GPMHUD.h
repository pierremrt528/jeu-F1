#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
class FGPMUI;

#include "GPMHUD.generated.h"

/**
 * HUD de course dessiné directement (sans assets) : vitesse, rapport, tour, chronos, mini-carte (comme js/hud.js),
 * et commandes tactiles multi-touch (gauche / droite à gauche de l'écran, frein / gaz à droite).
 * Sur Mac, les boutons s'utilisent à la souris (touche T pour les afficher ou les masquer).
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGPMHUD();
	virtual void DrawHUD() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GPM") bool bShowTouch = false;

private:
	struct FButton { FVector2D Pos, Size; const TCHAR* Label; };
	void Layout(float W, float H);
	void ReadTouches(class AGPMCar* Car);
	void DrawButton(const FButton& B, bool bDown);
	void DrawMiniMap(class AGPMCar* Car, FVector2D Origin, float Size);
	FString Chrono(float Seconds) const;
	FString Gap(float Seconds) const;
	void DrawCentered(const FString& Text, FLinearColor Color, float W, float Y, class UFont* Font, float S);
	void DrawLights(int32 On, float W);
	bool Clicked(float X, float Y, float BW, float BH);
	void DrawBtn(const FString& Label, float X, float Y, float BW, float BH, FLinearColor Col);
	bool bTap = false, bTapWas = true;
	FVector2D TapPos = FVector2D::ZeroVector;
	bool bWasDown = true;

	// Petits éléments canvas (GPMHUDMenus.cpp) ; les écrans de gestion sont en Slate (FGPMUI)
	void DrawCine(class AGPMGameMode* GM, float W, float H);
	/** Bouton dessiné ; renvoie vrai s'il vient d'être touché. */
	bool Btn(const FString& Label, float X, float Y, float BW, float BH, FLinearColor Col, bool bOn = true);
	/** Texte sur plusieurs lignes ; renvoie la hauteur occupée. */
	float DrawWrap(const FString& Text, float X, float Y, float MaxW, FLinearColor Color, float S);
	/** Nom affiché d'un pilote (« Toi » → le nom choisi à la création). */
	FString Nm(const FString& Driver) const;

	// Course (GPMHUDRace.cpp) : habillage « diffusion télé » de la maquette Figma (classement des 20 avec écarts et pneus,
	// chrono et secteurs, écarts devant / derrière, compte-tours, pneus et essence, mini-carte, radio), pause
	void DrawRace(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	float DrawTower(class AGPMGameMode* GM, class AGPMCar* Car, float X, float Y);
	void DrawLapPanel(class AGPMGameMode* GM, class AGPMCar* Car, float X, float Y, float PW);
	void DrawGaps(class AGPMGameMode* GM, class AGPMCar* Car, float X, float Y, float PW);
	void DrawTyrePanel(class AGPMGameMode* GM, class AGPMCar* Car, float X, float Y, float PW);
	void DrawCluster(class AGPMCar* Car, float W, float H);
	void DrawPause(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	void SetPaused(bool bPause);
	FString Code3(const FString& Driver) const;
	bool bShowFps = false;
	bool bGapLeader = false;   // classement : intervalle avec la voiture de devant (par défaut) ou écart avec le leader (touche G)
	// course complète : pluie à l'écran, radio du muret (3 boutons), météo
	void DrawRain(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	void DrawRadioPad(class AGPMGameMode* GM, class AGPMCar* Car);
	float DrawRadioMsg(const FString& Msg, float W, float Y);
	void DrawWeatherChip(class AGPMGameMode* GM, float RightX, float Y);
	FButton RadioBtn[3];
	bool bRadioDown[3] = { false, false, false };
	bool bRadioWas[3] = { false, false, false };

	// Chronométrage suivi par le HUD : points de passage (écarts en temps), secteurs de chaque voiture, delta en direct du joueur
	struct FCarTiming
	{
		float Mark[128];
		int32 MarkId[128];
		int32 LastMark = -1;
		int32 Sec = -1, SecLap = -1;
		float SecAt[2] = { 0.f, 0.f };
		float Cur[3] = { 0.f, 0.f, 0.f }, Last[3] = { 0.f, 0.f, 0.f }, PB[3] = { 0.f, 0.f, 0.f };
		int32 CurCol[3] = { 0, 0, 0 }, LastCol[3] = { 0, 0, 0 };   // 1 meilleur de la séance, 2 record perso, 3 plus lent
		FCarTiming() { for (int32 k = 0; k < 128; ++k) { Mark[k] = 0.f; MarkId[k] = -1; } }
	};
	TMap<const class AGPMCar*, FCarTiming> Timing;
	float SesBest[3] = { 0.f, 0.f, 0.f };
	float LastRaceTime = 0.f, FastestLap = 0.f;
	const class AGPMCar* FastestCar = nullptr;
	FString FastestCode;
	TArray<float> CurProf, BestProf;   // temps du tour du joueur à 80 points du tour (delta en direct avec son meilleur tour)
	int32 ProfLap = -1, StintStops = -1, StintLap0 = 1;
	float StintFuel = 0.f;
	FString AlertTxt;
	float AlertT = 0.f;
	void UpdateTiming(class AGPMGameMode* GM, class AGPMCar* Car, float Dt);
	float TimeGap(const class AGPMCar* Ahead, const class AGPMCar* Behind, float PerLap, float LapEst) const;

	// Panneau sombre à coin coupé en haut à droite (charte de la maquette)
	void Panel(float X, float Y, float W, float H, FLinearColor Col, float Cut = 6.f);

	// Charte des écrans dans le HUD : bandeaux penchés, masques (dégradé, halo, disque), police de l'interface
	void Slab(float X, float Y, float W, float H, FLinearColor Col);
	/** Masque de GPMUIStyle::MaskTex (1 trait penché, 2 dégradé, 3 halo, 4 disque, 5 dégradé vers le bas, 6 coin coupé, 7 trapèze). */
	void Paint(int32 Which, float X, float Y, float W, float H, FLinearColor Col);
	/** Texte en police de l'interface (0 titre, 1 gras, 2 étiquette, 3 demi-gras, 4 chiffres Rajdhani) ; Align 0 gauche, 0,5 centré, 1 droite. */
	void Ink(const FString& Text, float X, float Y, int32 Kind, float Size, FLinearColor Col, float Align = 0.f);
	FVector2D InkSize(const FString& Text, int32 Kind, float Size) const;
	/** Texte centré verticalement dans une bande de hauteur BoxH (pixels). */
	void InkV(const FString& Text, float X, float Y, float BoxH, int32 Kind, float Size, FLinearColor Col, float Align = 0.f);
	/** Pneu : anneau de la couleur de la gomme. */
	void TyreIcon(TCHAR Tyre, float Cx, float Cy, float Rad);
	FLinearColor HudAcc = FLinearColor(0.85f, 0.12f, 0.12f, 1.f);   // couleur de l'écurie du joueur

	FButton Left, Right, Gas, Brake, Box;
	bool bLeft = false, bRight = false, bGas = false, bBrake = false, bBoxBtn = false, bBoxWas = false;
	float Scale = 1.f;
	TSharedPtr<class FGPMUI> UI;
};
