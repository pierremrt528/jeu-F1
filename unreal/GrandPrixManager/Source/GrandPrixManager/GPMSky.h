#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPMSky.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FGPMTrackData;

namespace GPMSkyCfg
{
	/** Au-delà de cette distance, pas de brouillard : ciel, nuages et montagnes ont déjà des couleurs « brumeuses ». */
	constexpr float FogCutoff = 1200000.f;   // 12 km
}

/**
 * Ciel du circuit comme dans le jeu web (scenery.js) : dôme en dégradé (horizon → zénith du thème) avec le halo
 * et le disque du soleil, couche de nuages photographiques qui dérive lentement, et deux chaînes de montagnes
 * à l'horizon (crêtes bruitées, du pied au sommet enneigé selon le thème). Tout est sans éclairage, au-delà
 * de la coupure du brouillard, et ne coûte que quelques appels de rendu.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMSky : public AActor
{
	GENERATED_BODY()

public:
	AGPMSky();
	/** Construit le ciel du thème autour du circuit. */
	void Build(const FGPMTrackData* InTrack);
	/** Grisaille de la pluie : 0 beau temps … 1 ciel couvert (dégradé gris, nuages épais, soleil caché). */
	void SetGloom(float G);
	/** Couleur grise du ciel couvert (le brouillard prend la même sous la pluie, pour ne pas voir de raccord). */
	static FLinearColor GloomGrey() { return FLinearColor(0.33f, 0.35f, 0.38f); }

private:
	void BuildDome(const struct FGPMTheme& Th, const FVector& SunDir);
	void BuildClouds(float Yaw);
	void BuildRidges(const FGPMTrackData* InTrack, float SizeM);

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Dome;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> CloudLayer;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Ridges;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SkyMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SunMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CloudMID;
	UPROPERTY() TObjectPtr<UMaterialInterface> VertexMat;
};
