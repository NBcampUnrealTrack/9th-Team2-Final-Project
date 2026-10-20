#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Save/LCOSaveableActor.h"
#include "LCOCorridorSwapVolume.generated.h"

class UBoxComponent;
class UArrowComponent;

// 복도 기믹: 뒤돌아볼 때마다 복도와 막다른 길이 뒤바뀌는 연출
// 1) 플레이어가 볼륨 안에서 막다른 길(화살표 방향)을 바라봄
// 2) 고개를 돌리는 중 앞·뒤 어느 쪽도 화면에 안 보이는 순간
//    -> 앞의 막다른 벽(ActorsToHide)은 사라지고, 걸어온 복도에 벽(ActorsToShow)이 생김
// 3) 뒤를 보면 걸어온 길이 막혀 있고, 다시 돌아보면 막다른 길이 복도로 열려 있음
// - 맵에 배치 후 화살표(빨간색)가 막다른 길 쪽을 향하게 회전
// - 한 번만 동작하며, 바뀐 상태는 맵 이동 / 세이브 시 유지
UCLASS()
class LASTCHECKOUT_API ALCOCorridorSwapVolume : public AActor, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOCorridorSwapVolume();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "LCO|CorridorSwap")
	bool HasSwapped() const { return bHasSwapped; }

	// ILCOSaveableActor: 바뀌었는지 저장·복원 (bIsEnabled = 아직 안 바뀜)
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

protected:
	virtual void BeginPlay() override;

	// 바뀌는 순간 호출 (BP에서 사운드 등 연출 추가용)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|CorridorSwap")
	void OnCorridorSwapped();

	// 바뀔 때 사라질 액터 (앞의 막다른 벽)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|CorridorSwap")
	TArray<TObjectPtr<AActor>> ActorsToHide;

	// 바뀔 때 나타날 액터 (걸어온 복도를 막는 벽). 시작할 때 자동으로 숨김
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|CorridorSwap")
	TArray<TObjectPtr<AActor>> ActorsToShow;

	// 막다른 길을 "봤다"고 판단하는 각도 (화살표 방향 기준 좌우 몇 도 안쪽을 바라봐야 하는지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|CorridorSwap", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float DeadEndLookAngle = 30.f;

	// 화면 시야(FOV) 절반에 더하는 여유 각도. 클수록 더 확실히 안 보일 때 바뀜
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|CorridorSwap", meta = (ClampMin = "0.0", ClampMax = "40.0"))
	float ViewMargin = 15.f;

	// 플레이어 감지 볼륨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LCO|CorridorSwap")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 막다른 길 방향 표시 (에디터 전용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LCO|CorridorSwap")
	TObjectPtr<UArrowComponent> DeadEndArrow;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 카메라 방향과 화살표 방향 사이 각도 (좌우만, 0 = 막다른 길 정면 / 180 = 걸어온 방향)
	bool GetCameraYawAngle(float& OutAngle, float& OutHalfFOV) const;

	// 앞 벽 숨기기 + 뒤 벽 보이기
	void ApplySwappedState();

	static void SetActorsVisible(const TArray<TObjectPtr<AActor>>& Actors, bool bVisible);

	bool bPlayerInside = false;
	bool bHasSeenDeadEnd = false;
	bool bHasSwapped = false;
};
