#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOScareDoor.generated.h"

class UStaticMeshComponent;
class ALCOScareDoor;

// 문이 완전히 닫혔을 때 알림 (LCOScareEventVolume이 받아서 숨길 액터 처리)
DECLARE_MULTICAST_DELEGATE_OneParam(FLCOScareDoorSlammed, ALCOScareDoor*);

// 연출 전용 문 (들어갈 수 없는 객실 문 등)
// - 상호작용 / 열쇠 없음. 보통 LCOScareEventVolume이 호출
// - PlayScare(): 닫힌 문이 살짝 열림(OpenTime) → 잠깐 멈춤(HoldTime) → 빠르게 쾅 닫힘(CloseTime)
// - PlaySlam(): 살짝 열려 있던 문(bStartAjar)이 바로 쾅 닫힘(CloseTime)
// - BP 자식에서 DoorMesh에 문 메시를 지정해서 사용
// - 경첩(Hinge) 위치 = 문이 회전하는 축. 문 메시를 Hinge 기준으로 옆으로 옮겨서 배치할 것
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOScareDoor : public AActor
{
	GENERATED_BODY()

public:
	ALCOScareDoor();

	virtual void Tick(float DeltaSeconds) override;

	// 살짝 열렸다가 쾅 닫히는 연출 시작. 이미 연출 중이면 무시
	UFUNCTION(BlueprintCallable, Category = "LCO|Scare")
	void PlayScare();

	// 지금 열린 상태에서 바로 쾅 닫힘. 이미 연출 중이면 무시
	UFUNCTION(BlueprintCallable, Category = "LCO|Scare")
	void PlaySlam();

	// 연출 없이 바로 닫힌 상태로 (세이브 불러오기용)
	UFUNCTION(BlueprintCallable, Category = "LCO|Scare")
	void CloseImmediately();

	UFUNCTION(BlueprintPure, Category = "LCO|Scare")
	bool IsPlayingScare() const { return Phase != EScarePhase::Idle; }

	// 시작할 때 살짝 열린 상태인지
	bool IsStartAjar() const { return bStartAjar; }

	// 문이 완전히 닫힌 순간 알림 (C++용)
	FLCOScareDoorSlammed OnDoorSlammed;

protected:
	virtual void BeginPlay() override;

	// BP 연출 이벤트 (사운드 등)
	
	// 문이 살짝 열리기 시작할 때 (사운드)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Scare")
	void OnPeekStarted();

	// 문이 완전히 닫힌 순간 (사운드, 카메라 흔들림 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Scare")
	void OnSlammed();

	// 컴포넌트

	// 문이 회전하는 축 (경첩)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LCO|Scare")
	TObjectPtr<USceneComponent> Hinge;

	// 문 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LCO|Scare")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	// 설정값 (BP 기본값 / 레벨 인스턴스에서 변경 가능)

	// 체크하면 게임 시작 시 PeekAngle만큼 열린 상태 (틈으로 엿보는 연출용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare")
	bool bStartAjar = false;

	// 살짝 열리는 각도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float PeekAngle = 15.f;

	// 열리는 데 걸리는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare", meta = (ClampMin = "0.01", Units = "s"))
	float OpenTime = 0.6f;

	// 열린 채로 멈춰 있는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare", meta = (ClampMin = "0.0", Units = "s"))
	float HoldTime = 0.4f;

	// 닫히는 데 걸리는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare", meta = (ClampMin = "0.01", Units = "s"))
	float CloseTime = 0.12f;

	// 문이 반대쪽으로 열리면 체크
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare")
	bool bReverseDirection = false;

private:
	enum class EScarePhase : uint8
	{
		Idle,
		Opening,
		Holding,
		Closing
	};

	// 단계 전환 (경과 시간 초기화)
	void SetPhase(EScarePhase NewPhase);

	// 경첩 회전 적용 (0 = 닫힘, 1 = PeekAngle만큼 열림)
	void ApplyOpenAlpha(float Alpha);

	EScarePhase Phase = EScarePhase::Idle;
	float PhaseElapsed = 0.f;

	// 지금 열린 정도 (닫히기 시작할 때 이 값에서 0으로)
	float CurrentAlpha = 0.f;
	float CloseStartAlpha = 1.f;

	// 시작 시 경첩 회전 (닫힌 상태 기준값)
	FRotator ClosedHingeRotation = FRotator::ZeroRotator;
};
