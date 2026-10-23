#pragma once

#include "CoreMinimal.h"
#include "NPC/LCONPCBase.h"
#include "LCOCaretaker.generated.h"

class ATargetPoint;

// 관리인 (추격자). AI 로직은 AIController/BehaviorTree에서 담당한다.
UCLASS()
class LASTCHECKOUT_API ALCOCaretaker : public ALCONPCBase
{
	GENERATED_BODY()

public:
	ALCOCaretaker();

	// 다음 순찰 지점의 위치를 OutLocation에 담고 인덱스를 넘긴다. 지점이 없으면 false
	bool GetNextPatrolLocation(FVector& OutLocation);

	// 문 통과 구간(LCODoorBendZone)에 들어갈 때/나올 때 호출한다. 구간이 겹쳐도 안전하도록 들어간 횟수를 센다.
	UFUNCTION(BlueprintCallable, Category = "Caretaker|Door")
	void EnterDoorBendZone();

	UFUNCTION(BlueprintCallable, Category = "Caretaker|Door")
	void ExitDoorBendZone();

	// 허리를 숙인 정도 0~1. 애님 BP가 읽어서 척추를 숙이는 데 쓴다.
	UPROPERTY(BlueprintReadOnly, Category = "Caretaker|Door")
	float DoorBendAlpha = 0.f;

protected:
	virtual void Tick(float DeltaSeconds) override;

	// 허리를 숙이고 펴는 속도(초당 alpha 변화량). 클수록 빨리 숙인다.
	UPROPERTY(EditAnywhere, Category = "Caretaker|Door", meta = (ClampMin = "0.1"))
	float DoorBendBlendSpeed = 3.f;

	// 문을 통과하는 동안 이동 속도 배율. 느려져서 플레이어와 거리가 벌어진다.
	UPROPERTY(EditAnywhere, Category = "Caretaker|Door", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float DoorBendSpeedScale = 0.6f;

	// 순찰 지점 (맵에 배치한 인스턴스에서 TargetPoint를 순서대로 지정)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Patrol")
	TArray<TObjectPtr<ATargetPoint>> PatrolPoints;

private:
	int32 PatrolIndex = 0;
	int32 DoorBendZoneCount = 0;
};
