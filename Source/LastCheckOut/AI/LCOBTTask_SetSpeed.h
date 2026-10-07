#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LCOBTTask_SetSpeed.generated.h"

UENUM(BlueprintType)
enum class ELCOSpeedMode : uint8
{
	Walk,
	Run
};

/* NPC의 이동 속도를 걷기/달리기로 전환한다. 실제 속도 값은 NPC 클래스가 가진다. */
UCLASS()
class LASTCHECKOUT_API ULCOBTTask_SetSpeed : public UBTTaskNode
{
	GENERATED_BODY()

public:
	ULCOBTTask_SetSpeed();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Speed")
	ELCOSpeedMode SpeedMode = ELCOSpeedMode::Walk;
};
