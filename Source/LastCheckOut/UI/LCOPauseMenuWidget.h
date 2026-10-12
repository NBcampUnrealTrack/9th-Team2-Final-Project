#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOPauseMenuWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * 일시정지 메뉴 위젯 베이스 (계속하기 / 설정 / 메인메뉴로 / 게임 종료)
 * - 디자인은 WBP_PauseMenu(부모 클래스 = 이 클래스)에서 진행
 * - 게임 맵에서 위젯을 한 번 생성만 해두면,
 *   LCOPlayerController의 OnOptionsRequested(O 키)에 자동 바인딩되어 열기/닫기 토글
 * - 커서/입력 처리는 LCOPlayerController::SetUIMode()에 맡김
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 화면에 띄우고 게임 일시정지 + UI 모드
	UFUNCTION(BlueprintCallable, Category = "LCO|PauseMenu")
	void OpenPauseMenu();

	// 화면에서 내리고 일시정지 해제 + 게임 모드
	UFUNCTION(BlueprintCallable, Category = "LCO|PauseMenu")
	void ClosePauseMenu();

	// 열려 있으면 닫고, 닫혀 있으면 엶
	UFUNCTION(BlueprintCallable, Category = "LCO|PauseMenu")
	void TogglePauseMenu();

	void UnbindFromOptionsInput();

	UFUNCTION(BlueprintPure, Category = "LCO|PauseMenu")
	bool IsPauseMenuOpen() const { return IsInViewport(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Resume;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Settings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_MainMenu;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Quit;

private:
	// LCOPlayerController::OnOptionsRequested (O 키) 수신
	UFUNCTION()
	void HandleOptionsRequested();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleMainMenuClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	// UI 모드 전환 (LCOPlayerController면 SetUIMode 사용, 아니면 직접 처리)
	void SetPlayerUIMode(bool bEnable);

	// 일시정지 메뉴를 열기 전에 수첩 / 인벤토리 / 수납장 닫기
	void CloseOtherMenus();
	void BindLocalizationEvents();
	void UnbindLocalizationEvents();
	void RefreshLocalizedTexts();

	// 생성한 설정 위젯 (중복 생성 방지용)
	UPROPERTY()
	TObjectPtr<UUserWidget> SettingsWidget;

	// 레벨 이동 중 중복 클릭 방지
	bool bIsTransitioning = false;
};
