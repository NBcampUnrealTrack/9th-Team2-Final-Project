#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Notebook/LCONotebookTypes.h"
#include "Core/LCOGameInstance.h"
#include "Save/LCOWorldStateTypes.h"
#include "LCOSaveGame.generated.h"

// 세이브 파일에 들어가는 데이터 (UPROPERTY로 선언한 변수만 파일에 저장됨)
// 저장·불러오기 → LCOSaveManager 담당
UCLASS()
class LASTCHECKOUT_API ULCOSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 세이브 파일 형식 버전 (저장 항목이 바뀌면 올림)
	UPROPERTY()
	int32 SaveVersion = 1;

	// 저장한 시각
	UPROPERTY()
	FDateTime SavedTime;

	// 저장한 맵 경로, 이어하기 시 이 맵으로 이동
	// (예시: /Game/Maps/TestMap).
	UPROPERTY()
	FString MapName;

	// [Story]
	UPROPERTY()
	FName ChapterID = NAME_None;

	UPROPERTY()
	FName ObjectiveID = NAME_None;

	UPROPERTY()
	TArray<FName> CompletedObjectives;

	// 획득한 증언 / 키워드 플래그 이름 (예: "T02", "K01")
	UPROPERTY()
	TArray<FName> StoryFlags;
	
	// [수첩] (목표 / 단서 / 증언 / 사진)
	UPROPERTY()
	TArray<FLCONotebookEntry> NotebookEntries;
	
	// [Player] 인벤토리 / 퀵슬롯 / 시점 (맵 이동용 보관 구조체 재사용)
	UPROPERTY()
	FLCOPlayerCarryData PlayerData;

	// 저장 당시 플레이어가 있었는지 (메뉴 등 플레이어 없는 곳에서 저장하면 false)
	UPROPERTY()
	bool bHasPlayerData = false;

	// [맵 상태] 맵별 문 / 서랍장 / 트리거 상태 (맵 경로 → 액터별 상태)
	UPROPERTY()
	TMap<FName, FLCOMapSaveState> MapStates;

	// [퍼즐] 해결한 퍼즐 ID 목록 (LCOPuzzleSubsystem)
	UPROPERTY()
	TArray<FName> SolvedPuzzleIDs;
};
