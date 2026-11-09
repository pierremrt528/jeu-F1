#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
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
	void DrawQuali(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	void DrawResults(class AGPMGameMode* GM, class AGPMCar* Car, float W, float H);
	bool bWasDown = true;

	FButton Left, Right, Gas, Brake, Box;
	bool bLeft = false, bRight = false, bGas = false, bBrake = false, bBoxBtn = false, bBoxWas = false;
	float Scale = 1.f;
};
