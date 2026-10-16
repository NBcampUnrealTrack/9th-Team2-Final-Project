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

// 관리인에게 잡혔을 때. 암전, 입력 잠금, 증거 몰수 등 연출이 바인딩하고, 끝나면 FinishCaughtSequence()를 호출한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerCaught, AActor*, Player);

// 잡기 처리가 모두 끝나 플레이어와 관리인이 초기화된 뒤. 암전 해제, 입력 복구 등이 바인딩한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCaughtSequenceFinished, AActor*, Player);

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

	// 잡기 연출을 시작한다. (관리인 컨트롤러가 호출) 관리인을 멈추고 의심도를 고정한 뒤 OnPlayerCaught를 알린다.
	// OnPlayerCaught에 바인딩된 연출이 없으면 바로 FinishCaughtSequence()를 실행한다.
	// 이미 진행 중이어도 true를 반환한다.
	bool BeginCaughtSequence(AActor* CaughtPlayer);

	// 잡기 연출이 끝나는 시점(예: 화면이 완전히 어두워졌을 때)에 호출한다.
	// 플레이어를 시작 지점으로 옮기고, 관리인을 초기화하고, 체이싱을 종료한 뒤 OnCaughtSequenceFinished를 알린다.
	UFUNCTION(BlueprintCallable, Category = "Chasing")
	void FinishCaughtSequence();

	UFUNCTION(BlueprintPure, Category = "Chasing")
	bool IsCaughtSequenceActive() const { return bCaughtSequenceActive; }

	// 플레이어를 PlayerStart 위치로 옮긴다.
	static void TeleportPlayerToStart(UObject* WorldContextObject, APawn* PlayerPawn);

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

	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnPlayerCaught OnPlayerCaught;

	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnCaughtSequenceFinished OnCaughtSequenceFinished;

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
	float MinSpawnPathLength = 500.f;

	// 시골청년을 발견한 것으로 보는 거리(cm)
	UPROPERTY(EditAnywhere, Category = "Chasing")
	float FoundDistance = 500.f;

	// true면 사이에 장애물이 없을 때만 발견으로 인정한다.
	UPROPERTY(EditAnywhere, Category = "Chasing")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, Category = "Chasing")
	float FoundCheckInterval = 0.2f;

	// 체이싱이 끝난 직후 이 시간(초) 동안 의심도를 고정해 바로 다시 추격되지 않게 한다.
	UPROPERTY(EditAnywhere, Category = "Chasing")
	float PostChaseCooldown = 5.f;

	// 잡기 연출에서 이 시간(초) 안에 FinishCaughtSequence()가 호출되지 않으면 강제로 진행한다. (연결 누락 대비)
	UPROPERTY(EditAnywhere, Category = "Chasing")
	float CaughtSequenceTimeout = 5.f;

	// 시골청년을 발견한 뒤 청년이 플레이어에게 다가가 대화를 시작하는 거리(cm)
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthTalkDistance = 120.f;

	// 청년이 플레이어에게 다가가는 최대 시간(초). 지나면 그 자리에서 대화를 시작한다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthApproachTimeout = 6.f;

	// 대화 UI가 연결되기 전 테스트용: 이 시간(초) 뒤 대화를 자동으로 끝낸다. 0이면 UI가 EndConversation()을 호출할 때까지 기다린다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthTestAutoEndTime = 3.f;

	// 대화가 끝난 뒤 청년이 퇴장할 지점. 이 중 플레이어에게서 가장 멀고 도달 가능한 곳으로 간다.
	UPROPERTY(EditInstanceOnly, Category = "Chasing|Youth")
	TArray<TObjectPtr<ATargetPoint>> LeavePoints;

	// 퇴장을 시작한 뒤 이 시간(초)이 지나면 어디에 있든 사라진다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthLeaveTimeout = 10.f;

	// 플레이어 시야 밖이면서 이 거리(cm) 이상 떨어지면 사라진다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthDespawnDistance = 800.f;

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

	// 시골청년 발견 후 흐름: 청년이 다가옴 -> 대화 -> 퇴장 지점으로 걸어가 사라짐
	void BeginYouthEncounter();
	void CheckYouthApproach();
	void StartYouthTalk();
	void StartYouthLeave();
	void CheckYouthLeave();
	void CleanupYouth();

	UFUNCTION()
	void HandleYouthConversationEnded();

	// 플레이어 카메라 기준으로 화면 밖이거나 벽에 가려 보이지 않는지 확인한다.
	bool IsOutOfPlayerView(const AActor* Target) const;

	// 관리인 중 한 명이라도 플레이어를 볼 수 있는 거리와 시야에 있는지 확인한다.
	bool IsPlayerSpottedByCaretaker(const AActor* HidingSpot) const;

	TWeakObjectPtr<USuspicionComponent> BoundSuspicion;
	TWeakObjectPtr<ALCOSigolYouth> SpawnedYouth;

	// 발견 후 플레이어와 대화하고 퇴장하는 중인 청년 (체이싱이 끝난 뒤에도 유지된다)
	TWeakObjectPtr<ALCOSigolYouth> EncounterYouth;
	TWeakObjectPtr<ATargetPoint> YouthLeaveTarget;
	double YouthPhaseStartTime = 0.0;

	FTimerHandle SuspicionBindTimer;
	FTimerHandle FoundCheckTimer;
	FTimerHandle CooldownTimer;
	FTimerHandle CaughtTimeoutTimer;
	FTimerHandle YouthApproachTimer;
	FTimerHandle YouthAutoEndTimer;
	FTimerHandle YouthLeaveTimer;

	bool bCaughtSequenceActive = false;
	TWeakObjectPtr<AActor> CaughtPlayer;

	bool bIsChasing = false;

	// 플레이어가 겹쳐 있는 안전지대 볼륨 수
	int32 SafeZoneCount = 0;
};
