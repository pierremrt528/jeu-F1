#include "GPMGameMode.h"
#include "GrandPrixManager.h"
#include "GPMDataSubsystem.h"
#include "Engine/GameInstance.h"

AGPMGameMode::AGPMGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGPMGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogGPM, Log, TEXT("Grand Prix Manager : démarrage"));

	// Vérification de l'étape 2 : les données du jeu web sont lisibles.
	if (UGPMDataSubsystem* Data = GetGameInstance()->GetSubsystem<UGPMDataSubsystem>())
	{
		for (const FString& Id : Data->Calendar)
			if (const FGPMTrackData* T = Data->GetTrack(Id))
				UE_LOG(LogGPM, Log, TEXT("  %s : %.2f km"), *T->Name, T->Length / 1000.f);
	}
}
