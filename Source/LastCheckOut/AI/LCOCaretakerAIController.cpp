#include "AI/LCOCaretakerAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "DrawDebugHelpers.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

ALCOCaretakerAIController::ALCOCaretakerAIController()
{
	PrimaryActorTick.bCanEverTick = true;

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
		GetWorldTimerManager().ClearTimer(LostGraceTimer);
		SetState(ELCOCaretakerState::Chase);
	}
	else
	{
		// AI가 대상을 놓쳤을 때 마지막 위치를 갱신하고, 유예 시간 뒤에도 감지되지 않으면 LostPlayer로 변경
		BB->SetValueAsVector(LCOBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
		GetWorldTimerManager().SetTimer(LostGraceTimer, this, &ALCOCaretakerAIController::FinishLostGrace, LostGraceTime, false);
	}
}

void ALCOCaretakerAIController::FinishLostGrace()
{
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(LCOBlackboardKeys::TargetActor);
	}
	SetState(ELCOCaretakerState::LostPlayer);
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
	GetWorldTimerManager().ClearTimer(LostGraceTimer);
	StopMovement();
	GetPawn()->TeleportTo(InitialTransform.GetLocation(), InitialTransform.GetRotation().Rotator());
	AIPerception->ForgetAll();

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->ClearValue(LCOBlackboardKeys::TargetActor);
	}
	SetState(ELCOCaretakerState::Patrol);
}

void ALCOCaretakerAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugInfo();
	}
#endif
}

void ALCOCaretakerAIController::DrawDebugInfo() const
{
#if ENABLE_DRAW_DEBUG
	const APawn* NPC = GetPawn();
	UWorld* World = GetWorld();
	if (!NPC || !World)
	{
		return;
	}

	const FVector Origin = NPC->GetActorLocation();
	const FVector Forward = NPC->GetActorForwardVector();
	const FVector Ground = Origin - FVector(0.f, 0.f, 80.f);

	// 상태별 색상
	FColor StateColor = FColor::Green;
	switch (CurrentState)
	{
	case ELCOCaretakerState::Suspicious:
	case ELCOCaretakerState::Search:
	case ELCOCaretakerState::LostPlayer:
		StateColor = FColor::Yellow;
		break;
	case ELCOCaretakerState::Chase:
		StateColor = FColor::Red;
		break;
	default:
		break;
	}

	// 시야 거리(파랑), 시야 상실 거리(회색), 시야각 원뿔(하늘색), 잡기 거리(빨강)
	DrawDebugCircle(World, Ground, SightRadius, 48, FColor::Blue, false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugCircle(World, Ground, LoseSightRadius, 48, FColor(128, 128, 128), false, -1.f, 0, 2.f, FVector::ForwardVector, FVector::RightVector, false);
	DrawDebugCone(World, Origin, Forward, SightRadius, FMath::DegreesToRadians(PeripheralVisionAngle), 0.f, 16, FColor::Cyan, false, -1.f, 0, 1.f);
	DrawDebugSphere(World, Origin, CatchDistance, 16, FColor::Red, false, -1.f, 0, 2.f);

	// 상태 텍스트
	DrawDebugString(World, Origin + FVector(0.f, 0.f, 120.f), UEnum::GetDisplayValueAsText(CurrentState).ToString(), nullptr, StateColor, 0.f, true, 1.5f);

	const UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		return;
	}

	// 추격 대상까지의 선과 거리 (잡기 거리 안이면 빨강, 밖이면 노랑)
	if (const AActor* Target = Cast<AActor>(BB->GetValueAsObject(LCOBlackboardKeys::TargetActor)))
	{
		const FVector TargetLocation = Target->GetActorLocation();
		const float Distance = FVector::Dist(Origin, TargetLocation);
		const FColor LineColor = Distance <= CatchDistance ? FColor::Red : FColor::Yellow;

		DrawDebugLine(World, Origin, TargetLocation, LineColor, false, -1.f, 0, 3.f);
		DrawDebugString(World, (Origin + TargetLocation) * 0.5f,
			FString::Printf(TEXT("%.0f / Catch %.0f"), Distance, CatchDistance), nullptr, LineColor, 0.f, true, 1.2f);
	}

	// 마지막으로 본 위치
	if (CurrentState == ELCOCaretakerState::LostPlayer)
	{
		const FVector LastKnown = BB->GetValueAsVector(LCOBlackboardKeys::LastKnownLocation);
		DrawDebugSphere(World, LastKnown, 40.f, 8, FColor::Yellow, false, -1.f, 0, 2.f);
		DrawDebugString(World, LastKnown + FVector(0.f, 0.f, 60.f), TEXT("LastKnown"), nullptr, FColor::Yellow, 0.f, true, 1.2f);
	}
#endif
}
