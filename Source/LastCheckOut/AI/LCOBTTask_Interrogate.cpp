#include "AI/LCOBTTask_Interrogate.h"

#include "AI/LCOCaretakerAIController.h"
#include "AI/LCOCaretakerTypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NPC/LCONPCBase.h"
#include "TimerManager.h"

ULCOBTTask_Interrogate::ULCOBTTask_Interrogate()
{
	NodeName = TEXT("Interrogate");

	// 대화가 끝날 때까지 대기하므로 노드마다 인스턴스를 만든다.
	bCreateNodeInstance = true;
}

EBTNodeResult::Type ULCOBTTask_Interrogate::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(OwnerComp.GetAIOwner());
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	ALCONPCBase* NPC = Controller ? Cast<ALCONPCBase>(Controller->GetPawn()) : nullptr;
	APawn* Player = BB ? Cast<APawn>(BB->GetValueAsObject(LCOBlackboardKeys::TargetActor)) : nullptr;
	if (!Controller || !NPC || !Player)
	{
		return EBTNodeResult::Failed;
	}

	if (!Controller->BeginInterrogation(Player))
	{
		return EBTNodeResult::Failed;
	}

	CachedOwnerComp = &OwnerComp;
	CachedNPC = NPC;
	NPC->OnConversationEnded.AddDynamic(this, &ULCOBTTask_Interrogate::HandleConversationEnded);

	if (TestAutoEndTime > 0.f)
	{
		NPC->GetWorldTimerManager().SetTimer(AutoEndTimer, FTimerDelegate::CreateUObject(NPC, &ALCONPCBase::EndConversation), TestAutoEndTime, false);
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type ULCOBTTask_Interrogate::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ALCONPCBase* NPC = CachedNPC.Get();
	Cleanup();

	// 다른 상태로 바뀌어 중단된 경우이므로 상태는 건드리지 않고 의심도 고정만 푼다.
	if (ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(OwnerComp.GetAIOwner()))
	{
		Controller->EndInterrogation(false);
	}
	if (NPC)
	{
		NPC->EndConversation();
	}

	return EBTNodeResult::Aborted;
}

void ULCOBTTask_Interrogate::HandleConversationEnded()
{
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	Cleanup();

	if (!OwnerComp)
	{
		return;
	}

	if (ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(OwnerComp->GetAIOwner()))
	{
		Controller->EndInterrogation(true);
	}
	FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
}

void ULCOBTTask_Interrogate::Cleanup()
{
	if (ALCONPCBase* NPC = CachedNPC.Get())
	{
		NPC->OnConversationEnded.RemoveDynamic(this, &ULCOBTTask_Interrogate::HandleConversationEnded);
		NPC->GetWorldTimerManager().ClearTimer(AutoEndTimer);
	}
}

FString ULCOBTTask_Interrogate::GetStaticDescription() const
{
	return TestAutoEndTime > 0.f ? FString::Printf(TEXT("Auto end: %.1fs (Test)"), TestAutoEndTime) : TEXT("Wait for EndConversation");
}
