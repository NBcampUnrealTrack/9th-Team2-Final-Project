#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LCOBTTask_NextPatrolPoint.generated.h"

/* 관리인의 다음 순찰 지점 위치를 Blackboard의 PatrolLocation에 기록한다. */
UCLASS()
class LASTCHECKOUT_API ULCOBTTask_NextPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	ULCOBTTask_NextPatrolPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
