#include "AI/LCOBTTask_SetState.h"

#include "AI/LCOCaretakerAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

ULCOBTTask_SetState::ULCOBTTask_SetState()
{
	NodeName = TEXT("Set State");
}

EBTNodeResult::Type ULCOBTTask_SetState::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(OwnerComp.GetAIOwner());
	if (!Controller)
	{
		return EBTNodeResult::Failed;
	}

	Controller->SetState(NewState);
	return EBTNodeResult::Succeeded;
}

FString ULCOBTTask_SetState::GetStaticDescription() const
{
	return FString::Printf(TEXT("State: %s"), *UEnum::GetDisplayValueAsText(NewState).ToString());
}
