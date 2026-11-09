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

	// Course (GPMHUDRace.cpp) : tour des positions façon télé, compte-tours, pause
	void DrawRace(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	void DrawTower(class AGPMGameMode* GM, class AGPMCar* Car, float X, float Y);
	void DrawCluster(class AGPMCar* Car, float W, float H);
	void DrawPause(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	void SetPaused(bool bPause);
	FString Code3(const FString& Driver) const;
	bool bShowFps = false;
	// course complète : pluie à l'écran, radio du muret (3 boutons), secteurs des qualifications, météo
	void DrawRain(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	void DrawRadioPad(class AGPMGameMode* GM, class AGPMCar* Car);
	void DrawRadioMsg(const FString& Msg, float W, float Y);
	void DrawQualiPanel(class AGPMGameMode* GM, class AGPMCar* Car, float X, float Y);
	void DrawWeatherChip(class AGPMGameMode* GM, float Right, float Y);
	void DrawDamage(class AGPMCar* Car, float X, float Y, float H);
	FButton RadioBtn[3];
	bool bRadioDown[3] = { false, false, false };
	bool bRadioWas[3] = { false, false, false };

	FButton Left, Right, Gas, Brake, Box;
	bool bLeft = false, bRight = false, bGas = false, bBrake = false, bBoxBtn = false, bBoxWas = false;
	float Scale = 1.f;
	TSharedPtr<class FGPMUI> UI;
};
