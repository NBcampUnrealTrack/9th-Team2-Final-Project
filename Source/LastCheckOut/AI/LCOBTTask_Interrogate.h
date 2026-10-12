#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LCOBTTask_Interrogate.generated.h"

class ALCONPCBase;

// 플레이어에게 경고 대화를 건다. 대화가 끝날 때까지 기다렸다가 성공을 반환한다.
// 대화 UI는 NPC의 OnConversationStarted를 받아 열고, 닫을 때 EndConversation()을 호출한다.
UCLASS()
class LASTCHECKOUT_API ULCOBTTask_Interrogate : public UBTTaskNode
{
	GENERATED_BODY()

public:
	ULCOBTTask_Interrogate();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	// 대화 UI가 연결되기 전 테스트용. 이 시간(초)이 지나면 대화를 자동으로 끝낸다.
	// 0이면 UI가 EndConversation()을 호출할 때까지 기다린다.
	UPROPERTY(EditAnywhere, Category = "Interrogate")
	float TestAutoEndTime = 3.f;

private:
	UFUNCTION()
	void HandleConversationEnded();

	void Cleanup();

	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<ALCONPCBase> CachedNPC;
	FTimerHandle AutoEndTimer;
};
