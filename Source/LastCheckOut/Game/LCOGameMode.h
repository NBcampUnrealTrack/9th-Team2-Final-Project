#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Story/LCOStoryTypes.h"
#include "Game/LCOTimeOfDayTypes.h"
#include "LCOGameMode.generated.h"

class ULCOGameInstance;
class ALCOTimeOfDayManager;
struct FLCOPlayerCarryData;

UCLASS()
class LASTCHECKOUT_API ALCOGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ALCOGameMode();

	// 게임 중 맵 이동은 Open Level 대신 이 함수를 사용 (인벤토리 / 퀵슬롯 / 시점 유지)
	// PlayerStartTag: 도착할 PlayerStart의 태그 (비우면 기본 PlayerStart)
	// bAutoSaveOnArrival: 도착한 맵에서 플레이어 복원 후 자동 저장 (챕터 전환 등)
	UFUNCTION(BlueprintCallable, Category = "LCO|Travel")
	void TravelToLevel(TSoftObjectPtr<UWorld> Level, FName PlayerStartTag = NAME_None, bool bAutoSaveOnArrival = false);

	// 현재 플레이어의 인벤토리 / 퀵슬롯 / 시점을 OutData에 복사. 플레이어가 없으면 false
	// (맵 이동 보관 / 세이브 저장에서 공용으로 사용)
	bool CapturePlayerCarryData(FLCOPlayerCarryData& OutData) const;

	// 테스트용: 진행된 시간대와 상관없이 이 맵의 조명만 바꿈 (저장 안 됨, 맵 이동 시 원래 시간대로)
	UFUNCTION(BlueprintCallable, Category = "LCO|TimeOfDay|Test")
	void ApplyTimeOfDayForTest(ELCOTimeOfDay TimeOfDay);

	// 시간대를 다음 단계로 진행 (앞으로만, 저장되는 값). 지금 맵 조명에도 바로 적용
	// 챕터가 끝날 때 호출 (1막 클리어 → 해질녘)
	UFUNCTION(BlueprintCallable, Category = "LCO|TimeOfDay")
	void AdvanceTimeOfDay(ELCOTimeOfDay NewTimeOfDay);

	// 챕터별 시간대 (Chapter1: 정오 이후 / Chapter2: 해질녘 / Chapter3: 저녁)
	static ELCOTimeOfDay GetTimeOfDayForChapter(FName ChapterID);
	
protected:
	// 맵 액터들의 BeginPlay보다 먼저 호출됨 → 여기서 진행 데이터 준비 (새 게임 초기화 / 이어하기 적용)
	virtual void StartPlay() override;

	// 플레이어 캐릭터 생성 + 모든 액터 BeginPlay가 끝난 뒤 호출됨 → 여기서 맵 상태 / 맵 이동 데이터 복원
	virtual void HandleMatchHasStarted() override;

	// 맵 이동 시 지정한 도착 태그(Entry 옵션)와 같은 PlayerStart를 선택
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// 맵을 떠날 때: 스토리 알림 연결 해제
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 1막 클리어 후 2막을 시작할 맵 (비우면 이동 없이 챕터만 바뀌고 그 자리에서 자동 저장)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Story")
	TSoftObjectPtr<UWorld> Chapter2StartLevel;

	// 2막 시작 맵에서 도착할 PlayerStart 태그 (별관 407호 앞)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Story")
	FName Chapter2StartTag = TEXT("Room407");

	// END 3 등 엔딩 후 돌아갈 메인 메뉴 맵 (프로토타입: 엔딩 화면 대신 메인 메뉴로 복귀)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Story")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	// END 3 판정 후 메인 메뉴로 이동하기까지 기다리는 시간
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Story", meta = (ClampMin = "0.0", Units = "s"))
	float End3ReturnDelay = 1.f;
	
private:
	void InitializeCurrentMap();
	
	void StartGame();
	
	void StartNewGameProgress(ULCOGameInstance* LCOGameInstance);
	
	void StorePlayerCarryData();
	
	void RestorePlayerCarryData();

	// 지배인 보고 결과 처리 (1막 클리어 → 2막 시작 맵으로 이동 + 도착 후 자동 저장)
	UFUNCTION()
	void HandleReportResolved(ELCOStoryFlag PresentedKeyword, ELCOReportResult Result);

	// 맵 주소에 AutoSave 옵션이 붙어 왔으면 자동 저장
	void TryAutoSaveOnArrival();

	// 메인 메뉴로 이동 (END 3 후). 저장하지 않음 → 이어하기는 마지막 저장 시점부터
	void ReturnToMainMenu();

	// 저장된 시간대(챕터 시간대보다 이르면 챕터 시간대)로 조명 적용
	void RefreshTimeOfDay();

	// LCOTimeOfDayManager
	ALCOTimeOfDayManager* FindTimeOfDayManager() const;

	FTimerHandle ReturnToMainMenuTimer;

	// 로딩 화면이 끝나면 대기 중인 버프 적용 (로딩 중이면 0.1초마다 다시 확인)
	void ApplyPendingBuffsWhenLoaded();

	FTimerHandle PendingBuffTimer;

	// 로딩이 끝나면 적용할 버프 (남은 시간이 0이면 없음)
	float PendingMoveSpeedBuffAmount = 0.f;
	float PendingMoveSpeedBuffTime = 0.f;
	int32 PendingMaxStaminaBuffAmount = 0;
	float PendingMaxStaminaBuffTime = 0.f;
};
