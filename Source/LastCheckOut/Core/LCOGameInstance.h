#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LCOGameInstance.generated.h"

UENUM(BlueprintType)
enum class ELCOGameStartMode : uint8
{
	None      UMETA(DisplayName = "None"),       // 선택 안 됨 (메뉴 없이 바로 실행 등)
	NewGame   UMETA(DisplayName = "New Game"),   // 새 게임
	Continue  UMETA(DisplayName = "Continue")    // 이어하기
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

private:
	// 현재 시작 방식. 기본값 None
	// VisibleInstanceOnly: 디버깅 시 디테일 패널에서 값 확인용 (수정은 불가)
	UPROPERTY(VisibleInstanceOnly, Category = "LCO|GameStart")
	ELCOGameStartMode StartMode = ELCOGameStartMode::None;
};
