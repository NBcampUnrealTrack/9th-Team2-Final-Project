#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOPuzzleSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnLCOSolvedPuzzlesRestored);

// 해결한 퍼즐 ID 목록 관리 (GameInstance 단위라 맵을 이동해도 유지)
// - 퍼즐이 해결되면 MarkPuzzleSolved로 기록, 퍼즐은 BeginPlay에서 IsPuzzleSolved로 해결 상태를 복원
// - 저장: GetSolvedPuzzleIDs / 불러오기: RestoreSolvedPuzzles / 새 게임: ClearAll (LCOSaveManager, LCOGameMode에서 호출)
UCLASS()
class LASTCHECKOUT_API ULCOPuzzleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 퍼즐 해결 기록 (ALCOPuzzleBase::SolvePuzzle에서 호출)
	void MarkPuzzleSolved(FName PuzzleID);

	UFUNCTION(BlueprintPure, Category = "LCO|Puzzle")
	bool IsPuzzleSolved(FName PuzzleID) const { return !PuzzleID.IsNone() && SolvedPuzzleIDs.Contains(PuzzleID); }

	// 해결한 퍼즐 ID 목록 (세이브 저장용)
	UFUNCTION(BlueprintPure, Category = "LCO|Puzzle")
	TArray<FName> GetSolvedPuzzleIDs() const { return SolvedPuzzleIDs.Array(); }

	// 세이브에서 읽은 목록으로 교체 (세이브 불러오기용). 이미 배치된 퍼즐들에 복원 알림을 보냄
	UFUNCTION(BlueprintCallable, Category = "LCO|Puzzle")
	void RestoreSolvedPuzzles(const TArray<FName>& PuzzleIDs);

	// 전체 초기화 (새 게임 시작 시)
	UFUNCTION(BlueprintCallable, Category = "LCO|Puzzle")
	void ClearAll();

	// RestoreSolvedPuzzles 이후 호출. 레벨의 퍼즐이 해결 상태를 다시 확인할 때 사용
	// (세이브 적용이 퍼즐 BeginPlay보다 늦게 실행될 수 있으므로)
	FOnLCOSolvedPuzzlesRestored OnSolvedPuzzlesRestored;

private:
	UPROPERTY()
	TSet<FName> SolvedPuzzleIDs;
};
