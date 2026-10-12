#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LCOBTTask_FindSearchPoint.generated.h"

// 마지막으로 본 위치(LastKnownLocation) 주변의 이동 가능한 랜덤 지점을 골라 Blackboard의 SearchLocation에 기록한다.
UCLASS()
class LASTCHECKOUT_API ULCOBTTask_FindSearchPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	ULCOBTTask_FindSearchPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	// 수색 반경(cm). 의심 상태는 작게, 경계 상태는 크게 지정한다.
	UPROPERTY(EditAnywhere, Category = "Search")
	float SearchRadius = 600.f;
};
