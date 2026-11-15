#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPMDebris.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class AGPMCar;

/**
 * Débris des accrochages (debris.js du jeu web) : éclats de carbone instanciés (un seul appel de rendu)
 * et ailerons arrachés qui rebondissent sur la piste avant de disparaître.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMDebris : public AActor
{
	GENERATED_BODY()

public:
	AGPMDebris();
	virtual void Tick(float DeltaSeconds) override;

	/** Éclats de carbone autour de l'aileron touché. */
	void Burst(const AGPMCar* From, int32 Count, bool bRear);
	/** L'aileron arraché part rebondir sur la piste. */
	void LooseWing(const AGPMCar* From, bool bRear);
	/** Tout disparaît (nouvelle séance). */
	void Clear();

protected:
	virtual void BeginPlay() override;

private:
	struct FBit
	{
		FVector P = FVector::ZeroVector, Vel = FVector::ZeroVector, Size = FVector::OneVector;
		FRotator Rot = FRotator::ZeroRotator, Spin = FRotator::ZeroRotator;
		float Life = 0.f, Ground = 0.f;
	};
	TArray<FBit> Bits;
	TArray<FBit> Pieces;
	int32 NextBit = 0, NextPiece = 0;
	bool bBitsShown = false;

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Shards;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> PieceMeshes;
	UPROPERTY() TObjectPtr<UMaterialInterface> ShapeMat;
};
