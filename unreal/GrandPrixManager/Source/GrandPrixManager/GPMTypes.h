#pragma once

#include "CoreMinimal.h"
#include "GPMTypes.generated.h"

/** Pilote d'une écurie (data.json → TEAMS[].drivers). */
USTRUCT(BlueprintType)
struct FGPMDriver
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Skill = 0.95f;   // fraction de la vitesse limite qu'il ose prendre
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPlayer = false;
};

/** Écurie (data.json → TEAMS). Couleurs en 0xRRGGBB comme dans le jeu web. */
USTRUCT(BlueprintType)
struct FGPMTeam
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::Red;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Accent = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Perf = 0.975f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGPMDriver> Drivers;
};

/** Gomme (strategy.json → TYRES). */
USTRUCT(BlueprintType)
struct FGPMTyre
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Grip = 1.f;          // adhérence neuve
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WearPerLap = 0.085f; // usure par tour (wpl)
};

/**
 * Circuit pré-calculé par le jeu web (tracks/<id>.json) : ligne médiane tous les 2 m.
 * Repère du jeu web : x, z au sol, y = altitude, en mètres. Conversion Unreal (cm, Z vers le haut) dans ToWorld().
 */
USTRUCT(BlueprintType)
struct FGPMTrackData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) float Length = 0.f;   // m
	UPROPERTY(BlueprintReadOnly) float Width = 14.f;   // m
	UPROPERTY(BlueprintReadOnly) TArray<FVector> Points;      // déjà en coordonnées Unreal (cm)
	UPROPERTY(BlueprintReadOnly) TArray<float> WallLeft;      // distance du mur gauche à l'axe (m)
	UPROPERTY(BlueprintReadOnly) TArray<float> WallRight;     // distance du mur droit (m)
	UPROPERTY(BlueprintReadOnly) TArray<float> Curvature;     // 1/m
	UPROPERTY(BlueprintReadOnly) TArray<float> AISpeed;       // vitesse de référence de l'IA (m/s)

	// Repère du jeu web (m), pour la physique portée de js/car.js : x, z au sol, y altitude ; tangente (TX, TZ), cap H = atan2(TX, TZ)
	TArray<float> WX, WY, WZ, TX, TZ, H;
	TArray<float> RL;
	// Voie des stands (pitLane de track.js) : indices signés par rapport à la ligne (entrée e0→e1, sortie x0→x1), côté droit
	float PitHalf = 7.f, PitLat = 22.f, PitLoss = 22.f;
	int32 PitE0 = -190, PitE1 = -140, PitX0 = 70, PitX1 = 110;
	int32 PitK(int32 i) const { const int32 N = WX.Num(); return i > N / 2 ? i - N : i; }
	float PitLatAt(float K) const
	{
		auto Sm = [](float t) { t = FMath::Clamp(t, 0.f, 1.f); return t * t * (3 - 2 * t); };
		if (K < PitE1) return PitHalf - 1 + (PitLat - PitHalf + 1) * Sm((K - PitE0) / float(PitE1 - PitE0));
		if (K > PitX0) return PitLat - (PitLat - PitHalf + 1) * Sm((K - PitX0) / float(PitX1 - PitX0));
		return PitLat;
	}
	static float PitBox(int32 TeamIdx) { return -125.f + TeamIdx * 13.f; }   // trajectoire de course : décalage latéral (m) vers l'intérieur des virages (track.js)

	/** Point le plus proche autour de Idx (comme locate() de track.js). */
	int32 Locate(float X, float Z, int32 Idx) const
	{
		const int32 N = WX.Num();
		int32 Best = Idx; float Bd = TNumericLimits<float>::Max();
		for (int32 k = -12; k <= 24; ++k)
		{
			const int32 i = ((Idx + k) % N + N) % N;
			const float D = FMath::Square(WX[i] - X) + FMath::Square(WZ[i] - Z);
			if (D < Bd) { Bd = D; Best = i; }
		}
		return Best;
	}

	static FVector ToWorld(double X, double Alt, double Z) { return FVector(X * 100.0, Z * 100.0, Alt * 100.0); }
	int32 Num() const { return Points.Num(); }
};

/** Modèle 3D de monoplace exporté du jeu web (car.json, unreal/tools/export-car.mjs), déjà en repère Unreal (cm). */
struct FGPMCarModel
{
	TArray<FVector> P, N;              // sommets non indexés (3 par triangle), normales
	TArray<int32> K;                   // couleur : 0 = principale, 1 = accent, 2 = casque, sinon Fixed[K]
	TArray<FLinearColor> Fixed;        // couleurs fixes (carbone, pneus…), linéaires
};

