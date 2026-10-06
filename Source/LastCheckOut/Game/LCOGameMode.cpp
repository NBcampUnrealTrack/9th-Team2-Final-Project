#include "LCOGameMode.h"
#include "LastCheckOut/Core/LCOGameInstance.h"

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
	// Log Test
	UE_LOG(LogTemp, Log, TEXT("[LCOGameMode] InitializeCurrentMap: %s"), *GetWorld()->GetMapName()); 
}

void ALCOGameMode::StartGame()
{
	// GameInstance를 LCOGameInstance 타입으로 가져옴
	// Project Settings의 Game Instance Class가 다르면 nullptr이 반환
	ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>();
	if (!LCOGameInstance)
	{
		// Log Test
		UE_LOG(LogTemp, Warning, TEXT("[LCOGameMode] StartGame: LCOGameInstance not found"));
		return;
	}

	// 현재 시작 방식을 읽어서 출력 (지금은 None이 정상)
	// Log Test
	const ELCOGameStartMode StartMode = LCOGameInstance->GetStartMode();
	UE_LOG(LogTemp, Log, TEXT("[LCOGameMode] StartGame: StartMode = %s"), *UEnum::GetValueAsString(StartMode));
}

