#pragma once

#include "CoreMinimal.h"
#include "NPC/LCONPCBase.h"
#include "LCOCaretaker.generated.h"

class ATargetPoint;

/* 관리인 (추격자). AI 로직은 AIController/BehaviorTree에서 담당한다. */
UCLASS()
class LASTCHECKOUT_API ALCOCaretaker : public ALCONPCBase
{
	GENERATED_BODY()

public:
	ALCOCaretaker();

	/* 다음 순찰 지점의 위치를 OutLocation에 담고 인덱스를 넘긴다. 지점이 없으면 false */
	bool GetNextPatrolLocation(FVector& OutLocation);

protected:
	/* 순찰 지점 (맵에 배치한 인스턴스에서 TargetPoint를 순서대로 지정) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Patrol")
	TArray<TObjectPtr<ATargetPoint>> PatrolPoints;

private:
	int32 PatrolIndex = 0;
};
