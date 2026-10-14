#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOChasingManager.generated.h"

class ALCOHidingSpot;
class ALCOSigolYouth;
class ATargetPoint;
class USuspicionComponent;

// 체이싱이 끝난 이유
UENUM(BlueprintType)
enum class ELCOChasingEndReason : uint8
{
	Caught,     // 관리인에게 잡힘 (실패)
	LobbyEntry, // 안전지대(로비) 진입
	FoundYouth, // 시골청년 발견 (성공)
	Hidden      // 관리인의 시야 밖에서 은신처에 숨음 (성공)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChasingStarted, AActor*, SigolYouth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChasingEnded, ELCOChasingEndReason, Reason);

// 의심도가 100에 도달하면 시작되는 체이싱을 관리한다. (레벨에 하나 배치)
// 시작: 플레이어에게서 가장 먼 위치에 시골청년을 스폰한다.
// 종료: 잡힘 / 로비 진입 / 시골청년 발견 중 하나를 만족하면 즉시 종료한다.
UCLASS()
class LASTCHECKOUT_API ALCOChasingManager : public AActor
{
	GENERATED_BODY()

public:
	ALCOChasingManager();

	// 체이싱을 시작한다. 이미 진행 중이거나 안전지대 안이면 시작하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Chasing")
	void StartChasing();

	// 체이싱을 종료한다. 시골청년 제거, 관리인 추격 중단, 의심도 초기화를 처리한다.
	UFUNCTION(BlueprintCallable, Category = "Chasing")
	void EndChasing(ELCOChasingEndReason Reason);

	UFUNCTION(BlueprintPure, Category = "Chasing")
	bool IsChasing() const { return bIsChasing; }

	UFUNCTION(BlueprintPure, Category = "Chasing")
	bool IsPlayerInSafeZone() const { return SafeZoneCount > 0; }

	// 안전지대 볼륨이 호출한다.
	void NotifyPlayerEnteredSafeZone();
	void NotifyPlayerExitedSafeZone();

	// 은신처(옷장 등)에 숨을 때 은신 시스템이 호출한다. (HidingSpot은 은신처 액터)
	// 관리인에게 보이는 상태로 숨으면 실패(false)이고 체이싱은 계속된다.
	// 관리인의 시야 밖(벽에 가려졌거나 시야 거리·시야각 밖)에서 숨으면 체이싱을 종료하고 true를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Chasing")
	bool TryEndChasingByHiding(AActor* HidingSpot);

	// 체이싱 시작/종료 알림 (연출, BGM, 화면 효과, HUD가 바인딩)
	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnChasingStarted OnChasingStarted;

	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnChasingEnded OnChasingEnded;

protected:
	virtual void BeginPlay() override;

	// 스폰할 시골청년 클래스
	UPROPERTY(EditAnywhere, Category = "Chasing")
	TSubclassOf<ALCOSigolYouth> SigolYouthClass;

	// 스폰 후보 위치. 이 중 플레이어에게서 NavMesh 경로가 가장 먼 곳을 고른다.
	UPROPERTY(EditInstanceOnly, Category = "Chasing")
	TArray<TObjectPtr<ATargetPoint>> SpawnCandidates;

	// 후보 중 조건을 만족하는 곳이 없을 때 쓰는 대체 위치
	UPROPERTY(EditInstanceOnly, Category = "Chasing")
	TArray<TObjectPtr<ATargetPoint>> FallbackPoints;

	// 이 경로 길이(cm)보다 가까운 후보는 제외한다.
	UPROPERTY(EditAnywhere, Category = "Chasing")
	float MinSpawnPathLength = 3000.f;

	// 시골청년을 발견한 것으로 보는 거리(cm)
	UPROPERTY(EditAnywhere, Category = "Chasing")
	float FoundDistance = 300.f;

	// true면 사이에 장애물이 없을 때만 발견으로 인정한다.
	UPROPERTY(EditAnywhere, Category = "Chasing")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, Category = "Chasing")
	float FoundCheckInterval = 0.2f;

	// 체이싱이 끝난 직후 이 시간(초) 동안 의심도를 고정해 바로 다시 추격되지 않게 한다.
	UPROPERTY(EditAnywhere, Category = "Chasing")
	float PostChaseCooldown = 5.f;

private:
	// 플레이어의 SuspicionComponent를 찾아 OnSuspicionMax에 바인딩한다. 없으면 타이머로 재시도한다.
	void TryBindSuspicion();

	UFUNCTION()
	void HandleSuspicionMax();

	// 플레이어가 옷장에 숨었을 때 호출된다. 관리인의 시야 밖이면 체이싱을 종료한다.
	UFUNCTION()
	void HandleHidingSpotEntered(ALCOHidingSpot* HidingSpot, AActor* Hider);

	// 들킨 채 숨은 경우, 추격 중인 관리인들이 은신처로 가서 끌어내게 한다.
	void StartExtractByCaretakers(ALCOHidingSpot* HidingSpot);

	bool FindSpawnLocation(const FVector& PlayerLocation, FVector& OutLocation) const;
	void SpawnYouth(const FVector& Location);
	void CheckFoundYouth();

	// 관리인 중 한 명이라도 플레이어를 볼 수 있는 거리와 시야에 있는지 확인한다.
	bool IsPlayerSpottedByCaretaker(const AActor* HidingSpot) const;

	TWeakObjectPtr<USuspicionComponent> BoundSuspicion;
	TWeakObjectPtr<ALCOSigolYouth> SpawnedYouth;

	FTimerHandle SuspicionBindTimer;
	FTimerHandle FoundCheckTimer;
	FTimerHandle CooldownTimer;

	bool bIsChasing = false;

	// 플레이어가 겹쳐 있는 안전지대 볼륨 수
	int32 SafeZoneCount = 0;
};
