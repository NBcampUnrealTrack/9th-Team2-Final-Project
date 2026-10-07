#include "AI/LCOBTTask_SetSpeed.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "NPC/LCONPCBase.h"

ULCOBTTask_SetSpeed::ULCOBTTask_SetSpeed()
{
	NodeName = TEXT("Set Speed");
}

EBTNodeResult::Type ULCOBTTask_SetSpeed::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	ALCONPCBase* NPC = AIController ? Cast<ALCONPCBase>(AIController->GetPawn()) : nullptr;
	if (!NPC)
	{
		return EBTNodeResult::Failed;
	}

	if (SpeedMode == ELCOSpeedMode::Run)
	{
		NPC->SetRunSpeed();
	}
	else
	{
		NPC->SetWalkSpeed();
	}

	return EBTNodeResult::Succeeded;
}

FString ULCOBTTask_SetSpeed::GetStaticDescription() const
{
	return FString::Printf(TEXT("Speed: %s"), SpeedMode == ELCOSpeedMode::Run ? TEXT("Run") : TEXT("Walk"));
}
