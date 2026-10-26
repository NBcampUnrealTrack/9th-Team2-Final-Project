#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "Save/LCOSaveableActor.h"
#include "LCOElevator.generated.h"

// 엘리베이터 층 정보
USTRUCT(BlueprintType)
struct FLCOElevatorFloor
{
	GENERATED_BODY()

	// 층 ID. 이 층으로 보내는 버튼(LCOInteract)의 InputID와 같게 지정 (예: "1F", "2F")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator")
	FName FloorID;

	// 이 층에 멈췄을 때 Car의 높이 (액터 기준 상대 Z)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator")
	float Height = 0.f;

	// 이 층으로 가는 데 필요한 아이템 (카드키 등). 비워두면 제한 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator")
	FName RequiredItemID;
};

UENUM(BlueprintType)
enum class ELCOElevatorState : uint8
{
	Idle          UMETA(DisplayName = "대기 (문 열림)"),
	DoorsClosing  UMETA(DisplayName = "문 닫는 중"),
	Moving        UMETA(DisplayName = "이동 중"),
	DoorsOpening  UMETA(DisplayName = "문 여는 중")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorArrived, ALCOElevator*, Elevator, int32, FloorIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorFloorEvent, ALCOElevator*, Elevator, int32, FloorIndex);

// 같은 맵 안에서 층을 오가는 엘리베이터
// - 이 액터: 통로 + Car(움직이는 칸, 안쪽 문, 안쪽 층 버튼)
// - 층별 바깥 문 + 호출 버튼: ALCOElevatorLanding을 층마다 따로 배치 (층이 늘어도 이 BP는 그대로)
// - 안쪽 층 버튼 = Car 아래의 LCOInteract 컴포넌트, InputID = 목표 층의 FloorID
// - 흐름: 대기(문 열림) → 문 닫힘 → Car 이동 → 도착 → 문 열림
// - Car(이동하는 칸) 아래에 바닥 / 벽 / 안쪽 버튼 메시를 붙이면 같이 움직임. 플레이어는 CharacterMovement가 바닥을 따라 같이 이동시킴
// - 안쪽 문 애니메이션과 소리는 BP 자식에서 구현. 바깥 문은 승강장(ALCOElevatorLanding)이 델리게이트를 받아 처리
// - 현재 층은 저장하지 않음: 맵에 들어오거나 불러오면 항상 StartFloorIndex 층에서 대기 (다른 층에서는 호출 버튼으로 부름)
// - 전원 상태만 맵 이동 / 세이브 시 LCOWorldStateSubsystem이 저장·복원 (스위치로 켠 전원이 불러오기 후 꺼지지 않도록)
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOElevator : public AActor, public ILCOInteractionReceiver, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOElevator();

	virtual void Tick(float DeltaTime) override;

	// ILCOInteractionReceiver
	// 버튼의 InputID(FloorID)로 해당 층 요청
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 대기 상태가 아니면(문 여닫는 중 / 이동 중) 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 전원 꺼짐 / 카드키 없음일 때 전용 문구
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 전원 상태만 저장 (bIsEnabled). 현재 층은 저장하지 않음
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;

	// 전원 상태만 복원
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	// 층 요청 (버튼 외에 스토리 이벤트 등에서도 호출 가능). 요청이 받아들여지면 true
	UFUNCTION(BlueprintCallable, Category = "Elevator")
	bool RequestFloor(FName FloorID, AActor* Requester);

	// 전원 켜기 / 끄기 (스위치, 퍼즐 결과 등에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Elevator")
	void SetPowered(bool bNewPowered);

	UFUNCTION(BlueprintPure, Category = "Elevator")
	bool IsPowered() const { return bIsPowered; }

	UFUNCTION(BlueprintPure, Category = "Elevator")
	ELCOElevatorState GetElevatorState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Elevator")
	int32 GetCurrentFloorIndex() const { return CurrentFloorIndex; }

	UFUNCTION(BlueprintPure, Category = "Elevator")
	FName GetCurrentFloorID() const;

	// 층 ID로 Floors 인덱스 찾기 (없으면 INDEX_NONE)
	UFUNCTION(BlueprintPure, Category = "Elevator")
	int32 FindFloorIndex(FName FloorID) const;

	UFUNCTION(BlueprintPure, Category = "Elevator")
	const TArray<FLCOElevatorFloor>& GetFloors() const { return Floors; }

	// 해당 층 버튼에 표시할 문구 (전원 꺼짐 / 카드키 없음이면 전용 문구, 아니면 빈 문구 → 버튼 기본 문구)
	UFUNCTION(BlueprintPure, Category = "Elevator")
	FText GetPromptForFloor(FName FloorID, const AActor* Interactor) const;

	// ---------- 델리게이트 (승강장 등 외부용) ----------

	// 도착했을 때 (문이 열리기 시작하기 전)
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorArrived OnArrivedDelegate;

	// 시작 층에서 대기 상태로 초기화됐을 때 (BeginPlay). 해당 층 바깥 문을 연출 없이 열린 모습으로
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorFloorEvent OnInitializedDelegate;

	// FloorIndex 층에서 문이 열리기 시작할 때 (DoorDuration 동안)
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorFloorEvent OnDoorsOpeningDelegate;

	// FloorIndex 층에서 문이 닫히기 시작할 때 (DoorDuration 동안)
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorFloorEvent OnDoorsClosingDelegate;

