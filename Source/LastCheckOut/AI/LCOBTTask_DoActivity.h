#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Navigation/PathFollowingComponent.h"
#include "LCOBTTask_DoActivity.generated.h"

class AAIController;
class ALCOActivitySpot;
class ALCONPCBase;

// NPC의 루틴 목록에서 다음 활동 지점을 골라 찾아가고, 지점의 동작 목록을 위에서부터 한 줄씩 실행한다.
// 끝나면 지점 예약을 풀고 성공을 반환한다. 대화 중에는 대화가 끝날 때까지 다음 동작으로 넘어가지 않는다.
// BT에서는 이 태스크 하나를 Sequence/Root 아래에 두면 루틴이 반복된다.
UCLASS()
class LASTCHECKOUT_API ULCOBTTask_DoActivity : public UBTTaskNode
{
	GENERATED_BODY()

public:
	ULCOBTTask_DoActivity();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	// 루틴이 비었거나 지점이 사용 중이거나 이동에 실패했을 때, 이 시간(초) 뒤에 다음 시도를 한다.
	UPROPERTY(EditAnywhere, Category = "Activity")
	float RetryDelay = 2.f;

	// FacePlayer 동작에서 플레이어 쪽으로 몸을 돌리는 속도
	UPROPERTY(EditAnywhere, Category = "Activity")
	float FacePlayerInterpSpeed = 8.f;

private:
	UFUNCTION()
	void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	// FacePlayer 동작 중 0.03초마다 플레이어 쪽으로 몸을 돌린다.
	void UpdateFacePlayer();

	// FaceSlotDirection 동작 중 0.03초마다 슬롯 방향으로 부드럽게 돌고, 맞춰지면 다음 동작으로 넘어간다.
	void UpdateFaceSlot();

	// 다음 동작을 Delay초 뒤에 실행한다. 대화 중이면 끝날 때까지 미룬다.
	void ScheduleNextStep(float Delay);
	void RunNextStep();

	// 태스크를 Delay초 뒤에 성공으로 끝낸다. (바로 끝내면 BT가 매 프레임 반복하는 것을 막는다)
	void FinishAfter(float Delay);
	void FinishActivity();
	void Cleanup();

	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<AAIController> CachedAI;
	TWeakObjectPtr<ALCONPCBase> CachedNPC;
	TWeakObjectPtr<ALCOActivitySpot> CachedSpot;

	int32 StepIndex = -1;
	FTimerHandle StepTimer;
	FTimerHandle FaceTimer;

	// 같은 동작을 다시 확인하는 중이면 true (대기 동작의 반복 확인에서 따라 보기 타이머를 끊지 않기 위함)
	bool bRepeatingStep = false;
};
