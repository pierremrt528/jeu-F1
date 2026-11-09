#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPMTypes.h"
#include "GPMTrack.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Circuit généré à partir des données du jeu web (portage de js/scenery.js, partie piste).
 * Sections : 0 asphalte, 1 vibreurs, 2 herbe, 3 murs, 4 sol, 5 ligne de départ.
 * Collision : asphalte, herbe, sol et murs (collision complexe utilisée comme simple, pour Chaos Vehicle).
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMTrack : public AActor
{
	GENERATED_BODY()

public:
	AGPMTrack();

	/** Construit le circuit (id du jeu web, ex. "valmont"). Renvoie false si le circuit est introuvable. */
	UFUNCTION(BlueprintCallable, Category = "GPM") bool BuildCircuit(const FString& CircuitId);

	/** Point de départ (ligne) et direction de course, pour placer la caméra ou la grille. */
	UFUNCTION(BlueprintCallable, Category = "GPM") FTransform GetStartTransform() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GPM") FString CircuitId;

	/** Matériaux (étape 9). Sans matériau : couleurs par sommet. */
	UPROPERTY(EditAnywhere, Category = "GPM|Matériaux") TObjectPtr<UMaterialInterface> AsphaltMaterial;
	UPROPERTY(EditAnywhere, Category = "GPM|Matériaux") TObjectPtr<UMaterialInterface> KerbMaterial;
	UPROPERTY(EditAnywhere, Category = "GPM|Matériaux") TObjectPtr<UMaterialInterface> GrassMaterial;
	UPROPERTY(EditAnywhere, Category = "GPM|Matériaux") TObjectPtr<UMaterialInterface> WallMaterial;

	/** Piste mouillée (0 sèche … 1 trempée) : asphalte plus sombre et brillant (matériaux de Tools/gpm_assets.py). */
	void SetWet(float Wet);

	/** Seuil de courbure au-delà duquel on pose des vibreurs (même valeur que le jeu web). */
	static constexpr float KerbCurvature = 1.f / 220.f;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Mesh;
	const FGPMTrackData* Track = nullptr;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RoadMID;

	FVector Right(int32 i) const;   // vecteur latéral (côté WR) au point i, horizontal
};
