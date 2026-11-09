#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GPMGameMode.generated.h"

class AGPMTrack;
class AGPMCar;

/**
 * Mode de jeu de base. Étape 3 : charge un circuit (option ?circuit=<id>, comme le jeu web, ou commande GPM.Circuit <id>)
 * place la lumière et les 20 monoplaces sur la grille (19 IA, js/race.js), aspiration, contacts et classement en direct.
 */
UCLASS()
class GRANDPRIXMANAGER_API AGPMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGPMGameMode();
	virtual void BeginPlay() override;

	/** Remplace le circuit affiché. */
	UFUNCTION(BlueprintCallable, Category = "GPM") void LoadCircuit(const FString& Id);
	virtual void Tick(float DeltaSeconds) override;

	/** Place (1 = en tête) d'une voiture au classement en direct. */
	int32 PositionOf(const AGPMCar* C) const { return Order.IndexOfByKey(C) + 1; }
	int32 NumCars() const { return Order.Num(); }
	/** Secondes avant le départ (> 0 : compte à rebours, feux à l'étape 7). */
	float StartIn = 0.f;

private:
	UPROPERTY() TObjectPtr<AGPMTrack> Track;
	UPROPERTY() TObjectPtr<AGPMCar> Car;               // le joueur
	UPROPERTY() TArray<TObjectPtr<AGPMCar>> Cars;      // les 20 voitures
	TArray<AGPMCar*> Order;
	void SetupWorld();
	void PlaceCar();
	void Aspiration(float Dt);
	void Collide();
};
