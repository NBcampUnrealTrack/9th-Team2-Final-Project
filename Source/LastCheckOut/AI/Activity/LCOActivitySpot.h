#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOActivitySpot.generated.h"

class UAnimMontage;
class UArrowComponent;
class UTextRenderComponent;
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
	CloseDoor,           // 이 지점에 연결된 문(LinkedDoor)을 닫는다. Duration초 동안 닫히는 연출을 기다린다.
	GrabProp,            // 이 지점에 연결된 물건(LinkedProp)을 잡는다. NPC 앞에 붙어 함께 움직인다. (카트 밀기 등) Duration초 기다린다.
	ReleaseProp          // 들고 있던 물건을 그 자리에 놓는다. Duration초 기다린다.
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

	// GrabProp일 때 물건이 놓이는 위치. NPC 기준(앞 +X, 오른쪽 +Y)이고, Z는 발바닥 기준 높이(cm)이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	FVector PropOffset = FVector(90.f, 0.f, 0.f);

	// GrabProp일 때 물건이 NPC 정면에서 더 돌아가는 각도(도). 카트 손잡이 방향이 어긋날 때 맞춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
	float PropYaw = 0.f;
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

	// 지점 이름(ActivityName). 비어 있으면 액터 이름을 쓴다. 머리 위 디버그 문구와 에디터 표시에 쓴다.
	FString GetDisplayName() const;

	// NPC가 서야 할 위치와 바라볼 방향
	FVector GetSlotLocation() const { return GetActorLocation(); }
	FRotator GetSlotRotation() const { return GetActorRotation(); }

	// 슬롯 위치에 도착했다고 보는 거리(cm)
	float GetAcceptRadius() const { return AcceptRadius; }

	// OpenDoor / CloseDoor 동작이 여닫는 문
	ALCOOpenableBase* GetLinkedDoor() const { return LinkedDoor; }

	// GrabProp 동작이 잡는 물건 (벨보이가 미는 카트 등)
	AActor* GetLinkedProp() const { return LinkedProp; }

	// 슬롯을 예약한다. 이미 다른 NPC가 쓰고 있으면 실패한다.
	bool TryReserve(AActor* User);
	void Release(AActor* User);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	// ActivityName을 바꾸면 월드 아웃라이너의 액터 이름도 같이 바꾼다.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, Category = "Activity")
	TObjectPtr<USceneComponent> Root;

	// 지점 이름을 에디터 뷰포트 위에 글자로 보여 준다. (게임 중에는 숨김)
	UPROPERTY(VisibleAnywhere, Category = "Activity")
	TObjectPtr<UTextRenderComponent> NameLabel;

	// 슬롯이 바라보는 방향을 에디터에서 보여 준다.
	UPROPERTY(VisibleAnywhere, Category = "Activity")
	TObjectPtr<UArrowComponent> Arrow;

	// 지점 이름. 레벨에서 지점을 구별하는 데 쓴다. (예: Bellboy_CartPickup) 뷰포트 글자, 월드 아웃라이너 이름, 머리 위 디버그 문구에 표시된다.
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

	// GrabProp 동작이 잡는 물건(예: 짐 카트). 레벨에 놓은 지점에서 물건을 지정한다.
	UPROPERTY(EditInstanceOnly, Category = "Activity")
	TObjectPtr<AActor> LinkedProp;

private:
	TWeakObjectPtr<AActor> Occupant;
};
