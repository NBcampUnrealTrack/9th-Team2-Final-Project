#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/LCOWorldStateTypes.h"
#include "LCOWorldStateSubsystem.generated.h"

// 맵별 액터 상태(문 / 서랍장 / 트리거 등) 보관
// 맵 이동 시 LCOGameMode가 기록·복원, 세이브 시 LCOSaveManager가 파일에 저장·불러오기
UCLASS()
class LASTCHECKOUT_API ULCOWorldStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 현재 맵의 저장 대상 액터(ILCOSaveableActor) 상태를 기록 (맵 떠날 때 / 세이브할 때)
	void CaptureWorldState(UWorld* World);

	// 현재 맵에 기록된 상태를 액터에 적용 (맵 시작 후, 모든 BeginPlay 이후)
	void ApplyWorldState(UWorld* World);

	// 새 게임: 모든 맵 기록 삭제
	void ClearAll();

	// 세이브 파일 저장 / 불러오기용
	const TMap<FName, FLCOMapSaveState>& GetAllMapStates() const { return MapStates; }
	void RestoreAllMapStates(const TMap<FName, FLCOMapSaveState>& InMapStates);

private:
	// 맵 구분 키 (예: /Game/Maps/Test_Main, 에디터 Play 접두어 제거)
	static FName GetMapKey(const UWorld* World);

	// 맵 경로 → 그 맵의 액터별 상태
	UPROPERTY()
	TMap<FName, FLCOMapSaveState> MapStates;
};
