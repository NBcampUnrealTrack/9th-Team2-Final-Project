#include "AI/LCOBTTask_NextPatrolPoint.h"

#include "AIController.h"
#include "AI/LCOCaretakerTypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NPC/LCOCaretaker.h"

ULCOBTTask_NextPatrolPoint::ULCOBTTask_NextPatrolPoint()
{
	NodeName = TEXT("Next Patrol Point");
}

EBTNodeResult::Type ULCOBTTask_NextPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	ALCOCaretaker* Caretaker = AIController ? Cast<ALCOCaretaker>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Caretaker || !BB)
	{
		return EBTNodeResult::Failed;
	}

	FVector Location;
	if (!Caretaker->GetNextPatrolLocation(Location))
	{
		return EBTNodeResult::Failed;
	}

	BB->SetValueAsVector(LCOBlackboardKeys::PatrolLocation, Location);
	return EBTNodeResult::Succeeded;
}
