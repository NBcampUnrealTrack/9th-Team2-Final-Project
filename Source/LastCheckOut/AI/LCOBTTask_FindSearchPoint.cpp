#include "AI/LCOBTTask_FindSearchPoint.h"

#include "AIController.h"
#include "AI/LCOCaretakerTypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

ULCOBTTask_FindSearchPoint::ULCOBTTask_FindSearchPoint()
{
	NodeName = TEXT("Find Search Point");
}

EBTNodeResult::Type ULCOBTTask_FindSearchPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	const APawn* Pawn = AIController ? AIController->GetPawn() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Pawn || !BB)
	{
		return EBTNodeResult::Failed;
	}

	// 마지막 본 위치가 없으면 현재 위치를 기준으로 한다.
	FVector Origin = BB->GetValueAsVector(LCOBlackboardKeys::LastKnownLocation);
	if (Origin.IsNearlyZero())
	{
		Origin = Pawn->GetActorLocation();
	}

	const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Pawn->GetWorld());
	FNavLocation SearchPoint;
	if (!NavSystem || !NavSystem->GetRandomReachablePointInRadius(Origin, SearchRadius, SearchPoint))
	{
		return EBTNodeResult::Failed;
	}

	BB->SetValueAsVector(LCOBlackboardKeys::SearchLocation, SearchPoint.Location);
	return EBTNodeResult::Succeeded;
}

FString ULCOBTTask_FindSearchPoint::GetStaticDescription() const
{
	return FString::Printf(TEXT("Radius: %.0f"), SearchRadius);
}
