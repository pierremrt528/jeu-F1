#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPMScenery.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
struct FGPMTrackData;

/**
 * Décor du circuit (scenery.js), sans asset : relief du terrain qui épouse l'altitude de la piste,
 * arbres (sapins, feuillus, palmiers), immeubles, tribunes, stands, montagnes à l'horizon, mer.
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
	void BuildMountains();
	UHierarchicalInstancedStaticMeshComponent* AddLayer(const TCHAR* Shape, FLinearColor Color);
	/** Distance (m) au point de piste le plus proche, et son altitude (m). */
	float NearTrack(float X, float Y, float* OutAlt = nullptr, int32* OutIdx = nullptr) const;

	const FGPMTrackData* Track = nullptr;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
	UPROPERTY() TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Instances;
	UPROPERTY() TObjectPtr<UMaterialInterface> ShapeMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> VertexMat;

	// grille du terrain (cm) et grille de recherche des points de piste (cellules de 50 m)
	FVector2D Origin = FVector2D::ZeroVector;
	float Cell = 2000.f;
	int32 GX = 0, GY = 0;
	TArray<float> Heights;
	TMap<int64, TArray<int32>> Hash;
	float BaseZ = 0.f;
};
