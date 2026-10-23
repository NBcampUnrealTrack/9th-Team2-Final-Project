#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOChasingManager.generated.h"

class ALCOHidingSpot;
class ALCOSigolYouth;
class UDecalComponent;
class UMaterialInterface;
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

#pragma region 초기화와 의심도 바인딩
public:
	ALCOChasingManager();

protected:
	virtual void BeginPlay() override;

private:
	// 플레이어의 SuspicionComponent를 찾아 OnSuspicionMax에 바인딩한다. 없으면 타이머로 재시도한다.
	void TryBindSuspicion();

	UFUNCTION()
	void HandleSuspicionMax();

	TWeakObjectPtr<USuspicionComponent> BoundSuspicion;
	FTimerHandle SuspicionBindTimer;
#pragma endregion

#pragma region 체이싱 시작과 종료
public:
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

	// 체이싱 시작/종료 알림 (연출, BGM, 화면 효과, HUD가 바인딩)
	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnChasingStarted OnChasingStarted;

	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnChasingEnded OnChasingEnded;

protected:
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

private:
	bool FindSpawnLocation(const FVector& PlayerLocation, FVector& OutLocation) const;
	void SpawnYouth(const FVector& Location);
	void CheckFoundYouth();

	TWeakObjectPtr<ALCOSigolYouth> SpawnedYouth;
	FTimerHandle FoundCheckTimer;
	FTimerHandle CooldownTimer;

	bool bIsChasing = false;

	// 플레이어가 겹쳐 있는 안전지대 볼륨 수
	int32 SafeZoneCount = 0;
#pragma endregion

#pragma region 옷장 은신과 끌어내기
public:
	// 은신처(옷장 등)에 숨을 때 은신 시스템이 호출한다. (HidingSpot은 은신처 액터)
	// 관리인에게 보이는 상태로 숨으면 실패(false)이고 체이싱은 계속된다.
	// 관리인의 시야 밖(벽에 가려졌거나 시야 거리·시야각 밖)에서 숨으면 체이싱을 종료하고 true를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "Chasing")
	bool TryEndChasingByHiding(AActor* HidingSpot);

private:
	// 플레이어가 옷장에 숨었을 때 호출된다. 관리인의 시야 밖이면 체이싱을 종료한다.
	UFUNCTION()
	void HandleHidingSpotEntered(ALCOHidingSpot* HidingSpot, AActor* Hider);

	// 들킨 채 숨은 경우, 추격 중인 관리인들이 은신처로 가서 끌어내게 한다.
	void StartExtractByCaretakers(ALCOHidingSpot* HidingSpot);

	// 관리인 중 한 명이라도 플레이어를 볼 수 있는 거리와 시야에 있는지 확인한다.
	bool IsPlayerSpottedByCaretaker(const AActor* HidingSpot) const;
#pragma endregion

#pragma region 잡기 연출
public:
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

	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnPlayerCaught OnPlayerCaught;

	UPROPERTY(BlueprintAssignable, Category = "Chasing")
	FOnCaughtSequenceFinished OnCaughtSequenceFinished;

protected:
	// 잡기 연출에서 이 시간(초) 안에 FinishCaughtSequence()가 호출되지 않으면 강제로 진행한다. (연결 누락 대비)
	UPROPERTY(EditAnywhere, Category = "Chasing")
	float CaughtSequenceTimeout = 5.f;

private:
	FTimerHandle CaughtTimeoutTimer;

	bool bCaughtSequenceActive = false;
	TWeakObjectPtr<AActor> CaughtPlayer;
#pragma endregion

