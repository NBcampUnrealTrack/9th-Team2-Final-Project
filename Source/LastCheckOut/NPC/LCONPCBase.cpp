#include "NPC/LCONPCBase.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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
}

void ALCONPCBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AActor* Partner = ConversationPartner.Get();
	if (!Partner)
	{
		SetActorTickEnabled(false);
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

void ALCONPCBase::SetMovementSpeed(float NewSpeed)
{
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
}

void ALCONPCBase::SetWalkSpeed()
{
	SetMovementSpeed(WalkSpeed);
}

void ALCONPCBase::SetRunSpeed()
{
	SetMovementSpeed(RunSpeed);
}

void ALCONPCBase::BeginConversation(AActor* Partner)
{
	if (!Partner || bInConversation)
	{
		return;
	}

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
	SetActorTickEnabled(false);

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
	if (bCanTalk)
	{
		BeginConversation(Interactor);
	}
}

bool ALCONPCBase::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 대화할 수 없는 NPC이거나 이미 대화 중이면 상호작용 불가
	return !bCanTalk || bInConversation;
}
