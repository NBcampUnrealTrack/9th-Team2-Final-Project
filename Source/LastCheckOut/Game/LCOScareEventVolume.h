#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Save/LCOSaveableActor.h"
#include "LCOScareEventVolume.generated.h"

class UBoxComponent;
class ALCOScareDoor;

// 연출 종류
UENUM(BlueprintType)
enum class ELCOScareType : uint8
{
	// 닫힌 문이 살짝 열렸다가 쾅 닫힘
	PeekAndSlam		UMETA(DisplayName = "살짝 열렸다 닫힘"),

	// 살짝 열려 있던 문(bStartAjar)이 쾅 닫힘 (틈으로 엿보던 NPC 연출 등)
	SlamFromAjar	UMETA(DisplayName = "열린 문 쾅 닫힘")
};

// 공포 연출 볼륨: 플레이어가 들어오면 StartDelay 후 지정한 문들에 연출 실행
// - 맵에 배치 후 디테일 패널의 TargetDoors에 연출할 LCOScareDoor를 지정, ScareType으로 연출 선택
// - ActorsToHideAfter: 문이 닫힌 뒤 숨길 액터 (문 틈으로 엿보던 NPC 등)
// - 한 번만 발생. 발생 여부는 맵 이동 / 세이브 시 LCOWorldStateSubsystem이 저장·복원 (ILCOSaveableActor)
// - PlayerStart를 볼륨 안에 두지 말 것 (시작하자마자 발생함)
UCLASS()
class LASTCHECKOUT_API ALCOScareEventVolume : public AActor, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOScareEventVolume();

	UFUNCTION(BlueprintPure, Category = "LCO|Scare")
	bool HasTriggered() const { return bHasTriggered; }

	// ILCOSaveableActor: 발생 여부 저장·복원 (bIsEnabled = 아직 발생 안 함)
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

protected:
	virtual void BeginPlay() override;

	// 연출이 시작될 때 (StartDelay 이후). 조명 깜빡임 등 추가 연출을 BP에서 연결 (비워둬도 됨)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Scare")
	void OnScareTriggered();

	// 연출 종류
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare")
	ELCOScareType ScareType = ELCOScareType::PeekAndSlam;

	// 연출할 문들
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|Scare")
	TArray<TObjectPtr<ALCOScareDoor>> TargetDoors;

	// 문이 닫힌 뒤 숨길 액터 (문 틈으로 엿보던 NPC 등)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|Scare")
	TArray<TObjectPtr<AActor>> ActorsToHideAfter;

	// 볼륨에 들어간 뒤 연출 시작까지 기다리는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Scare", meta = (ClampMin = "0.0", Units = "s"))
	float StartDelay = 0.5f;

	// 플레이어 감지 볼륨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LCO|Scare")
	TObjectPtr<UBoxComponent> TriggerBox;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// StartDelay 후 호출: 문 연출 실행
	void PlayScareOnDoors();

	// 문이 닫혔을 때: ActorsToHideAfter 숨김
	void HandleDoorSlammed(ALCOScareDoor* Door);

	void HideActorsAfter();

	// 이미 발생했는지 (한 번만 발생)
	bool bHasTriggered = false;

	FTimerHandle StartDelayTimer;
};
