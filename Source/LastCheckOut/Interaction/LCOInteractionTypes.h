#pragma once

#include "CoreMinimal.h"
#include "Story/LCOStoryTypes.h"
#include "LCOInteractionTypes.generated.h"

class UGameInstance;

// 상호작용 결과로 스토리 진행에 반영할 내용 (퍼즐 해결, 통화 완료 등에서 공통 사용)
// 세이브에 스토리 상태로 저장되므로, 이어하기 시 다시 적용하지 않는다
USTRUCT(BlueprintType)
struct LASTCHECKOUT_API FLCOStoryResult
{
	GENERATED_BODY()

	// 완료할 목표 (현재 목표와 같을 때만 완료됨). 비워두면 무시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FName ObjectiveToComplete;

	// 획득할 증언 / 키워드 플래그 (해금 조건을 만족해야 획득)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TArray<ELCOStoryFlag> FlagsToUnlock;

	bool IsEmpty() const { return ObjectiveToComplete.IsNone() && FlagsToUnlock.IsEmpty(); }

	// LCOStorySubsystem에 결과 적용
	void Apply(const UGameInstance* GameInstance) const;
};
