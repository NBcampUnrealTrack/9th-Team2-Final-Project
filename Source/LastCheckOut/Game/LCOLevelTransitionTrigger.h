#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Save/LCOSaveableActor.h"
#include "LCOLevelTransitionTrigger.generated.h"

class UBoxComponent;

// 맵 이동 트리거: 플레이어가 볼륨에 닿으면 LCOGameMode::TravelToLevel로 다음 맵으로 이동
// - 맵에 배치 후 디테일 패널에서 TargetLevel(목적지 맵), TargetPlayerStartTag(도착 위치) 지정
// - bTriggerEnabled를 끄면 닿아도 이동하지 않음 (스토리 진행 전 잠금 등)
UCLASS()
class LASTCHECKOUT_API ALCOLevelTransitionTrigger : public AActor, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOLevelTransitionTrigger();

	// 트리거 켜기·끄기 (스토리 진행에 따라 출구를 여는 용도)
	UFUNCTION(BlueprintCallable, Category = "LCO|Travel")
	void SetTriggerEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "LCO|Travel")
	bool IsTriggerEnabled() const { return bTriggerEnabled; }

	// ILCOSaveableActor: 켜짐/꺼짐 상태 저장·복원 (맵 이동 / 세이브)
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

protected:
	virtual void BeginPlay() override;

	// 이동할 맵
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|Travel")
	TSoftObjectPtr<UWorld> TargetLevel;

	// 도착할 PlayerStart의 태그 (목적지 맵 PlayerStart의 Player Start Tag와 같게). 비우면 기본 PlayerStart
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|Travel")
	FName TargetPlayerStartTag;

	// 이동할 때 완료할 목표 (현재 목표와 같을 때만 완료됨). 비우면 무시
	// 예: 별관 이동 트리거에 C2_FollowCaretaker → 관리인 따라가기 끝
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|Travel")
	FName ObjectiveToComplete;

	// 챕터 클리어 조건이 되면 이동 대신 챕터 클리어 (현재는 2막: K11). 조건이 안 되면 평소처럼 이동
	// 예: 별관 출구 트리거
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|Travel")
	bool bCompleteChapterIfReady = false;

	// 꺼져 있으면 닿아도 이동하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|Travel")
	bool bTriggerEnabled = true;

	// 플레이어 감지 볼륨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LCO|Travel")
	TObjectPtr<UBoxComponent> TriggerBox;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 이동 중복 방지 (OpenLevel 전에 겹침 이벤트가 여러 번 와도 1번만 이동)
	bool bHasTriggered = false;
};
