#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOPauseMenuWidget.generated.h"

class UButton;

/**
 * 일시정지 메뉴 위젯 베이스 (계속하기 / 설정 / 메인메뉴로 / 게임 종료)
 * - 디자인은 WBP_PauseMenu(부모 클래스 = 이 클래스)에서 진행
 * - 입력(ESC 등) 담당 쪽에서 위젯을 만들어 두고 OpenPauseMenu()만 호출하면 됨
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 화면에 띄우고 게임 일시정지 + UI 입력 + 커서 표시
	UFUNCTION(BlueprintCallable, Category = "LCO|PauseMenu")
	void OpenPauseMenu();

	// 화면에서 내리고 일시정지 해제 + 게임 입력 + 커서 숨김
	UFUNCTION(BlueprintCallable, Category = "LCO|PauseMenu")
	void ClosePauseMenu();

protected:
	virtual void NativeOnInitialized() override;

	// 메인메뉴로 버튼을 눌렀을 때 열 레벨 (WBP 클래스 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|PauseMenu")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	// 설정 버튼으로 띄울 설정 위젯 (팀장님 설정 위젯 완성 후 WBP 클래스 디폴트에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|PauseMenu")
	TSubclassOf<UUserWidget> SettingsWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Resume;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_MainMenu;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Quit;

private:
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleMainMenuClicked();

	UFUNCTION()
	void HandleQuitClicked();

	// 생성한 설정 위젯 (중복 생성 방지용)
	UPROPERTY()
	TObjectPtr<UUserWidget> SettingsWidget;

	// 레벨 이동 중 중복 클릭 방지
	bool bIsTransitioning = false;
};
