#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/LCOCaretakerTypes.h"
#include "AI/SuspicionComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "LCOCaretakerAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;
class ALCOChasingManager;

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
	
	bool BeginInterrogation(APawn* Player);
	void EndInterrogation(bool bContinueSearch);

	// 추격을 멈추고 순찰로 되돌린다. (체이싱이 끝났을 때 매니저가 호출)
	void StopChase();

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

	// true면 시야 감지로 추격한다. (의심도 연동 전 테스트용 임시 동작)
	// false면 플레이어의 의심도 구간에 따라 상태를 바꾼다.
	UPROPERTY(EditDefaultsOnly, Category = "AI|Suspicion")
	bool bUseSightForChase = true;

	// true면 고위험 구간에서 경고 대화를 MaxWarnings번 한 뒤, 다시 고위험에 진입하면 대화 없이 의심도를 100으로 올려 추격한다.
	// false면 고위험 구간에 진입할 때마다 경고 대화를 한다.
	UPROPERTY(EditDefaultsOnly, Category = "AI|Suspicion")
	bool bEscalateAfterWarnings = true;

	// 추격으로 넘어가기 전까지 허용하는 경고 대화 횟수
	UPROPERTY(EditDefaultsOnly, Category = "AI|Suspicion", meta = (EditCondition = "bEscalateAfterWarnings", ClampMin = "1"))
	int32 MaxWarnings = 2;

	// 감지가 끊겨도 이 시간(초) 동안은 추격을 유지한다. 시야 경계에서 감지가 깜빡이는 것을 막는다.
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float LostGraceTime = 1.0f;

	// 시야/잡기 거리/추격 대상을 화면에 그린다. (테스트용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Debug")
	bool bDrawDebug = false;

private:
	// 플레이어의 SuspicionComponent를 찾아 의심도 변화에 바인딩한다. 플레이어가 아직 없으면 타이머로 재시도한다.
	void TryBindSuspicion();

	UFUNCTION()
	void HandleSuspicionChanged(float NewValue, ELCOSuspicionLevel NewLevel);

	// 의심도 구간에 대응하는 관리인 상태. 구간과 상태의 대응은 이 함수에서만 정한다.
	//   Calm       (0~19)   -> Patrol      순찰
	//   Suspicious (20~49)  -> Suspicious  주변 확인
	//   Alert      (50~69)  -> Search      수색
	//   Danger     (70~99)  -> Interrogate 플레이어에게 다가가 경고 대화 (잡지 않는다)
	//   Max        (100)    -> Chase       추격 (잡으면 잡기 처리)
	static ELCOCaretakerState LevelToState(ELCOSuspicionLevel Level);

	TWeakObjectPtr<USuspicionComponent> BoundSuspicion;
	TWeakObjectPtr<ALCOChasingManager> ChasingManager;

	// 직전에 처리한 의심도 구간. 구간이 바뀔 때만 상태를 갱신한다.
	ELCOSuspicionLevel LastSuspicionLevel = ELCOSuspicionLevel::Calm;

	// 지금까지 한 경고 대화 횟수. 잡히면 초기화한다.
	int32 WarningCount = 0;
	FTimerHandle SuspicionBindTimer;

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
