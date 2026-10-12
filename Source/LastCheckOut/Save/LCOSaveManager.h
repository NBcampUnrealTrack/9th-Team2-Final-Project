#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOSaveManager.generated.h"

class ULCOSaveGame;

// 저장 시작·완료 알림 (저장 중 아이콘 UI 등에서 사용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOSaveStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOSaveFinished, bool, bSuccess);

// 세이브 파일 저장·불러오기 담당 (GameInstance 단위라 레벨 이동해도 유지)
// - 저장: SaveGame() → 스토리 / 수첩 / 플레이어 상태를 LCOSaveGame에 채운 뒤 비동기 저장
// - 이어하기: StartContinue() → 파일 읽기 → 저장한 맵으로 이동
//            → 새 맵의 LCOGameMode가 ApplyLoadedData() 호출 → 각 시스템에 적용
// - 슬롯은 1개 (LCO_Save)
UCLASS()
class LASTCHECKOUT_API ULCOSaveManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// GameInstance 생성 시 엔진이 자동 호출 (생성 확인 로그)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ---------- 확인 ----------

	// 세이브 파일이 있는지 (메인 메뉴 이어하기 버튼 활성화용)
	UFUNCTION(BlueprintPure, Category = "LCO|Save")
	bool HasSaveFile() const;

	// 저장 진행 중인지
	UFUNCTION(BlueprintPure, Category = "LCO|Save")
	bool IsSaving() const { return bIsSaving; }

	// ---------- 저장 ----------

	// 현재 진행 상태를 모아 비동기 저장. 이미 저장 중이면 무시하고 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Save")
	bool SaveGame();

	// ---------- 이어하기 ----------

	// 이어하기 시작: 세이브 파일을 읽고 저장한 맵으로 이동 (메인 메뉴 이어하기 버튼에서 호출)
	// 실제 데이터 적용은 이동한 맵의 LCOGameMode(Continue 분기)에서 ApplyLoadedData로 처리
	UFUNCTION(BlueprintCallable, Category = "LCO|Save")
	bool StartContinue();

	// 세이브 파일을 읽어 메모리에 보관 (실제 적용은 ApplyLoadedData). 실패 시 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Save")
	bool LoadSaveFile();

	// 읽어온 데이터를 각 시스템에 적용 (스토리 / 수첩 / 플레이어). LoadSaveFile 먼저 호출 필요
	UFUNCTION(BlueprintCallable, Category = "LCO|Save")
	bool ApplyLoadedData();

	// 세이브 파일을 이미 읽어 둔 상태인지 (LoadSaveFile 성공 여부)
	UFUNCTION(BlueprintPure, Category = "LCO|Save")
	bool HasLoadedData() const { return LoadedSaveGame != nullptr; }

	// 불러온 세이브의 맵 경로. 불러온 데이터가 없으면 빈 문자열
	UFUNCTION(BlueprintPure, Category = "LCO|Save")
	FString GetLoadedMapName() const;

	// ---------- 델리게이트 ----------

	UPROPERTY(BlueprintAssignable, Category = "LCO|Save")
	FOnLCOSaveStarted OnSaveStarted;

	UPROPERTY(BlueprintAssignable, Category = "LCO|Save")
	FOnLCOSaveFinished OnSaveFinished;

private:
	// AsyncSaveGameToSlot 완료 시 호출
	void HandleAsyncSaveFinished(const FString& SlotName, const int32 UserIndex, bool bSuccess);

	// 세이브 슬롯 이름 / 유저 인덱스 (슬롯 1개)
	static const FString SaveSlotName;
	static constexpr int32 SaveUserIndex = 0;

	// LoadSaveFile로 읽어온 데이터
	UPROPERTY()
	TObjectPtr<ULCOSaveGame> LoadedSaveGame;

	bool bIsSaving = false;
};