#pragma region 시골청년 발견 후 흐름
protected:
	// 시골청년을 발견한 뒤 청년이 플레이어에게 다가가 대화를 시작하는 거리(cm)
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthTalkDistance = 120.f;

	// 청년이 플레이어에게 다가가는 최대 시간(초). 지나면 그 자리에서 대화를 시작한다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthApproachTimeout = 6.f;

	// 대화 UI가 연결되기 전 테스트용: 이 시간(초) 뒤 대화를 자동으로 끝낸다. 0이면 UI가 EndConversation()을 호출할 때까지 기다린다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthTestAutoEndTime = 3.f;

	// 대화가 끝난 뒤 청년이 플레이어 반대 방향으로 걸어가는 최대 거리(cm). 복도를 따라 이 거리까지 걷다가 사라진다. (별도의 퇴장 지점은 필요 없다)
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth", meta = (ClampMin = "300.0"))
	float YouthLeaveWalkDistance = 1500.f;

	// 퇴장을 시작한 뒤 이 시간(초)이 지나면 어디에 있든 사라진다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthLeaveTimeout = 10.f;

	// 플레이어 시야 밖이면서 이 거리(cm) 이상 떨어지면 사라진다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Youth")
	float YouthDespawnDistance = 800.f;

private:
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

	// 발견 후 플레이어와 대화하고 퇴장하는 중인 청년 (체이싱이 끝난 뒤에도 유지된다)
	TWeakObjectPtr<ALCOSigolYouth> EncounterYouth;
	FVector YouthLeaveDestination = FVector::ZeroVector;
	bool bHasYouthLeaveDestination = false;
	double YouthPhaseStartTime = 0.0;

	FTimerHandle YouthApproachTimer;
	FTimerHandle YouthAutoEndTimer;
	FTimerHandle YouthLeaveTimer;
#pragma endregion

#pragma region 길 안내 발자국
protected:
	// 체이싱 중 시골청년까지의 최단 경로를 바닥 발자국으로 안내한다. 발자국 위치는 체이싱이 시작될 때 한 번에 정해 월드에 고정하고, 플레이어는 보이는 범위만 바꾼다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	bool bShowPathGuide = true;

	// 왼발/오른발 발자국 데칼 머티리얼. 둘 다 비어 있으면 디버그 점으로 대신 표시한다. (에셋이 나오면 지정)
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	TObjectPtr<UMaterialInterface> FootprintMaterialLeft;

	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	TObjectPtr<UMaterialInterface> FootprintMaterialRight;

	// 발자국 텍스처가 진행 방향과 어긋날 때 보정하는 회전(도). 발끝이 진행 방향을 향하게 맞춘다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float FootprintYawOffset = 0.f;

	// 데칼 크기 (X: 투영 깊이, Y/Z: 절반 크기, cm)
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	FVector FootprintDecalSize = FVector(20.f, 18.f, 18.f);

	// 발자국 간격(cm). 청년 쪽 끝을 기준으로 이 간격마다 놓는다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float FootprintSpacing = 110.f;

	// 왼발/오른발을 경로 좌우로 벌리는 거리(cm)
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float FootprintSideOffset = 25.f;

	// 플레이어 앞쪽으로 경로를 따라 이 거리(cm)까지의 발자국만 보인다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float GuideRange = 1000.f;

	// 경로 이탈 확인과 보이는 범위를 갱신하는 주기(초)
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float GuideUpdateInterval = 0.5f;

	// 플레이어가 이 거리(cm) 안으로 가까이 가거나 지나친 발자국은 지운다. (한 번 지나친 발자국은 다시 나타나지 않는다)
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float FootprintPassDistance = 150.f;

	// 지나간 발자국을 지우는 확인 주기(초). 경로 계산 없이 거리만 확인하므로 짧게 해도 부담이 적다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float FootprintPassCheckInterval = 0.05f;

	// 코너에서 발자국을 벽(NavMesh 가장자리)에서 이 거리(cm) 이상 띄운다. 좌우로 벌어지는 폭(FootprintSideOffset)만큼은 더해서 띄운다. 0이면 사용하지 않는다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float FootprintWallClearance = 40.f;

	// 안내선을 벽에서 떨어지려고 옆으로 옮기는 최대 거리(cm). 코너에서 과하게 바깥쪽으로 밀려나는 것을 제한한다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float FootprintMaxWallPush = 120.f;

	// 벽에서 떨어지려고 옮긴 정도가 코너 앞뒤로 완만하게 이어지는 거리(cm). 클수록 코너 주변의 안내선이 넓고 완만하게 휜다. 0이면 이어 붙이지 않는다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide", meta = (ClampMin = "0.0"))
	float FootprintWallBlendDistance = 250.f;

	// 안내선을 촘촘하게 나누는 간격(cm). 작을수록 선이 매끄럽지만 경로를 구할 때 한 번 드는 계산이 늘어난다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide", meta = (ClampMin = "20.0"))
	float GuideLineStep = 50.f;

	// 안내선을 부드럽게 다듬는 횟수. 클수록 코너가 둥글어진다. 0이면 다듬지 않는다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide", meta = (ClampMin = "0", ClampMax = "10"))
	int32 GuideLineSmoothPasses = 3;

	// 플레이어가 안내 경로에서 이 거리(cm) 넘게 벗어나면 다른 방향으로 간 것으로 본다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float GuideDeviationDistance = 400.f;

	// 다른 방향으로 간 채 이 시간(초)이 지나면 기존 발자국을 지우고 지금 위치에서 다시 그린다.
	UPROPERTY(EditAnywhere, Category = "Chasing|Guide")
	float GuideRerouteDelay = 4.f;

	// 안내선을 화면에 그린다. (테스트용) 빨강: 엔진이 구한 원래 최단 경로, 초록: 벽 거리를 반영해 만든 최종 안내선, 노랑/주황 구: 왼발/오른발 위치, 회색 구: 지나간 발자국
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chasing|Guide")
	bool bDrawGuideLine = false;

