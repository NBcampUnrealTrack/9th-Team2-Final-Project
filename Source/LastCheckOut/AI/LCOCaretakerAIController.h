#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/LCOCaretakerTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "LCOCaretakerAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UBehaviorTree;

/* 관리인 AI 컨트롤러. 감지와 상태 전환을 담당하고, 행동은 Behavior Tree가 실행한다. */
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
	virtual void OnPossess(APawn* InPawn) override;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float SightRadius = 1500.f;

	/** 시야를 잃은 뒤에도 기억하는 범위 */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float LoseSightRadius = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float PeripheralVisionAngle = 70.f;

	/** 감지한 자극을 잊기까지의 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sight")
	float SightMaxAge = 3.f;

private:
	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	ELCOCaretakerState CurrentState = ELCOCaretakerState::Idle;
};
