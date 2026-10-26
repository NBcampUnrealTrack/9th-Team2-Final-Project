#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOTaxiIntroDirector.generated.h"

// 게임 인트로 (택시 맵) 진행 담당 (Taxi 맵에 1개 배치)
// - 로딩 화면이 사라진 뒤 StartDelay 뒤에 택시 기사 대화를 자동으로 시작 (로딩이 없으면 맵 시작 기준)
// - 대화가 끝나면 NextLevel의 NextPlayerStartTag 위치로 이동 (도착 후 자동 저장)
// - 택시 안에서는 플레이어 이동을 막음 (시점 회전은 가능)
UCLASS()
class LASTCHECKOUT_API ALCOTaxiIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	ALCOTaxiIntroDirector();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 재생할 대화 (DialogueFlow_<DialogueID>.csv)
	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	FName DialogueID = TEXT("TaxiDriver");

	UPROPERTY(EditAnywhere, Category = "LCO|TaxiIntro")
	FName StartNodeID = TEXT("FIRST_01");

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

private:
	UFUNCTION()
	void HandleDialogueFinished(FName FinishedDialogueID, bool bCompletedNormally);

	UFUNCTION()
	void HandleLoadingFinished(bool bSuccess);

	void ScheduleIntroDialogue();
	void StartIntroDialogue();
	void TravelToNextLevel();

	FTimerHandle StartDialogueTimerHandle;
	bool bTravelStarted = false;
};
