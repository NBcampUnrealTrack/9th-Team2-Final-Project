#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOTaxiIntroDirector.generated.h"

class USoundBase;

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

	// ---------- 호텔 도착 ----------

	// 맵 시작부터 검은 화면 유지 → StartDelay 뒤 페이드 인 → 끝나면 대화 시작
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival")
	bool bStartScreenBlack = false;

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro|Arrival", meta = (ClampMin = "0.0", Units = "s"))
	float ArrivalFadeInDuration = 1.5f;

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

	// 검은 화면이면 페이드 인 후, 아니면 바로 대화 시작
	void BeginIntroSequence();

	// 이동 · 시점 잠금 해제 (NextLevel이 없을 때)
	void UnlockPlayerInput();

	void ScheduleIntroDialogue();
	void StartIntroDialogue();
	void TravelToNextLevel();

	FTimerHandle StartDialogueTimerHandle;
	FTimerHandle ArrivalFadeTimerHandle;
	bool bTravelStarted = false;
	bool bArrivalFadePlayed = false;
	bool bDepartureSoundPlayed = false;
	bool bInputLocked = false;
};
