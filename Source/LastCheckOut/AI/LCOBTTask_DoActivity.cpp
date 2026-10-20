#include "AI/LCOBTTask_DoActivity.h"

#include "AIController.h"
#include "AI/Activity/LCOActivitySpot.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Interaction/LCOOpenableBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NPC/LCONPCBase.h"
#include "TimerManager.h"

namespace
{
	// 머리 위 디버그 문구에 표시할 동작 이름
	const TCHAR* GetStepTypeName(ELCOActivityStepType Type)
	{
		switch (Type)
		{
		case ELCOActivityStepType::MoveToSlot: return TEXT("MoveToSlot");
		case ELCOActivityStepType::FaceSlotDirection: return TEXT("FaceSlotDirection");
		case ELCOActivityStepType::PlayMontage: return TEXT("PlayMontage");
		case ELCOActivityStepType::Wait: return TEXT("Wait");
		case ELCOActivityStepType::WaitUntilPlayerNear: return TEXT("WaitUntilPlayerNear");
		case ELCOActivityStepType::WaitUntilPlayerFar: return TEXT("WaitUntilPlayerFar");
		case ELCOActivityStepType::FacePlayer: return TEXT("FacePlayer");
		case ELCOActivityStepType::OpenDoor: return TEXT("OpenDoor");
		case ELCOActivityStepType::CloseDoor: return TEXT("CloseDoor");
		}
		return TEXT("Unknown");
	}
}

ULCOBTTask_DoActivity::ULCOBTTask_DoActivity()
{
	NodeName = TEXT("Do Activity");

	// NPC마다 진행 상태(동작 순서, 타이머)를 따로 가진다.
	bCreateNodeInstance = true;
}

EBTNodeResult::Type ULCOBTTask_DoActivity::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	ALCONPCBase* NPC = AI ? Cast<ALCONPCBase>(AI->GetPawn()) : nullptr;
	if (!AI || !NPC)
	{
		return EBTNodeResult::Failed;
	}

	CachedOwnerComp = &OwnerComp;
	CachedAI = AI;
	CachedNPC = NPC;
	StepIndex = -1;

	// 루틴에서 다음 지점을 고르고 예약한다. 없거나 사용 중이면 잠시 뒤 다음 지점으로 다시 시도한다.
	ALCOActivitySpot* Spot = NPC->GetNextRoutineSpot();
	if (!Spot || !Spot->TryReserve(NPC))
	{
		CachedSpot.Reset();
		FinishAfter(RetryDelay);
		return EBTNodeResult::InProgress;
	}

	CachedSpot = Spot;
	NPC->SetWalkSpeed();
	AI->ReceiveMoveCompleted.AddUniqueDynamic(this, &ULCOBTTask_DoActivity::OnMoveCompleted);

	ScheduleNextStep(0.f);
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type ULCOBTTask_DoActivity::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* AI = CachedAI.Get())
	{
		AI->StopMovement();
	}
	if (ALCONPCBase* NPC = CachedNPC.Get())
	{
		NPC->StopAnimMontage();
	}

	Cleanup();
	return EBTNodeResult::Aborted;
}

FString ULCOBTTask_DoActivity::GetStaticDescription() const
{
	return TEXT("NPC Routine: go to next Activity Spot and run its steps");
}

void ULCOBTTask_DoActivity::ScheduleNextStep(float Delay)
{
	const ALCONPCBase* NPC = CachedNPC.Get();
	if (!NPC)
	{
		return;
	}

	UWorld* World = NPC->GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &ULCOBTTask_DoActivity::RunNextStep), FMath::Max(Delay, KINDA_SMALL_NUMBER), false);
}

