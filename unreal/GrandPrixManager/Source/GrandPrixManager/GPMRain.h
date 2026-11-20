#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPMRain.generated.h"

class UInstancedStaticMeshComponent;
class AGPMCar;

/**
 * Pluie en 3D : gerbes d'eau soulevées par les roues (plus fortes avec la vitesse et l'eau sur la piste)
 * et gouttes qui tombent autour de la caméra. Deux maillages instanciés seulement (deux appels de dessin).
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMRain : public AActor
{
	GENERATED_BODY()

public:
	AGPMRain();
	/** Chaque image : Wet = eau sur la piste (0..1), Fall = pluie qui tombe (0..1). */
	void UpdateRain(float Dt, const FVector& Cam, const FRotator& CamRot, const TArray<AGPMCar*>& Cars, float Wet, float Fall, USceneComponent* OnboardCam, float Speed);
	/** Tout disparaît (nouvelle séance, menus). */
	void Clear();

private:
	struct FPuff { FVector P = FVector::ZeroVector, Vel = FVector::ZeroVector; float Age = 0.f, Life = 0.f, Size = 1.f, Alpha = 0.f; };
	TArray<FPuff> Puffs;
	int32 NextPuff = 0;
	TArray<FVector> Drops;            // position des gouttes autour de la caméra (cm, repère du monde)
	TMap<const AGPMCar*, float> Emit; // gerbes en attente d'émission par voiture
	bool bShown = false;
	FVector LastCam = FVector::ZeroVector;

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Spray;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rain;
	/** Gouttes sur l'objectif en T-cam et en cockpit : elles se posent, puis glissent avec le vent de la vitesse. */
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lens;
	struct FLensDrop { FVector2D P = FVector2D::ZeroVector; float Size = 0.f, Age = 0.f, Life = 0.f; };
	TArray<FLensDrop> LensDrops;
	float LensAcc = 0.f;
	void Show(bool bOn);
};
