#include "AI/LCOCaretakerAIController.h"

#include "AI/Chasing/LCOChasingManager.h"

#include "BehaviorTree/BehaviorTree.h"
#include "DrawDebugHelpers.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NPC/LCONPCBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
// 테스트용 콘솔 명령: 플레이어의 의심도를 직접 조절한다. (출시 빌드 제외)
namespace
{
	USuspicionComponent* FindPlayerSuspicion(UWorld* World)
	{
		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
		return PlayerPawn ? PlayerPawn->FindComponentByClass<USuspicionComponent>() : nullptr;
	}

	float GetFirstArg(const TArray<FString>& Args, float DefaultValue)
	{
		return Args.IsEmpty() ? DefaultValue : FCString::Atof(*Args[0]);
	}

	FAutoConsoleCommandWithWorldAndArgs GAddSuspicionCommand(
		TEXT("LCO.Suspicion.Add"), TEXT("LCO.Suspicion.Add <Amount> : 의심도를 올린다. (기본 15)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USuspicionComponent* Suspicion = FindPlayerSuspicion(World))
			{
				Suspicion->AddSuspicion(GetFirstArg(Args, 15.f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GReduceSuspicionCommand(
		TEXT("LCO.Suspicion.Reduce"), TEXT("LCO.Suspicion.Reduce <Amount> : 의심도를 내린다. (기본 20)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USuspicionComponent* Suspicion = FindPlayerSuspicion(World))
			{
				Suspicion->ReduceSuspicion(GetFirstArg(Args, 20.f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GSetSuspicionCommand(
		TEXT("LCO.Suspicion.Set"), TEXT("LCO.Suspicion.Set <Value> : 의심도를 지정한 값으로 맞춘다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			USuspicionComponent* Suspicion = FindPlayerSuspicion(World);
			if (!Suspicion)
			{
				return;
			}

			const float Diff = GetFirstArg(Args, 0.f) - Suspicion->GetSuspicion();
			if (Diff > 0.f)
			{
				Suspicion->AddSuspicion(Diff);
			}
			else
			{
				Suspicion->ReduceSuspicion(-Diff);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GResetSuspicionCommand(
		TEXT("LCO.Suspicion.Reset"), TEXT("LCO.Suspicion.Reset : 의심도를 0으로 초기화한다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (USuspicionComponent* Suspicion = FindPlayerSuspicion(World))
			{
				Suspicion->ResetSuspicion();
			}
		}));
}
#endif

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

	// 레벨에 배치된 체이싱 매니저를 찾는다. 없으면 안전지대 판정 없이 동작한다.
	ChasingManager = Cast<ALCOChasingManager>(UGameplayStatics::GetActorOfClass(this, ALCOChasingManager::StaticClass()));

	// 플레이어가 아직 생성되지 않았을 수 있어 바인딩될 때까지 재시도한다.
	TryBindSuspicion();
	if (!BoundSuspicion.IsValid())
	{
		GetWorldTimerManager().SetTimer(SuspicionBindTimer, this, &ALCOCaretakerAIController::TryBindSuspicion, 0.5f, true);
	}
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
	// 의심도 기준으로 동작할 때는 시야 감지로 상태를 바꾸지 않는다.
	if (!bUseSightForChase)
	{
		return;
	}

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

	// 안전지대(로비) 안에서는 잡을 수 없다.
	if (ChasingManager.IsValid() && ChasingManager->IsPlayerInSafeZone())
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
	// 체이싱이 끝났으므로 의심도를 초기화한다.
	if (BoundSuspicion.IsValid())
	{
		BoundSuspicion->ResetSuspicion();
	}
	WarningCount = 0;

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

	// 체이싱 중이었다면 실패로 종료한다. (시골청년 제거, 의심도 초기화)
	if (ChasingManager.IsValid())
	{
		ChasingManager->EndChasing(ELCOChasingEndReason::Caught);
	}
}

void ALCOCaretakerAIController::StopChase()
{
	GetWorldTimerManager().ClearTimer(LostGraceTimer);
	StopMovement();
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
	case ELCOCaretakerState::Interrogate:
		StateColor = FColor::Orange;
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

	// 플레이어 의심도 수치와 구간
	if (BoundSuspicion.IsValid())
	{
		DrawDebugString(World, Origin + FVector(0.f, 0.f, 150.f),
			FString::Printf(TEXT("Suspicion %.0f (%s)"), BoundSuspicion->GetSuspicion(), *UEnum::GetDisplayValueAsText(BoundSuspicion->GetSuspicionLevel()).ToString()),
			nullptr, FColor::White, 0.f, true, 1.2f);
	}

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

void ALCOCaretakerAIController::TryBindSuspicion()
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	USuspicionComponent* Suspicion = PlayerPawn ? PlayerPawn->FindComponentByClass<USuspicionComponent>() : nullptr;
	if (!Suspicion)
	{
		return;
	}

	BoundSuspicion = Suspicion;
	Suspicion->OnSuspicionChanged.AddDynamic(this, &ALCOCaretakerAIController::HandleSuspicionChanged);
	GetWorldTimerManager().ClearTimer(SuspicionBindTimer);
}

void ALCOCaretakerAIController::HandleSuspicionChanged(float NewValue, ELCOSuspicionLevel NewLevel)
{
	if (bUseSightForChase)
	{
		return;
	}

	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB || NewLevel == LastSuspicionLevel)
	{
		return;
	}
	LastSuspicionLevel = NewLevel;

	// 경고 횟수를 넘겼으면 대화 없이 의심도를 100으로 올려 추격으로 넘긴다.
	// 같은 Broadcast 안에서 다시 변경하면 구독자들이 순서가 뒤바뀐 알림을 받으므로 다음 틱에 처리한다.
	if (NewLevel == ELCOSuspicionLevel::Danger && bEscalateAfterWarnings && WarningCount >= MaxWarnings)
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (BoundSuspicion.IsValid())
			{
				BoundSuspicion->AddSuspicion(1000.f);
			}
		}));
		return;
	}

	// 구간에 진입하는 순간의 플레이어 정보를 Blackboard에 기록한다. (계속 추적하지 않는다.)
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	switch (NewLevel)
	{
	case ELCOSuspicionLevel::Calm:
		BB->ClearValue(LCOBlackboardKeys::TargetActor);
		break;
	case ELCOSuspicionLevel::Suspicious:
	case ELCOSuspicionLevel::Alert:
		// 수색 기준점
		if (PlayerPawn)
		{
			BB->SetValueAsVector(LCOBlackboardKeys::LastKnownLocation, PlayerPawn->GetActorLocation());
		}
		break;
	case ELCOSuspicionLevel::Danger:
	case ELCOSuspicionLevel::Max:
		// 시야와 관계없이 플레이어를 대상으로 한다.
		BB->SetValueAsObject(LCOBlackboardKeys::TargetActor, PlayerPawn);
		break;
	}

	SetState(LevelToState(NewLevel));
}

ELCOCaretakerState ALCOCaretakerAIController::LevelToState(ELCOSuspicionLevel Level)
{
	switch (Level)
	{
	case ELCOSuspicionLevel::Suspicious:
		return ELCOCaretakerState::Suspicious;
	case ELCOSuspicionLevel::Alert:
		return ELCOCaretakerState::Search;
	case ELCOSuspicionLevel::Danger:
		return ELCOCaretakerState::Interrogate;
	case ELCOSuspicionLevel::Max:
		return ELCOCaretakerState::Chase;
	case ELCOSuspicionLevel::Calm:
	default:
		return ELCOCaretakerState::Patrol;
	}
}

bool ALCOCaretakerAIController::BeginInterrogation(APawn* Player)
{
	ALCONPCBase* NPC = Cast<ALCONPCBase>(GetPawn());
	if (!NPC || !Player || NPC->IsInConversation())
	{
		return false;
	}

	++WarningCount;

	// 대화 중에는 의심도가 오르거나 내려가지 않도록 고정한다.
	if (BoundSuspicion.IsValid())
	{
		BoundSuspicion->SetFrozen(true);
	}

	NPC->BeginConversation(Player);
	return true;
}

void ALCOCaretakerAIController::EndInterrogation(bool bContinueSearch)
{
	if (BoundSuspicion.IsValid())
	{
		BoundSuspicion->SetFrozen(false);
	}

	if (!bContinueSearch)
	{
		return;
	}

	// 경고가 끝나면 플레이어 주변에 머물며 수색한다. (의심도 100을 기다리는 상태)
	UBlackboardComponent* BB = GetBlackboardComponent();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (BB && PlayerPawn)
	{
		BB->SetValueAsVector(LCOBlackboardKeys::LastKnownLocation, PlayerPawn->GetActorLocation());
	}
	SetState(ELCOCaretakerState::Search);
}