protected:
	virtual void BeginPlay() override;

	// ---------- BP 연출 이벤트 ----------

	// 연출 없이 시작 층에 문이 열린 모습으로 맞춤 (BeginPlay). 다른 층의 바깥 문은 닫힌 모습으로 맞출 것
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnElevatorInitialized(int32 FloorIndex);

	// 문이 닫히기 시작할 때 (FloorIndex 층의 바깥 문 + Car 문). DoorDuration 동안 애니메이션
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnDoorsClosing(int32 FloorIndex);

	// 문이 열리기 시작할 때. DoorDuration 동안 애니메이션
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnDoorsOpening(int32 FloorIndex);

	// 다른 층 요청이 받아들여졌을 때 (문이 닫히기 직전). 안쪽 버튼 불 켜기 등 → OnArrived에서 끄기
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnFloorRequested(int32 FromFloorIndex, int32 ToFloorIndex, AActor* Requester);

	// Car가 움직이기 시작할 때 (모터 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnMoveStarted(int32 FromFloorIndex, int32 ToFloorIndex);

	// 목표 층에 도착했을 때 (도착 벨 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnArrived(int32 FloorIndex);

	// 요청이 거부됐을 때 (전원 꺼짐 / 카드키 없음)
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnRequestDenied(AActor* Requester, int32 FloorIndex);

	// ---------- 컴포넌트 ----------

	// 위아래로 움직이는 칸. 바닥 / 벽 / 안쪽 버튼을 이 아래에 붙일 것
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Elevator")
	TObjectPtr<USceneComponent> Car;

	// ---------- 설정값 ----------

	// 층 목록 (아래층부터). FloorID는 버튼 InputID와 같게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator")
	TArray<FLCOElevatorFloor> Floors;

	// 시작 층 (Floors 인덱스)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator", meta = (ClampMin = "0"))
	int32 StartFloorIndex = 0;

	// Car 이동 속도 (cm/s). 너무 빠르면 내려갈 때 플레이어가 바닥에서 떨어질 수 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator", meta = (ClampMin = "10.0", Units = "cm/s"))
	float MoveSpeed = 200.f;

	// 문이 열리거나 닫히는 데 걸리는 시간. BP 문 애니메이션 길이와 같게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator", meta = (ClampMin = "0.0", Units = "s"))
	float DoorDuration = 1.f;

	// 전원. 꺼져 있으면 버튼을 눌러도 움직이지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator")
	bool bIsPowered = true;

	// ---------- 상호작용 문구 ----------

	// 전원이 꺼져 있을 때
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator|Prompt")
	FText UnpoweredPrompt;

	// 카드키가 필요한 층인데 없을 때
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Elevator|Prompt")
	FText LockedFloorPrompt;

private:
	// 요청 가능 여부 (전원, 층 존재, 카드키). 실패 시 INDEX_NONE이 아닌 층 인덱스를 OutFloorIndex로 돌려줌
	bool CanRequestFloor(FName FloorID, const AActor* Requester, int32& OutFloorIndex) const;

	bool HasRequiredItem(const FLCOElevatorFloor& Floor, const AActor* Requester) const;

	void StartClosingDoors();
	void StartMoving();
	void Arrive();
	void FinishOpeningDoors();

	// Car를 해당 층 높이로 바로 이동 (X / Y는 유지)
	void SnapCarToFloor(int32 FloorIndex);

	ELCOElevatorState State = ELCOElevatorState::Idle;

	int32 CurrentFloorIndex = 0;
	int32 TargetFloorIndex = 0;

	FTimerHandle DoorTimerHandle;
};
