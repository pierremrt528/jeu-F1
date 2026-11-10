#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPMScenery.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
struct FGPMTrackData;

/**
 * Décor du circuit (scenery.js), sans asset : relief du terrain qui épouse l'altitude de la piste (prolongé jusqu'à
 * l'horizon), arbres (sapins à étages, feuillus bosselés, palmiers à palmes), immeubles, tribunes, stands, mer et plage.
 * Ciel et montagnes : AGPMSky.
 * Tout est instancié (un appel de rendu par type d'objet), sans ombres, pour tenir 60 i/s sur mobile.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMScenery : public AActor
{
	GENERATED_BODY()

public:
	AGPMScenery();
	void Build(const FGPMTrackData* InTrack);

	/** Altitude du terrain (cm) au point (X, Y) du monde Unreal. */
	float GroundZ(float X, float Y) const;

private:
	void BuildTerrain();
	void BuildTrees();
	void BuildCity();
	void BuildStands();
	/**
	 * Calque de formes instanciées. Avec les matériaux de Tools/gpm_assets.py, chaque instance a sa propre teinte
	 * (entre Color et Color2, plus ou moins claire) ; Windows : bandeaux vitrés des immeubles.
	 */
	UHierarchicalInstancedStaticMeshComponent* AddLayer(const TCHAR* Shape, FLinearColor Color, FLinearColor Color2 = FLinearColor::Transparent, float Rough = 0.9f, bool bWindows = false);
	/** Teinte de chaque instance (données d'instance), une fois tout le décor posé. */
	void TintInstances();
	/** Distance (m) au point de piste le plus proche, et son altitude (m). */
	float NearTrack(float X, float Y, float* OutAlt = nullptr, int32* OutIdx = nullptr) const;

	const FGPMTrackData* Track = nullptr;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Instances;
	/** Arbres fusionnés par carrés de 600 m (un appel de rendu par carré, coupé hors du champ). */
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Foliage;
	UPROPERTY() TObjectPtr<UMaterialInterface> ShapeMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> VertexMat;

	// grille du terrain (cm) et grille de recherche des points de piste (cellules de 50 m)
	FVector2D Origin = FVector2D::ZeroVector;
	float Cell = 2000.f;
	int32 GX = 0, GY = 0;
	TArray<float> Heights;
	TMap<int64, TArray<int32>> Hash;
	float BaseZ = 0.f;
	// mer au sud du circuit (comme le jeu web) : rivage à SeaY (cm), rien de construit à moins de 45 m
	bool bSeaSide = false;
	float SeaY = 0.f;
	bool InSea(float Y) const { return bSeaSide && Y < SeaY + 4500.f; }
};
