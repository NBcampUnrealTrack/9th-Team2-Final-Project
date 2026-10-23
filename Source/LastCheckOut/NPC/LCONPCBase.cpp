#include "NPC/LCONPCBase.h"

#include "AIController.h"
#include "AI/Activity/LCOActivitySpot.h"
#include "AI/Chasing/LCOChasingManager.h"
#include "BehaviorTree/BehaviorTree.h"
#include "DrawDebugHelpers.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#pragma region 초기화와 이동 속도

ALCONPCBase::ALCONPCBase()
{
	// Tick은 대화 중에만 켠다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
}

void ALCONPCBase::BeginPlay()
{
	Super::BeginPlay();

	SetWalkSpeed();

	// 디버그 문구를 표시하려면 대화 중이 아니어도 Tick이 필요하다.
	if (bDrawDebugStatus)
	{
		SetActorTickEnabled(true);
	}
}

void ALCONPCBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 루틴용 BT가 지정되어 있으면 AI가 빙의될 때 실행한다.
	if (DefaultBehaviorTree)
	{
		if (AAIController* AIController = Cast<AAIController>(NewController))
		{
			AIController->RunBehaviorTree(DefaultBehaviorTree);
		}
	}
}

void ALCONPCBase::SetMovementSpeed(float NewSpeed)
{
	BaseMoveSpeed = NewSpeed;
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * MoveSpeedScale;
}

void ALCONPCBase::SetMoveSpeedScale(float NewScale)
{
	MoveSpeedScale = FMath::Max(NewScale, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * MoveSpeedScale;
}

void ALCONPCBase::SetWalkSpeed()
{
	SetMovementSpeed(WalkSpeed);
}

void ALCONPCBase::SetRunSpeed()
{
	SetMovementSpeed(RunSpeed);
}

#pragma endregion

#pragma region 대화

void ALCONPCBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugStatus && !DebugStatus.IsEmpty())
	{
		DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight() + 40.f), DebugStatus, nullptr, FColor::Yellow, 0.f, true);
	}
#endif

	const AActor* Partner = ConversationPartner.Get();
	if (!Partner)
	{
		// 대화 상대가 없으면 회전할 필요가 없으므로, 디버그 표시가 꺼져 있을 때만 Tick을 끈다.
		if (!bDrawDebugStatus)
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	// 수평 방향으로만 대화 상대를 바라본다.
	FVector ToPartner = Partner->GetActorLocation() - GetActorLocation();
	ToPartner.Z = 0.f;
	if (ToPartner.IsNearlyZero())
	{
		return;
	}

	SetActorRotation(FMath::RInterpTo(GetActorRotation(), ToPartner.Rotation(), DeltaSeconds, TurnInterpSpeed));
}

void ALCONPCBase::BeginConversation(AActor* Partner)
{
	if (!Partner || bInConversation)	return;

	bInConversation = true;
	ConversationPartner = Partner;

	// 이동과 BT를 멈춘다.
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		if (AIController->BrainComponent)
		{
			AIController->BrainComponent->PauseLogic(TEXT("Conversation"));
		}
	}

	SetActorTickEnabled(true);
	OnConversationStarted.Broadcast(Partner);
}

void ALCONPCBase::EndConversation()
{
	if (!bInConversation)
	{
		return;
	}

	bInConversation = false;
	ConversationPartner.Reset();
	SetActorTickEnabled(bDrawDebugStatus);

	// BT를 재개한다.
	if (const AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (AIController->BrainComponent)
		{
			AIController->BrainComponent->ResumeLogic(TEXT("Conversation"));
		}
	}

	OnConversationEnded.Broadcast();
}

void ALCONPCBase::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	if (bCanTalk && !IsPlayerBeingChased())
	{
		BeginConversation(Interactor);
	}
}

bool ALCONPCBase::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 대화할 수 없는 NPC이거나 이미 대화 중이면 상호작용 불가. 체이싱 중에는 F로 말을 걸 수 없다. (대화로 관리인에게 잡히는 것을 피하는 것을 막는다)
	return !bCanTalk || bInConversation || IsPlayerBeingChased();
}

bool ALCONPCBase::IsPlayerBeingChased() const
{
	const ALCOChasingManager* Manager = Cast<ALCOChasingManager>(UGameplayStatics::GetActorOfClass(this, ALCOChasingManager::StaticClass()));
	return Manager && Manager->IsChasing();
}

#pragma endregion

#pragma region 루틴 (활동 지점)

ALCOActivitySpot* ALCONPCBase::GetNextRoutineSpot()
{
	// 비어 있는 칸은 건너뛴다.
	for (int32 Attempt = 0; Attempt < Routine.Num(); ++Attempt)
	{
		ALCOActivitySpot* Spot = Routine[RoutineIndex % Routine.Num()];
		RoutineIndex = (RoutineIndex + 1) % Routine.Num();
		if (Spot)
		{
			return Spot;
		}
	}

	return nullptr;
}

#pragma endregion
