#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "GPMTypes.h"
#include "GPMDataSubsystem.generated.h"

/**
 * Charge les données exportées du jeu web (fichiers JSON de Content/Data, voir unreal/tools/export-data.mjs).
 * Accès typé aux écuries, gommes et circuits ; accès brut (JSON) à tout le reste,
 * en attendant le portage de chaque système (garage, événements, presse…).
 */
UCLASS()
class GRANDPRIXMANAGER_API UGPMDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintReadOnly, Category = "GPM") TArray<FGPMTeam> Teams;
	UPROPERTY(BlueprintReadOnly, Category = "GPM") TArray<FGPMTyre> Tyres;
	UPROPERTY(BlueprintReadOnly, Category = "GPM") TArray<FString> Calendar;   // ids des 22 circuits dans l'ordre de la saison

	/** Circuit pré-calculé (chargé à la demande puis gardé en mémoire). */
	const FGPMTrackData* GetTrack(const FString& Id);

	/** Ids de tous les circuits disponibles. */
	UFUNCTION(BlueprintCallable, Category = "GPM") TArray<FString> GetCircuitIds() const;

	/** Section brute : File = nom du fichier sans .json (ex. "garage"), Key = tableau exporté (ex. "TREES"). */
	TSharedPtr<FJsonValue> GetRaw(const FString& File, const FString& Key);

	static FString DataDir();

private:
	TSharedPtr<FJsonObject> LoadFile(const FString& RelPath);
	TMap<FString, TSharedPtr<FJsonObject>> Files;
	TMap<FString, TSharedPtr<FGPMTrackData>> TrackCache;   // pointeurs stables : la voiture et le circuit les gardent
	TArray<FString> CircuitIds;
};
