#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOMainMenuWidget.generated.h"

class UButton;

/**
 * 메인 메뉴 위젯 베이스 (게임 시작 / 종료)
 * - 디자인은 WBP_MainMenu(부모 클래스 = 이 클래스)에서 진행
 * - WBP 안의 버튼 이름이 BindWidget 변수명과 같아야 자동 연결됨
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

	// 게임 시작 시 열 레벨 (WBP 클래스 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|MainMenu")
	TSoftObjectPtr<UWorld> GameLevel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Start;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Quit;

private:
	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleQuitClicked();

	// 레벨 이동 중 중복 클릭 방지
	bool bIsTransitioning = false;
};
