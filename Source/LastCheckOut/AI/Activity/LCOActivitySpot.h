#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOActivitySpot.generated.h"

class UAnimMontage;
class UArrowComponent;
class ALCOOpenableBase;

// 활동 지점에서 NPC가 순서대로 실행하는 동작의 종류
UENUM(BlueprintType)
enum class ELCOActivityStepType : uint8
{
	MoveToSlot,          // 슬롯 위치로 이동
	FaceSlotDirection,   // 슬롯이 바라보는 방향으로 몸을 돌림
	PlayMontage,         // 몽타주 재생
	Wait,                // 지정한 시간 대기
	WaitUntilPlayerNear, // 플레이어가 Distance 안으로 들어올 때까지 대기 (앉아 있다가 일어나기 등)
	WaitUntilPlayerFar,  // 플레이어가 Distance 밖으로 벗어날 때까지 대기
	FacePlayer,          // 플레이어를 바라본다. Duration이 0이면 한 번에 돌고, 0보다 크면 그 시간 동안 부드럽게 따라 본다.
	OpenDoor,            // 이 지점에 연결된 문(LinkedDoor)을 연다. Duration초 동안 열리는 연출을 기다린다.
	CloseDoor            // 이 지점에 연결된 문(LinkedDoor)을 닫는다. Duration초 동안 닫히는 연출을 기다린다.
};

USTRUCT(BlueprintType)
struct FLCOActivityStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	ELCOActivityStepType Type = ELCOActivityStepType::Wait;

	// Wait일 때 대기 시간(초), FacePlayer일 때 플레이어를 따라 보는 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	float Duration = 3.f;

	// WaitUntilPlayerNear / WaitUntilPlayerFar일 때 플레이어와의 거리(cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	float Distance = 400.f;

	// WaitUntilPlayerNear / WaitUntilPlayerFar일 때 기다리는 동안 플레이어를 부드럽게 따라 볼지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	bool bFacePlayerWhileWaiting = false;

	// OpenDoor일 때 문이 잠겨 있으면 먼저 잠금을 푼다. (직원이 열쇠를 가진 것으로 본다)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	bool bUnlockIfLocked = true;

	// PlayMontage일 때 재생할 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	TObjectPtr<UAnimMontage> Montage;

	// PlayMontage일 때 몽타주가 끝날 때까지 기다릴지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	bool bWaitForMontage = true;
};

// NPC가 찾아가서 할 일을 하는 지점. 레벨에 배치하고, NPC 인스턴스의 Routine 목록에 순서대로 지정한다.
// 슬롯 위치와 방향은 이 액터의 위치와 회전이며(화살표가 바라보는 방향), 한 번에 한 명만 쓸 수 있다.
UCLASS()
class LASTCHECKOUT_API ALCOActivitySpot : public AActor
{
	GENERATED_BODY()

public:
	ALCOActivitySpot();

	const TArray<FLCOActivityStep>& GetSteps() const { return Steps; }

	// NPC가 서야 할 위치와 바라볼 방향
	FVector GetSlotLocation() const { return GetActorLocation(); }
	FRotator GetSlotRotation() const { return GetActorRotation(); }

	// 슬롯 위치에 도착했다고 보는 거리(cm)
	float GetAcceptRadius() const { return AcceptRadius; }

	// OpenDoor / CloseDoor 동작이 여닫는 문
	ALCOOpenableBase* GetLinkedDoor() const { return LinkedDoor; }

	// 슬롯을 예약한다. 이미 다른 NPC가 쓰고 있으면 실패한다.
	bool TryReserve(AActor* User);
	void Release(AActor* User);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Activity")
	TObjectPtr<USceneComponent> Root;

	// 슬롯이 바라보는 방향을 에디터에서 보여 준다.
	UPROPERTY(VisibleAnywhere, Category = "Activity")
	TObjectPtr<UArrowComponent> Arrow;

	// 지점 이름 (디버그용)
	UPROPERTY(EditAnywhere, Category = "Activity")
	FName ActivityName;

	// NPC가 이 지점에서 위에서부터 순서대로 실행하는 동작 목록
	UPROPERTY(EditAnywhere, Category = "Activity")
	TArray<FLCOActivityStep> Steps;

	UPROPERTY(EditAnywhere, Category = "Activity")
	float AcceptRadius = 40.f;

	// OpenDoor / CloseDoor 동작이 여닫는 문. 레벨에 놓은 지점에서 문을 지정한다.
	UPROPERTY(EditInstanceOnly, Category = "Activity")
	TObjectPtr<ALCOOpenableBase> LinkedDoor;

private:
	TWeakObjectPtr<AActor> Occupant;
};