private:
	// 경로 이탈을 확인하고(필요하면 다시 그림) 지나간 발자국을 지운 뒤 보이는 발자국을 갱신한다.
	void UpdatePathGuide();
	void HidePathGuide();
	void ResetPathGuide();

	// 플레이어에서 청년까지의 최단 경로를 구해 발자국 위치를 모두 정한다. 길이 막혀 청년까지 닿지 못하면(부분 경로) false를 돌려주고 아무것도 만들지 않는다.
	bool BuildGuideRoute(const FVector& StartLocation, const FVector& TargetLocation);

	// 플레이어에게 가장 가까운 안내 경로 위의 지점까지의 경로상 거리(OutProgress)와, 경로에서 떨어진 거리(OutDistanceToRoute)를 구한다.
	bool GetPlayerRouteProgress(const FVector& PlayerLocation, float& OutProgress, float& OutDistanceToRoute) const;

	// 플레이어가 지나간 발자국에 표시한다. 바뀐 것이 있으면 true.
	bool MarkPassedFootprints();

	// 짧은 주기로 지나간 발자국만 확인한다. (경로 계산 없음)
	void CheckPassedFootprints();

	// 보일 발자국을 골라 데칼(또는 디버그 표시)에 반영한다.
	void RefreshFootprintDecals();

	// 안내선을 디버그로 그린다. (bDrawGuideLine, 패키징 빌드 제외)
	void DrawGuideLineDebug() const;

	FTimerHandle PathGuideTimer;
	FTimerHandle PassCheckTimer;

	// 발자국 데칼을 재사용한다. (사용하지 않는 것은 숨김)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> FootprintPool;

	// 안내 경로를 따라 월드에 고정된 발자국. 위치는 경로를 구할 때 한 번에 정해지고 이후 바뀌지 않는다.
	struct FPlacedFootprint
	{
		FVector Location = FVector::ZeroVector;
		float Yaw = 0.f;
		bool bLeft = true;

		// 안내 경로의 시작(플레이어 쪽)에서 이 발자국까지의 경로상 거리
		float RouteDistance = 0.f;

		// 플레이어가 지나간 발자국. 한 번 지나가면 다시 보이지 않는다.
		bool bPassed = false;
	};
	TArray<FPlacedFootprint> PlacedFootprints;

	// 안내 경로(체이싱이 시작될 때 구하고, 크게 벗어나면 다시 구한다)와 누적 거리
	TArray<FVector> RoutePoints;
	TArray<float> RouteCumulative;

	// 디버그 표시용: 벽 거리를 반영하기 전의 촘촘하게 나눈 원래 최단 경로
	TArray<FVector> DebugRawLine;
	bool bHasRoute = false;

	// 경로에서 벗어나기 시작한 시각. 벗어나 있지 않으면 음수.
	double OffRouteStartTime = -1.0;
#pragma endregion
};
