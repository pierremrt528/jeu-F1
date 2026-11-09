#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GPMSeason.generated.h"

/** Résultat du joueur sur un Grand Prix (season.results du jeu web). */
USTRUCT()
struct FGPMRaceResult
{
	GENERATED_BODY()
	UPROPERTY() FString Circuit;
	UPROPERTY() int32 Pos = 0;
	UPROPERTY() int32 Pts = 0;
	UPROPERTY() FString Winner;
	UPROPERTY() FString Fastest;
};

/** Sauvegarde de la saison (gpm-season) : calendrier, manche en cours, points des pilotes et des écuries. */
UCLASS()
class GRANDPRIXMANAGER_API UGPMSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() int32 Year = 2027;
	UPROPERTY() int32 Round = 0;
	UPROPERTY() TArray<FString> Calendar;
	UPROPERTY() TMap<FString, int32> DriverPts;
	UPROPERTY() TMap<FString, int32> TeamPts;
	UPROPERTY() TMap<FString, FString> DriverTeam;
	UPROPERTY() TArray<FGPMRaceResult> Results;
};

/** Une ligne de classement. */
struct FGPMStanding { FString Name, Team; int32 Pts = 0; FLinearColor Color = FLinearColor::White; bool bPlayer = false; };

/** Arrivée d'un pilote, pour enregistrer une course. */
struct FGPMFinish { FString Name, Team; float BestLap = 0.f; bool bPlayer = false; };

/**
 * Saison / carrière (season.js) : calendrier des 22 GP, points 25-18-15… + 1 pour le meilleur tour (top 10),
 * classements pilotes et écuries, sauvegarde locale. La gestion (budget, garage, presse…) viendra aux étapes 12-14.
 */
UCLASS()
class GRANDPRIXMANAGER_API UGPMSeasonSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void NewSeason(bool bNextYear = false);
	bool IsOver() const { return S && S->Round >= S->Calendar.Num(); }
	int32 Round() const { return S ? S->Round : 0; }
	int32 NumRounds() const { return S ? S->Calendar.Num() : 0; }
	int32 Year() const { return S ? S->Year : 2027; }
	FString CurrentCircuit() const;
	/** Enregistre l'arrivée (dans l'ordre) et passe à la manche suivante ; renvoie les points du joueur. */
	int32 RecordRace(const TArray<FGPMFinish>& Order);
	TArray<FGPMStanding> Drivers() const;
	TArray<FGPMStanding> Teams() const;
	const TArray<FGPMRaceResult>& Results() const;
	void Save();

	static constexpr int32 Points[10] = { 25, 18, 15, 12, 10, 8, 6, 4, 2, 1 };

private:
	UPROPERTY() TObjectPtr<UGPMSaveGame> S;
};
