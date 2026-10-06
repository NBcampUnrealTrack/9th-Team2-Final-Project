#include "AI/LCOCaretakerAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
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
}

void ALCOCaretakerAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

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
		BB->SetValueAsObject(LCOBlackboardKeys::TargetActor, Actor);
		BB->SetValueAsVector(LCOBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
		SetState(ELCOCaretakerState::Chase);
	}
	else
	{
		BB->ClearValue(LCOBlackboardKeys::TargetActor);
		BB->SetValueAsVector(LCOBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
		SetState(ELCOCaretakerState::LostPlayer);
	}
}
