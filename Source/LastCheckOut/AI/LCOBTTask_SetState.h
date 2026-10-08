#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "AI/LCOCaretakerTypes.h"
#include "LCOBTTask_SetState.generated.h"

// 관리인 컨트롤러의 상태를 지정한 값으로 바꾼다. (Blackboard State 키도 함께 갱신됨)
UCLASS()
class LASTCHECKOUT_API ULCOBTTask_SetState : public UBTTaskNode
{
	GENERATED_BODY()

public:
	ULCOBTTask_SetState();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "State")
	ELCOCaretakerState NewState = ELCOCaretakerState::Patrol;
};