void ULCOBTTask_DoActivity::RunNextStep()
{
	ALCONPCBase* NPC = CachedNPC.Get();
	AAIController* AI = CachedAI.Get();
	ALCOActivitySpot* Spot = CachedSpot.Get();
	if (!NPC || !AI || !Spot)
	{
		FinishActivity();
		return;
	}

	// 대화 중에는 대화가 끝날 때까지 기다린다.
	if (NPC->IsInConversation())
	{
		NPC->SetDebugStatus(TEXT("Conversation"));
		ScheduleNextStep(0.5f);
		return;
	}

	// 다른 동작으로 넘어갈 때 이전 동작의 회전(따라 보기)을 끝낸다. 같은 대기 동작을 다시 확인하는 중에는 유지한다.
	const bool bRepeat = bRepeatingStep;
	bRepeatingStep = false;
	if (!bRepeat)
	{
		NPC->GetWorldTimerManager().ClearTimer(FaceTimer);
	}

	++StepIndex;
	const TArray<FLCOActivityStep>& Steps = Spot->GetSteps();
	if (!Steps.IsValidIndex(StepIndex))
	{
		FinishActivity();
		return;
	}

	const FLCOActivityStep& Step = Steps[StepIndex];

	// 머리 위에 현재 동작과 지점 이름을 표시한다.
	NPC->SetDebugStatus(FString::Printf(TEXT("%s\n%s"), GetStepTypeName(Step.Type), *GetNameSafe(Spot)));

	switch (Step.Type)
	{
	case ELCOActivityStepType::MoveToSlot:
	{
		const EPathFollowingRequestResult::Type Request = AI->MoveToLocation(Spot->GetSlotLocation(), Spot->GetAcceptRadius(), true, true, true, false);
		if (Request == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			ScheduleNextStep(0.f);
		}
		else if (Request == EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOBTTask_DoActivity] %s cannot reach %s."), *GetNameSafe(NPC), *GetNameSafe(Spot));
			FinishAfter(RetryDelay);
		}
		// RequestSuccessful이면 OnMoveCompleted에서 이어진다.
		break;
	}

	case ELCOActivityStepType::FaceSlotDirection:
		// 슬롯 방향으로 부드럽게 돈다. 맞춰지면 UpdateFaceSlot이 다음 동작으로 넘기고, 오래 걸리면 2초 뒤에 넘어간다.
		NPC->GetWorldTimerManager().SetTimer(FaceTimer, FTimerDelegate::CreateUObject(this, &ULCOBTTask_DoActivity::UpdateFaceSlot), 0.03f, true);
		ScheduleNextStep(2.f);
		break;

	case ELCOActivityStepType::PlayMontage:
	{
		float Length = 0.f;
		if (Step.Montage)
		{
			Length = NPC->PlayAnimMontage(Step.Montage);
		}
		ScheduleNextStep(Step.bWaitForMontage ? FMath::Max(Length, 0.f) : 0.f);
		break;
	}

	case ELCOActivityStepType::Wait:
		ScheduleNextStep(Step.Duration);
		break;

	case ELCOActivityStepType::OpenDoor:
	case ELCOActivityStepType::CloseDoor:
	{
		// 지점에 연결된 문을 연다 / 닫는다. 문이 없거나 이미 그 상태면 기다리지 않고 넘어간다.
		ALCOOpenableBase* Door = Spot->GetLinkedDoor();
		if (!Door)
		{
			ScheduleNextStep(0.f);
			break;
		}

		if (Step.Type == ELCOActivityStepType::OpenDoor)
		{
			if (Door->IsOpen())
			{
				ScheduleNextStep(0.f);
				break;
			}

			if (Door->IsLocked() && Step.bUnlockIfLocked)
			{
				Door->Unlock();
			}
			Door->Open();
		}
		else
		{
			if (!Door->IsOpen())
			{
				ScheduleNextStep(0.f);
				break;
			}
			Door->Close();
		}

		// 문이 열리고 닫히는 연출이 끝날 때까지 기다린다.
		ScheduleNextStep(Step.Duration);
		break;
	}

	case ELCOActivityStepType::FacePlayer:
	{
		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(NPC, 0);
		if (PlayerPawn && Step.Duration <= 0.f)
		{
			// 한 번에 플레이어 쪽을 본다.
			FVector ToPlayer = PlayerPawn->GetActorLocation() - NPC->GetActorLocation();
			ToPlayer.Z = 0.f;
			if (!ToPlayer.IsNearlyZero())
			{
				NPC->SetActorRotation(ToPlayer.Rotation());
			}
			ScheduleNextStep(0.f);
		}
		else if (PlayerPawn)
		{
			// Duration 동안 부드럽게 플레이어를 따라 본다.
			NPC->GetWorldTimerManager().SetTimer(FaceTimer, FTimerDelegate::CreateUObject(this, &ULCOBTTask_DoActivity::UpdateFacePlayer), 0.03f, true);
			ScheduleNextStep(Step.Duration);
		}
		else
		{
			ScheduleNextStep(0.f);
		}
		break;
	}

	case ELCOActivityStepType::WaitUntilPlayerNear:
	case ELCOActivityStepType::WaitUntilPlayerFar:
	{
		// 조건이 만족될 때까지 같은 동작에서 0.2초마다 다시 확인한다.
		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(NPC, 0);
		const bool bPlayerNear = PlayerPawn && FVector::Dist2D(PlayerPawn->GetActorLocation(), NPC->GetActorLocation()) <= Step.Distance;
		const bool bConditionMet = (Step.Type == ELCOActivityStepType::WaitUntilPlayerNear) ? bPlayerNear : !bPlayerNear;
		if (!bConditionMet)
		{
			// 기다리는 동안 플레이어를 따라 보게 설정했으면 따라 보기 타이머를 한 번만 시작한다.
			if (Step.bFacePlayerWhileWaiting && !NPC->GetWorldTimerManager().IsTimerActive(FaceTimer))
			{
				NPC->GetWorldTimerManager().SetTimer(FaceTimer, FTimerDelegate::CreateUObject(this, &ULCOBTTask_DoActivity::UpdateFacePlayer), 0.03f, true);
			}

			bRepeatingStep = true;
			--StepIndex;
			ScheduleNextStep(0.2f);
			break;
		}

		ScheduleNextStep(0.f);
		break;
	}
	}
}

