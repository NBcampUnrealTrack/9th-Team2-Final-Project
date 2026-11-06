#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Story/LCOStoryTypes.h"
#include "Game/LCOTimeOfDayTypes.h"
#include "LCOGameMode.generated.h"

class ULCOGameInstance;
class ALCOTimeOfDayManager;
class ULCOTimeOfDayPresetAsset;
class UUserWidget;
struct FLCOPlayerCarryData;

// 챕터 설정 (시작 위치 / 시간대 / 수첩 압수)
// 목록에 없는 챕터(Chapter1)는 정오 이후, 이동 없음, 압수 없음
USTRUCT(BlueprintType)
struct FLCOChapterStartInfo
{
	GENERATED_BODY()

	// 챕터가 시작되면 이동할 맵. 비우면 이동 없이 그 자리에서 자동 저장
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	TSoftObjectPtr<UWorld> Level;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	FName PlayerStartTag;

	// 이 챕터의 시간대 (앞으로만 진행)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	ELCOTimeOfDay TimeOfDay = ELCOTimeOfDay::Noon;

	// 추격에서 잡히면 수첩 압수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	bool bConfiscateNotebookOnCaught = true;

	// 아직 만들지 않은 챕터 (출시 범위 끝). 이 챕터로 넘어가면 시간대 · 이동 · 저장 없이 "To be continued" 엔딩
	// 저장하지 않으므로 이어하기는 마지막 저장 시점부터. 챕터를 만들면 체크 해제
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	bool bEndOfContent = false;
};

// 엔딩 도달 (UI: 엔딩 화면 표시 후 FinishEnding 호출)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOEndingReached, FName, EndingID);

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

	// 챕터별 시간대 (ChapterStartInfos의 TimeOfDay, 목록에 없으면 정오 이후)
	ELCOTimeOfDay GetTimeOfDayForChapter(FName ChapterID) const;

	// 엔딩 도달. OnEndingReached에 연결된 화면이 없으면 EndingReturnDelay 후 메인 메뉴로
	UFUNCTION(BlueprintCallable, Category = "LCO|Ending")
	void ReachEnding(FName EndingID);

	// 엔딩 화면이 끝났을 때 호출 → 메인 메뉴로
	UFUNCTION(BlueprintCallable, Category = "LCO|Ending")
	void FinishEnding();

	UPROPERTY(BlueprintAssignable, Category = "LCO|Ending")
	FOnLCOEndingReached OnEndingReached;
	
protected:
	// 맵 액터들의 BeginPlay보다 먼저 호출됨 → 여기서 진행 데이터 준비 (새 게임 초기화 / 이어하기 적용)
	virtual void StartPlay() override;

	// 플레이어 캐릭터 생성 + 모든 액터 BeginPlay가 끝난 뒤 호출됨 → 여기서 맵 상태 / 맵 이동 데이터 복원
	virtual void HandleMatchHasStarted() override;

	// 맵 이동 시 지정한 도착 태그(Entry 옵션)와 같은 PlayerStart를 선택
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// 맵을 떠날 때: 스토리 알림 연결 해제
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 챕터 ID(Chapter2, Chapter3 ...) → 시작 맵 + 도착 PlayerStart 태그 + 시간대 + 수첩 압수
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Story")
	TMap<FName, FLCOChapterStartInfo> ChapterStartInfos;

	// 모든 맵 공용 시간대 조명 값 (DA_TimeOfDayPresets). 비우면 맵마다 배치된 매니저의 값 사용
	UPROPERTY(EditDefaultsOnly, Category = "LCO|TimeOfDay")
	TObjectPtr<ULCOTimeOfDayPresetAsset> TimeOfDayPresets;

	// 엔딩 후 돌아갈 메인 메뉴 맵
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Story")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	// 엔딩 화면이 연결되지 않았을 때 메인 메뉴로 이동하기까지 기다리는 시간
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Story", meta = (ClampMin = "0.0", Units = "s"))
	float EndingReturnDelay = 1.f;

	// 엔딩 시작 시 화면이 검게 바뀌는 시간 (끝나면 엔딩 화면 표시)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Ending", meta = (ClampMin = "0.0", Units = "s"))
	float EndingFadeOutDuration = 1.5f;

	// 엔딩 ID → 엔딩 화면 위젯 (예: ToBeContinued → WBP_ToBeContinued, End3 → WBP_End3)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Ending")
	TMap<FName, TSubclassOf<UUserWidget>> EndingWidgets;

	// 엔딩 화면을 보여 주는 시간 (끝나면 메인 메뉴)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Ending", meta = (ClampMin = "0.0", Units = "s"))
	float EndingScreenDuration = 4.f;
	
private:
	void InitializeCurrentMap();
	
	void StartGame();
	
	void StartNewGameProgress(ULCOGameInstance* LCOGameInstance);
	
	void StorePlayerCarryData();
	
	void RestorePlayerCarryData();

	// 이어하기: 저장한 위치 / 방향으로 플레이어 이동 (위치 정보가 없으면 PlayerStart 그대로)
	void RestoreSavedPlayerTransform();

	// 지배인 보고 결과 처리 (1막 클리어 → 2막 시작 맵으로 이동 + 도착 후 자동 저장)
	UFUNCTION()
	void HandleReportResolved(ELCOStoryFlag PresentedKeyword, ELCOReportResult Result);

	// 챕터 클리어 알림 (2막 클리어 → 3막 시작 등)
	UFUNCTION()
	void HandleChapterAdvanced(FName CompletedChapterID, FName NewChapterID);

	// 추격에서 잡힌 뒤 처리가 끝났을 때 (2막부터 수첩 압수)
	UFUNCTION()
	void HandleCaughtSequenceFinished(AActor* Player);

	// 맵의 추격 매니저에 잡힘 알림 연결
	void BindChasingManager();

	// 새 챕터 시작 공통 처리: 시간대 진행 → ChapterStartInfos의 시작 맵으로 이동 → 도착 후 자동 저장
	void StartChapterTransition(FName NewChapterID);

	// 엔딩 진행 중 (중복 호출 방지)
	bool bEndingInProgress = false;

	// 페이드 아웃이 끝나면 엔딩 화면 표시 (위젯이 없으면 OnEndingReached 또는 메인 메뉴)
	void ShowEndingScreen();

	FName PendingEndingID;
	FTimerHandle EndingFadeTimer;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> EndingWidget;

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
