#include "LCOGameMode.h"
#include "LastCheckOut.h"
#include "LastCheckOut/Core/LCOGameInstance.h"
#include "LastCheckOut/Story/LCOStorySubsystem.h"

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
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] InitializeCurrentMap: %s"), 
		*GetWorld()->GetMapName()); 
}

void ALCOGameMode::StartGame()
{
	// GameInstance를 LCOGameInstance 타입으로 가져옴
	// Project Settings의 Game Instance Class가 다르면 nullptr이 반환
	ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>();
	if (!LCOGameInstance)
	{
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StartGame: LCOGameInstance not found"));
		return;
	}
	
	// GameInstance에 붙어 있는 StorySubsystem을 가져옴 (엔진이 자동 생성)
	ULCOStorySubsystem* StorySubsystem = LCOGameInstance->GetSubsystem<ULCOStorySubsystem>();
	if (!StorySubsystem)
	{
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StartGame: LCOStorySubsystem not found"));
		return;
	}
	
	// 현재 시작 방식 확인
	// Log Test
	const ELCOGameStartMode StartMode = LCOGameInstance->GetStartMode();
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: StartMode = %s"), 
		*UEnum::GetValueAsString(StartMode));
	
	switch (StartMode)
	{
	case ELCOGameStartMode::NewGame:
		// 새 게임: 스토리 진행 상태 초기화 + Chapter1 시작
		StorySubsystem->StartNewStory();

		// 다음 맵 이동 시 다시 새 게임으로 처리되지 않도록 None으로 되돌림
		LCOGameInstance->ResetStartMode();
		break;

	case ELCOGameStartMode::Continue:
		// 이어하기: SaveManager 구현 전까지 미지원
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StartGame: Continue is not supported yet"));
		break;

	case ELCOGameStartMode::None:
	default:
		// 메뉴 없이 맵을 바로 실행했거나, 맵 이동으로 들어온 경우
		// - 진행 중인 챕터가 없으면 → 새 스토리(Chapter1) 자동 시작 (개발/테스트 편의)
		// - 진행 중인 챕터가 있으면 → 기존 진행 상태 유지 (맵 이동 시)
		// 자동 시작을 끄고 메인 메뉴의 "새 게임"을 거쳐야만 스토리가 시작되도록 하려면?
		// 아래 if / else 블록 전체를 삭제하면 됨
		if (StorySubsystem->GetCurrentChapterID().IsNone())
		{
			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: No chapter in progress -> StartNewStory"));
			StorySubsystem->StartNewStory();
		}
		else
		{
			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: Keep current chapter = %s"),
				*StorySubsystem->GetCurrentChapterID().ToString());
		}
		break;
	}
}

