#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GPMFactory.generated.h"

/** Une pièce d'un arbre de développement (TREES de garage.js). */
struct FGPMPart
{
	FString Id, Name, Desc, Tree;
	float Cost = 20.f;
	int32 Row = 1, Col = 2;                 // position dans l'arbre (rangée, colonne sur 3)
	TArray<FString> Req, ReqAny;            // toutes / au moins une
	TMap<FString, float> Fx;                // vtop, acc, mu, brake, kd, steer, fuel, rel…
};

struct FGPMTree { FString Id, Name; FLinearColor Color; TArray<FGPMPart> Parts; };

/** Multiplicateurs de la voiture du joueur (carUpgrades de garage.js). */
struct FGPMUpgrades { float VTop = 1, Acc = 1, Mu = 1, Brake = 1, Kd = 1, Steer = 1, Fuel = 1; };

/**
 * Usine (garage.js) : 5 pôles (moteur, aéro, châssis, fiabilité, stands), pièces payées en M€ qui demandent
 * 20 à 56 jours, avec un risque de retard puis d'échec ; une recherche par pôle, 2 ateliers en même temps.
 * L'état est gardé dans la sauvegarde de la saison.
 */
UCLASS()
class GRANDPRIXMANAGER_API UGPMFactorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const TArray<FGPMTree>& Trees() const { return TreeList; }
	const FGPMPart* Part(const FString& Id) const;
	static float Price(const FGPMPart& P) { return FMath::RoundToFloat(P.Cost / 5.f * 10.f) / 10.f; }
	static int32 Delay(const FGPMPart& P) { return P.Cost <= 20 ? 20 : P.Cost <= 55 ? 36 : 56; }
	static int32 FailPct(const FGPMPart& P) { return P.Cost <= 20 ? 6 : P.Cost <= 55 ? 10 : 16; }
	int32 Workshops() const { return 2; }
	int32 Running() const;

	enum class EState : uint8 { Owned, Running, Available, TooExpensive, Busy, Locked };
	EState StateOf(const FGPMPart& P) const;
	/** Lance la recherche (paie le prix). */
	bool Start(const FString& Id);
	/** Fait avancer les recherches de N jours ; les messages vont dans les actualités de la saison. */
	void Tick(int32 Days);
	FGPMUpgrades Upgrades() const;
	float Budget() const;
	void AddMoney(float M);

private:
	TArray<FGPMTree> TreeList;
	class UGPMSaveGame* Save() const;
	void Persist() const;
};
