#include "GPMGameMode.h"
#include "GrandPrixManager.h"

AGPMGameMode::AGPMGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGPMGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogGPM, Log, TEXT("Grand Prix Manager : démarrage"));
}
