#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventorySlot.h"
#include "Player/LCOPlayerCharacter.h"
#include "Game/LCOTimeOfDayTypes.h"
#include "LCOGameInstance.generated.h"

class ULCOStorySubsystem;
class ULCONotebookSubsystem;

UENUM(BlueprintType)
enum class ELCOGameStartMode : uint8
{
	None      UMETA(DisplayName = "None"),       // 선택 안 됨 (메뉴 없이 바로 실행 등)
	NewGame   UMETA(DisplayName = "New Game"),   // 새 게임
	Continue  UMETA(DisplayName = "Continue")    // 이어하기
};

// 맵 이동(Open Level) 시 캐릭터가 새로 생성되면서 사라지는 데이터를 잠깐 맡아두는 구조체
// 이동 직전에 채우고, 새 맵에서 복원한 즉시 비움 (평소 원본은 캐릭터의 컴포넌트)
USTRUCT()
struct FLCOPlayerCarryData
{
	GENERATED_BODY()

	// 인벤토리 슬롯 (InventoryComponent::InventorySlots)
	UPROPERTY()
	TArray<FInventorySlot> InventorySlots;

	// 퀵슬롯 등록 아이템 (슬롯 순서대로)
	UPROPERTY()
	TArray<FName> QuickSlotItemIDs;

	// 1인칭 / 3인칭 시점
	UPROPERTY()
	ELCOViewMode ViewMode = ELCOViewMode::FirstPerson;

	// 버프 (커피 등). 남은 시간이 0이면 버프 없음
	// 새 맵에서 로딩 화면이 끝난 뒤 남은 시간만큼 다시 적용 (LCOGameMode)
	UPROPERTY()
	float MoveSpeedBuffAmount = 0.f;

	UPROPERTY()
	float MoveSpeedBuffRemainingTime = 0.f;

	UPROPERTY()
	int32 MaxStaminaBuffAmount = 0;

	UPROPERTY()
	float MaxStaminaBuffRemainingTime = 0.f;

	// 직감 남은 사용 횟수. -1이면 기록 없음 (이 항목이 생기기 전 세이브) → 복원하지 않음
	UPROPERTY()
	int32 SenseCharges = -1;

	// 패시브 아이템 영구 효과 배율 (기본 1.0)
	UPROPERTY()
	float PermanentMoveSpeedMultiplier = 1.f;

	UPROPERTY()
	float PermanentRegenMultiplier = 1.f;

	// 보관된 데이터가 있는지
	UPROPERTY()
	bool bHasData = false;
};

UCLASS()
class LASTCHECKOUT_API ULCOGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	// 게임 실행 시 1회 호출. GameInstance 적용 여부 확인용 로그
	virtual void Init() override;
	
	// 시작 방식 설정 (메인 메뉴 시작 버튼 LCOMainMenuWidget에서 NewGame으로 설정)
	UFUNCTION(BlueprintCallable, Category = "LCO|GameStart")
	void SetStartMode(ELCOGameStartMode NewMode);

	// 현재 시작 방식 조회 (LCOGameMode::StartGame()에서 사용)
	UFUNCTION(BlueprintPure, Category = "LCO|GameStart")
	ELCOGameStartMode GetStartMode() const { return StartMode; }

	// None으로 되돌림 
	// LCOGameMode의 NewGame 처리 후, LCOPauseMenuWidget의 메인 메뉴 복귀 시 호출
	UFUNCTION(BlueprintCallable, Category = "LCO|GameStart")
	void ResetStartMode();

	// ---------- Subsystem 접근 ----------

	// 스토리 진행 (챕터 / 목표 / 완료 목표)
	UFUNCTION(BlueprintPure, Category = "LCO|System")
	ULCOStorySubsystem* GetStorySubsystem() const;

	// 수첩 기록
	UFUNCTION(BlueprintPure, Category = "LCO|System")
	ULCONotebookSubsystem* GetNotebookSubsystem() const;

	// ---------- 맵 이동용 임시 보관 ----------

	// 맵 이동 직전 저장 (LCOGameMode::TravelToLevel()에서 호출)
	void StoreCarryData(const FLCOPlayerCarryData& Data);

	// 꺼내면서 비움. 보관된 데이터가 없으면 false (LCOGameMode::RestorePlayerCarryData()에서 호출)
	bool ConsumeCarryData(FLCOPlayerCarryData& OutData);

	// 보관함 비우기 (새 게임 시작 시 호출)
	void ClearCarryData();

	// ---------- 시간대 ----------

	// 지금까지 진행된 시간대 (맵 도착으로 앞으로만 진행, 맵을 옮겨도 유지). 세이브에 저장
	UFUNCTION(BlueprintPure, Category = "LCO|TimeOfDay")
	ELCOTimeOfDay GetCurrentTimeOfDay() const { return CurrentTimeOfDay; }

	// LCOGameMode(맵 도착 / 새 게임 초기화), LCOSaveManager(이어하기 복원)에서 호출
	void SetCurrentTimeOfDay(ELCOTimeOfDay NewTimeOfDay) { CurrentTimeOfDay = NewTimeOfDay; }

private:
	// 현재 시작 방식. 기본값 None
	// VisibleInstanceOnly: 디버깅 시 디테일 패널에서 값 확인용 (수정은 불가)
	UPROPERTY(VisibleInstanceOnly, Category = "LCO|GameStart")
	ELCOGameStartMode StartMode = ELCOGameStartMode::None;

	// 맵 이동 중에만 값이 들어 있음
	UPROPERTY()
	FLCOPlayerCarryData CarryData;

	// 지금까지 진행된 시간대 (새 게임 = 정오 이후)
	UPROPERTY(VisibleInstanceOnly, Category = "LCO|TimeOfDay")
	ELCOTimeOfDay CurrentTimeOfDay = ELCOTimeOfDay::Noon;
};
