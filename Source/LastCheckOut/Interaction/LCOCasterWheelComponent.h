#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOCasterWheelComponent.generated.h"

// 바퀴 하나의 설정
USTRUCT(BlueprintType)
struct FLCOCasterWheel
{
	GENERATED_BODY()

	// 이동 방향으로 돌아가는 축(세로축, Yaw)의 Scene 컴포넌트 이름. 비우면 방향은 바뀌지 않음 (고정 바퀴)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel")
	FName SwivelComponentName;

	// 굴러가는 축(바퀴 축 중심)의 Scene 컴포넌트 이름. 비우면 굴러가지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel")
	FName SpinComponentName;

	// 바퀴 반지름 (cm). 굴러가는 속도 계산에 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheel", meta = (ClampMin = "0.1", Units = "cm"))
	float Radius = 5.f;

	// 런타임 상태
	TWeakObjectPtr<USceneComponent> Swivel;
	TWeakObjectPtr<USceneComponent> Spin;
	FQuat SwivelRestRotation = FQuat::Identity;
	FQuat SpinRestRotation = FQuat::Identity;
	float SwivelYaw = 0.f;
	float SpinAngle = 0.f;
};

// 카트, 휠체어 등의 바퀴를 소유 액터의 실제 이동에 맞춰 움직이는 컴포넌트
// - 매 프레임 액터 위치 변화로 속도를 계산 (NPC에 붙여서 끌든, 따로 이동하든 상관없음)
// - 캐스터 바퀴(Swivel 지정): 이동 방향으로 서서히 돌아감 / 고정 바퀴(Swivel 비움): 액터 앞 방향 고정
// - 모든 바퀴: 이동 거리만큼 굴러감 (Spin)
// - 바퀴 메시는 원래 자세 그대로 두고, 그 위의 Scene 컴포넌트만 회전 (메시의 원래 회전값을 덮어쓰지 않음)
//   구성 예: Caster_FL (Scene, 세로축) → WheelSpin_FL (Scene, 바퀴 축 중심) → Wheel_FL (Static Mesh)
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOCasterWheelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOCasterWheelComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 현재 이동 속도 (cm/s, 액터 기준 로컬 방향)
	UFUNCTION(BlueprintPure, Category = "Wheel")
	FVector GetLocalVelocity() const { return LocalVelocity; }

	// 바퀴 방향 / 회전을 처음 상태로 되돌림 (순간이동 후 등)
	UFUNCTION(BlueprintCallable, Category = "Wheel")
	void ResetWheels();

protected:
	virtual void BeginPlay() override;

	// 바퀴 목록 (앞바퀴는 Swivel + Spin, 뒷바퀴는 Spin만 지정하는 식)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	TArray<FLCOCasterWheel> Wheels;

	// 캐스터 바퀴가 이동 방향으로 돌아가는 속도 (도/초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "0.0", Units = "deg/s"))
	float SwivelSpeed = 360.f;

	// 이보다 느리면 바퀴 방향을 바꾸지 않음 (멈춰 있을 때 흔들림 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinSwivelSpeed = 5.f;

	// 한 프레임 이동이 이보다 크면 순간이동으로 보고 무시 (위치 복원, 맵 이동 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "0.0", Units = "cm"))
	float TeleportDistance = 300.f;

	// 굴러가는 회전축 (Spin 컴포넌트 기준). 바퀴 축이 Y 방향이면 (0, 1, 0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	FVector SpinAxis = FVector(0.f, 1.f, 0.f);

private:
	USceneComponent* FindSceneComponent(FName ComponentName) const;
	void ApplyWheel(FLCOCasterWheel& Wheel) const;

	FVector LastLocation = FVector::ZeroVector;
	FVector LocalVelocity = FVector::ZeroVector;
	bool bHasLastLocation = false;
};
