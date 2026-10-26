#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "LCOElevatorLanding.generated.h"

class ALCOElevator;

// 엘리베이터 승강장 (층마다 하나씩 배치)
// - 바깥 문 + 호출 버튼(LCOInteract, Linked Target은 비워둠 → 이 액터로 전달)을 BP 자식에서 구성
// - 레벨 인스턴스에서 Elevator(엘리베이터 액터)와 FloorID(이 승강장의 층)만 지정
// - 호출 버튼을 누르면 Elevator->RequestFloor(FloorID)로 엘리베이터를 이 층으로 부름
// - 엘리베이터가 이 층에서 문을 여닫을 때 바깥 문 BP 이벤트가 호출됨 (다른 층 이벤트는 무시)
// - 저장할 상태 없음 (바깥 문은 엘리베이터 상태를 따라감)
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOElevatorLanding : public AActor, public ILCOInteractionReceiver
{
	GENERATED_BODY()

public:
	ALCOElevatorLanding();

	// ILCOInteractionReceiver
	// 호출 버튼 → 엘리베이터를 이 층으로 부름
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 엘리베이터가 없거나 대기 상태가 아니면 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 전원 꺼짐 / 카드키 없음 문구는 엘리베이터 설정을 따름
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	UFUNCTION(BlueprintPure, Category = "Elevator")
	ALCOElevator* GetElevator() const { return Elevator; }

	UFUNCTION(BlueprintPure, Category = "Elevator")
	FName GetFloorID() const { return FloorID; }

	// 에디터 드롭다운용. 연결된 엘리베이터의 FloorID 목록
	UFUNCTION()
	TArray<FName> GetFloorIDOptions() const;

protected:
	virtual void BeginPlay() override;

	// ---------- BP 연출 이벤트 ----------

	// 연출 없이 바깥 문 모습만 맞춤 (bDoorsOpen: 엘리베이터가 이 층에서 대기 중이면 true)
	// Timeline을 쓰는 BP는 Set New Time(열림 1 / 닫힘 0)으로 위치도 같이 맞출 것
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnLandingInitialized(bool bDoorsOpen);

	// 호출 버튼으로 엘리베이터를 불렀을 때 (다른 층에 있어서 이동이 시작될 때만). 호출 버튼 불 켜기 등 → OnLandingDoorsOpening에서 끄기
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnLandingCalled(ULCOInteractComponent* CallButton);

	// 이 층에서 바깥 문이 열리기 시작할 때 (엘리베이터의 DoorDuration 동안)
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnLandingDoorsOpening();

	// 이 층에서 바깥 문이 닫히기 시작할 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Elevator")
	void OnLandingDoorsClosing();

	// ---------- 설정값 (레벨 인스턴스에서 지정) ----------

	// 연결할 엘리베이터
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Elevator")
	TObjectPtr<ALCOElevator> Elevator;

	// 이 승강장의 층 (엘리베이터 Floors의 FloorID 중 선택)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Elevator", meta = (GetOptions = "GetFloorIDOptions"))
	FName FloorID;

private:
	UFUNCTION()
	void HandleElevatorInitialized(ALCOElevator* InElevator, int32 FloorIndex);

	UFUNCTION()
	void HandleDoorsOpening(ALCOElevator* InElevator, int32 FloorIndex);

	UFUNCTION()
	void HandleDoorsClosing(ALCOElevator* InElevator, int32 FloorIndex);

	// 이 승강장의 Floors 인덱스 (BeginPlay에서 FloorID로 찾음)
	int32 MyFloorIndex = INDEX_NONE;
};
