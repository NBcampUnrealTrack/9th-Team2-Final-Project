#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LCOGameMode.generated.h"

class ULCOGameInstance;
struct FLCOPlayerCarryData;

UCLASS()
class LASTCHECKOUT_API ALCOGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ALCOGameMode();

	// 게임 중 맵 이동은 Open Level 대신 이 함수를 사용 (인벤토리 / 퀵슬롯 / 시점 유지)
	UFUNCTION(BlueprintCallable, Category = "LCO|Travel")
	void TravelToLevel(TSoftObjectPtr<UWorld> Level);

	// 현재 플레이어의 인벤토리 / 퀵슬롯 / 시점을 OutData에 복사. 플레이어가 없으면 false
	// (맵 이동 보관 / 세이브 저장에서 공용으로 사용)
	bool CapturePlayerCarryData(FLCOPlayerCarryData& OutData) const;
	
protected:
	virtual void BeginPlay() override;

	// 플레이어 캐릭터 생성 + 모든 액터 BeginPlay가 끝난 뒤 호출됨 → 여기서 맵 이동 데이터 복원
	virtual void HandleMatchHasStarted() override;
	
private:
	void InitializeCurrentMap();
	
	void StartGame();

	// 새 게임 진행 데이터 초기화: 수첩 → 이동 보관함 → 스토리 순서
	void StartNewGameProgress(ULCOGameInstance* LCOGameInstance);

	// 맵 이동 직전: 캐릭터의 인벤토리 / 퀵슬롯 / 시점을 GameInstance에 보관
	void StorePlayerCarryData();

	// 새 맵 시작 후: 보관한 데이터를 새 캐릭터에 복원
	void RestorePlayerCarryData();
};
