#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/LCOCaretakerTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "LCOCaretakerAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;

// 관리인 AI 컨트롤러. 감지와 상태 전환을 담당하고, 행동은 Behavior Tree가 실행한다.
UCLASS()
class LASTCHECKOUT_API ALCOCaretakerAIController : public AAIController
{
	GENERATED_BODY()

public:
	ALCOCaretakerAIController();

	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetState(ELCOCaretakerState NewState);

	UFUNCTION(BlueprintPure, Category = "AI")
	ELCOCaretakerState GetState() const { return CurrentState; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float SightRadius = 1500.f;

	// 시야를 잃은 뒤에도 기억하는 범위
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float LoseSightRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float PeripheralVisionAngle = 70.f;

	// 감지한 자극을 잊기까지의 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float SightMaxAge = 3.f;

	// 추격 중 이 거리 안으로 들어오면 플레이어를 잡은 것으로 처리
	UPROPERTY(EditDefaultsOnly, Category = "AI|Catch")
	float CatchDistance = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Catch")
	float CatchCheckInterval = 0.1f;

	// 감지가 끊겨도 이 시간(초) 동안은 추격을 유지한다. 시야 경계에서 감지가 깜빡이는 것을 막는다.
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float LostGraceTime = 1.0f;

	// 시야/잡기 거리/추격 대상을 화면에 그린다. (테스트용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Debug")
	bool bDrawDebug = false;

private:
	void DrawDebugInfo() const;

	// 추격 중인 대상과의 거리를 확인한다.
	void CheckCatch();

	// 임시 처리: 플레이어를 시작 지점으로, 관리인을 최초 위치로 되돌린다.
	void HandlePlayerCaught(AActor* CaughtPlayer);

	FTimerHandle CatchCheckTimer;
	FTimerHandle LostGraceTimer;

	// 유예 시간이 끝났을 때 대상을 지우고 LostPlayer 상태로 전환한다.
	void FinishLostGrace();

	// 관리인이 빙의된 시점의 위치 (잡기 후 복귀 지점)
	FTransform InitialTransform;

	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	ELCOCaretakerState CurrentState = ELCOCaretakerState::Idle;
};
