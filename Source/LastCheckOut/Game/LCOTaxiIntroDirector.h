#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/LCOLocationCaptionWidget.h"
#include "LCOTaxiIntroDirector.generated.h"

class USoundBase;
class UAudioComponent;

// 게임 인트로 (택시 맵) 진행 담당 (Taxi 맵에 1개 배치)
// - 로딩 화면이 사라진 뒤 StartDelay 뒤에 택시 기사 대화를 자동으로 시작 (로딩이 없으면 맵 시작 기준)
// - 대화가 정상 완료되면 NextLevel의 NextPlayerStartTag 위치로 이동 (도착 후 자동 저장)
// - 택시 안에서는 플레이어 이동을 막음 (bLockPlayerLook이면 시점 회전도)
// - 호텔 도착 맵: bStartScreenBlack이면 검은 화면 → 페이드 인 → 대화 시작, DepartureSoundNodeID 확인 시 택시 출발 사운드
// - NextLevel이 비어 있으면 대화가 끝날 때 이동 · 시점 잠금 해제
UCLASS()
class LASTCHECKOUT_API ALCOTaxiIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	ALCOTaxiIntroDirector();

	// 호텔 도착 페이드 인 (검은 화면 → 장면). BP에서 덮어써서 연출 변경 가능 (대화는 ArrivalFadeInDuration 뒤에 시작)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|TaxiIntro")
	void PlayTaxiArrivalFadeIn();

	// 택시 엔진 · 출발 사운드. BP에서 덮어써서 연출 변경 가능
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|TaxiIntro")
	void PlayTaxiDepartureSound();

	// 페이드 인이 끝난 뒤 도착 브금. BP에서 덮어써서 연출 변경 가능
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|TaxiIntro")
	void PlayArrivalBGM();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 재생할 대화 (DialogueFlow_<DialogueID>.csv)
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	FName DialogueID = TEXT("TaxiDriver");

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	FName StartNodeID = TEXT("FIRST_01_01");

	// 로딩이 끝나고 대화가 시작되기까지 대기 시간
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro", meta = (ClampMin = "0.1", Units = "s"))
	float StartDelay = 1.f;

	// 로딩 화면이 사라질 때까지 기다린 뒤 StartDelay 시작
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	bool bWaitForLoadingScreen = true;

	// 대화가 끝나면 이동할 맵 (Test_Outdoor)
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	TSoftObjectPtr<UWorld> NextLevel;

	// 도착할 PlayerStart의 태그
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	FName NextPlayerStartTag = TEXT("TaxiSpawn");

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	bool bAutoSaveOnArrival = true;

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	bool bLockPlayerMovement = true;

	// 대화가 끝날 때까지 시점 회전도 막음 (화면 고정)
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	bool bLockPlayerLook = false;

	// 인트로를 본 기록 (상태 플래그, 세이브에 저장), 있으면 연출 전체를 건너뜀, 비우면 매번 실행
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	FName PlayedStateFlag;

	// ---------- 호텔 도착 ----------

	// 맵 시작부터 검은 화면 유지 → StartDelay 뒤 페이드 인 → 끝나면 대화 시작
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	bool bStartScreenBlack = false;

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0.0", Units = "s"))
	float ArrivalFadeInDuration = 1.5f;

	// 검은 화면에서 대화 전에 보여 줄 위치 자막 (WBP_LocationCaption). 비우면 자막 없음
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	TSubclassOf<ULCOLocationCaptionWidget> LocationCaptionClass;

	// 자막 줄 (UI.csv 키). 시계 줄은 Show Clock 체크
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	TArray<FLCOLocationCaptionLine> LocationCaptionLines;

	// 자막 시계 시작 시각
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0", ClampMax = "23"))
	int32 ClockStartHour = 15;

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0", ClampMax = "59"))
	int32 ClockStartMinute = 20;

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0", ClampMax = "59"))
	int32 ClockStartSecond = 0;

	// 이 대사를 넘길 때 페이드 인 (검은 화면 위에 대사 표시). 비우면 페이드 인 후 대화 시작
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	FName FadeInNodeID;

	// 페이드 인이 끝나면 재생 (맵을 떠날 때까지 유지, 반복은 사운드 에셋의 Looping)
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	TObjectPtr<USoundBase> ArrivalBGM;

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0.0"))
	float ArrivalBGMVolume = 1.f;

	// 브금이 작게 시작해서 커지는 시간
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0.0", Units = "s"))
	float ArrivalBGMFadeInTime = 1.f;

	// 이 노드를 확인(다음으로 진행)할 때 택시 출발 사운드
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	FName DepartureSoundNodeID = TEXT("ARRIVAL_01_04");

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	TObjectPtr<USoundBase> TaxiDepartureSound;

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0.0"))
	float DepartureSoundVolume = 1.f;

private:
	UFUNCTION()
	void HandleDialogueFinished(FName FinishedDialogueID, bool bCompletedNormally);

	UFUNCTION()
	void HandleLoadingFinished(bool bSuccess);

	UFUNCTION()
	void HandleDialogueNodeConfirmed(FName ConfirmedDialogueID, FName NodeID);

	// 검은 화면이면 (자막 →) 페이드 인 후, 아니면 바로 대화 시작
	void BeginIntroSequence();

	// 자막 이후 진행
	void ContinueIntroSequence();

	UFUNCTION()
	void HandleLocationCaptionFinished();

	// 페이드 인 1회 + 끝나면 브금
	void TriggerArrivalFadeIn();

	// 이동 · 시점 잠금 해제 (NextLevel이 없을 때)
	void UnlockPlayerInput();

	// 이동 · 시점 잠금 (BeginPlay 다음 프레임. PlayerController BeginPlay의 입력 초기화 이후에 걸어야 유지됨)
	void LockPlayerInput();

	void ScheduleIntroDialogue();
	void StartIntroDialogue();
	void TravelToNextLevel();

	FTimerHandle StartDialogueTimerHandle;
	FTimerHandle ArrivalFadeTimerHandle;
	FTimerHandle ArrivalBGMTimerHandle;
	FTimerHandle LockInputTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ArrivalBGMComponent;

	UPROPERTY(Transient)
	TObjectPtr<ULCOLocationCaptionWidget> LocationCaptionWidget;

	bool bLocationCaptionShown = false;
	bool bTravelStarted = false;
	bool bArrivalFadePlayed = false;
	bool bDepartureSoundPlayed = false;
	bool bInputLocked = false;

	// 잠금 해제가 이미 요청됨 (다음 프레임 잠금보다 해제가 먼저 오면 잠그지 않음)
	bool bInputUnlockRequested = false;
};