void ULCOBTTask_DoActivity::UpdateFacePlayer()
{
	ALCONPCBase* NPC = CachedNPC.Get();
	const APawn* PlayerPawn = NPC ? UGameplayStatics::GetPlayerPawn(NPC, 0) : nullptr;
	if (!NPC || !PlayerPawn)
	{
		return;
	}

	FVector ToPlayer = PlayerPawn->GetActorLocation() - NPC->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (ToPlayer.IsNearlyZero())
	{
		return;
	}

	NPC->SetActorRotation(FMath::RInterpTo(NPC->GetActorRotation(), ToPlayer.Rotation(), 0.03f, FacePlayerInterpSpeed));
}

void ULCOBTTask_DoActivity::UpdateFaceSlot()
{
	ALCONPCBase* NPC = CachedNPC.Get();
	const ALCOActivitySpot* Spot = CachedSpot.Get();
	if (!NPC || !Spot)
	{
		return;
	}

	const FRotator Target(0.f, Spot->GetSlotRotation().Yaw, 0.f);
	const FRotator Next = FMath::RInterpTo(NPC->GetActorRotation(), Target, 0.03f, FacePlayerInterpSpeed);
	NPC->SetActorRotation(Next);

	// 거의 맞춰지면 정확히 맞추고 다음 동작으로 넘어간다.
	if (FMath::Abs(FMath::FindDeltaAngleDegrees(Next.Yaw, Target.Yaw)) < 1.f)
	{
		NPC->SetActorRotation(Target);
		NPC->GetWorldTimerManager().ClearTimer(FaceTimer);
		ScheduleNextStep(0.f);
	}
}

void ULCOBTTask_DoActivity::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	const ALCONPCBase* NPC = CachedNPC.Get();
	const ALCOActivitySpot* Spot = CachedSpot.Get();
	if (!NPC || !Spot || !Spot->GetSteps().IsValidIndex(StepIndex) || Spot->GetSteps()[StepIndex].Type != ELCOActivityStepType::MoveToSlot)
	{
		return;
	}

	if (Result == EPathFollowingResult::Success)
	{
		ScheduleNextStep(0.f);
	}
	else if (NPC->IsInConversation())
	{
		// 대화 때문에 이동이 중단된 경우. 대화가 끝나면 이 이동을 다시 한다.
		--StepIndex;
		ScheduleNextStep(0.5f);
	}
	else
	{
		FinishAfter(RetryDelay);
	}
}

void ULCOBTTask_DoActivity::FinishAfter(float Delay)
{
	const ALCONPCBase* NPC = CachedNPC.Get();
	UWorld* World = NPC ? NPC->GetWorld() : nullptr;
	if (!World)
	{
		FinishActivity();
		return;
	}

	World->GetTimerManager().SetTimer(StepTimer, FTimerDelegate::CreateUObject(this, &ULCOBTTask_DoActivity::FinishActivity), FMath::Max(Delay, KINDA_SMALL_NUMBER), false);
}

void ULCOBTTask_DoActivity::FinishActivity()
{
	UBehaviorTreeComponent* OwnerComp = CachedOwnerComp.Get();
	Cleanup();

	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
	}
}

void ULCOBTTask_DoActivity::Cleanup()
{
	if (const ALCONPCBase* NPC = CachedNPC.Get())
	{
		if (UWorld* World = NPC->GetWorld())
		{
			World->GetTimerManager().ClearTimer(StepTimer);
			World->GetTimerManager().ClearTimer(FaceTimer);
		}
	}

	if (AAIController* AI = CachedAI.Get())
	{
		AI->ReceiveMoveCompleted.RemoveDynamic(this, &ULCOBTTask_DoActivity::OnMoveCompleted);
	}

	if (ALCOActivitySpot* Spot = CachedSpot.Get())
	{
		Spot->Release(CachedNPC.Get());
	}
	CachedSpot.Reset();

	if (ALCONPCBase* NPC = CachedNPC.Get())
	{
		NPC->SetDebugStatus(TEXT("Idle"));
	}
}
