#include "AI/LCOCaretakerAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

ALCOCaretakerAIController::ALCOCaretakerAIController()
{
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerception);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
	SightConfig->SetMaxAge(SightMaxAge);
	// 플레이어는 팀 설정이 없어 Neutral로 취급되므로 모두 감지한다.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void ALCOCaretakerAIController::BeginPlay()
{
	Super::BeginPlay();

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
	SightConfig->SetMaxAge(SightMaxAge);
	AIPerception->ConfigureSense(*SightConfig);

	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ALCOCaretakerAIController::OnTargetPerceptionUpdated);

	GetWorldTimerManager().SetTimer(CatchCheckTimer, this, &ALCOCaretakerAIController::CheckCatch, CatchCheckInterval, true);
}

void ALCOCaretakerAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	InitialTransform = InPawn->GetActorTransform();

	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
		SetState(CurrentState);
	}
}

void ALCOCaretakerAIController::SetState(ELCOCaretakerState NewState)
{
	CurrentState = NewState;

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsEnum(LCOBlackboardKeys::State, static_cast<uint8>(NewState));
	}
}

void ALCOCaretakerAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 플레이어만 대상으로 한다.
	const APawn* SensedPawn = Cast<APawn>(Actor);
	if (!SensedPawn || !SensedPawn->IsPlayerControlled())
	{
		return;
	}

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		//AI가 대상을 포착했을 때 플레이어의 위치와 정보를 블랙보드에 저장, 상태를 Chase로 변경
		BB->SetValueAsObject(LCOBlackboardKeys::TargetActor, Actor);
		BB->SetValueAsVector(LCOBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
		SetState(ELCOCaretakerState::Chase);
	}
	else
	{
		//AI가 대상을 놓쳤을 때 블랙보드의 TargetActor칸을 깨끗하게 비우고, 마지막 위치를 업데이트하고 상태를 LostPlayer로 변경
		BB->ClearValue(LCOBlackboardKeys::TargetActor);
		BB->SetValueAsVector(LCOBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
		SetState(ELCOCaretakerState::LostPlayer);
	}
}

void ALCOCaretakerAIController::CheckCatch()
{
	if (CurrentState != ELCOCaretakerState::Chase || !GetPawn())
	{
		return;
	}

	const UBlackboardComponent* BB = GetBlackboardComponent();
	AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(LCOBlackboardKeys::TargetActor)) : nullptr;
	if (!Target)
	{
		return;
	}

	if (FVector::Dist(GetPawn()->GetActorLocation(), Target->GetActorLocation()) <= CatchDistance)
	{
		HandlePlayerCaught(Target);
	}
}

void ALCOCaretakerAIController::HandlePlayerCaught(AActor* CaughtPlayer)
{
	// 플레이어를 Player Start 위치로 이동
	APawn* PlayerPawn = Cast<APawn>(CaughtPlayer);
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this);
	const AActor* PlayerStart = (GameMode && PlayerPawn) ? GameMode->FindPlayerStart(PlayerPawn->GetController()) : nullptr;
	if (PlayerPawn && PlayerStart)
	{
		PlayerPawn->TeleportTo(PlayerStart->GetActorLocation(), PlayerStart->GetActorRotation());
		if (UPawnMovementComponent* PlayerMovement = PlayerPawn->GetMovementComponent())
		{
			PlayerMovement->StopMovementImmediately();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOCaretakerAIController] Player Start not found. Player was not reset."));
	}

	// 관리인을 최초 위치로 되돌리고 순찰 상태로 복귀
	StopMovement();
	GetPawn()->TeleportTo(InitialTransform.GetLocation(), InitialTransform.GetRotation().Rotator());
	AIPerception->ForgetAll();

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(LCOBlackboardKeys::TargetActor);
	}
	SetState(ELCOCaretakerState::Patrol);
}
