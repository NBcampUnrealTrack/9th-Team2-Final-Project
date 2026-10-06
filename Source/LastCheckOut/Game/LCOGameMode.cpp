#include "LCOGameMode.h"

ALCOGameMode::ALCOGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALCOGameMode::BeginPlay()
{
	Super::BeginPlay();

	InitializeCurrentMap();
	StartGame();
}

void ALCOGameMode::InitializeCurrentMap()
{
	UE_LOG(LogTemp, Log, TEXT("[LCOGameMode] InitializeCurrentMap: %s"), *GetWorld()->GetMapName());
}

void ALCOGameMode::StartGame()
{
	UE_LOG(LogTemp, Log, TEXT("[LCOGameMode] StartGame"));
}

