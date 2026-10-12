#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOMainMenuWidget.generated.h"

class UButton;
class UWidget;
enum class ELCOGameStartMode : uint8;

/**
 * 메인 메뉴 위젯 베이스 (로직 담당)
 * - 디자인은 WBP_MainMenu(부모 클래스 = 이 클래스)에서 진행
 * - WBP 안의 위젯 이름이 아래 BindWidget 변수명과 정확히 같아야 자동 연결됨
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 설정 패널 열기/닫기 (BP에서도 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "LCO|MainMenu")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "LCO|MainMenu")
	void CloseSettings();

	// 이어하기 가능 여부 (세이브 슬롯 존재 여부)
	UFUNCTION(BlueprintPure, Category = "LCO|MainMenu")
	bool CanContinue() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	// ---------- BP 연출용 이벤트 (선택 구현) ----------

	// 레벨 이동 직전 호출 (페이드아웃 애니메이션 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|MainMenu")
	void BP_OnStartTransition();

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|MainMenu")
	void BP_OnSettingsOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|MainMenu")
	void BP_OnSettingsClosed();

	// ---------- 설정값 (WBP 클래스 디폴트에서 지정) ----------

	// 새 게임 / 이어하기 시 열 레벨
	UPROPERTY(EditDefaultsOnly, Category = "LCO|MainMenu")
	TSoftObjectPtr<UWorld> GameLevel;

	// 이어하기 판단용 세이브 슬롯 이름 (세이브 시스템 담당자와 맞출 것)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|MainMenu")
	FString SaveSlotName = TEXT("LCO_Save_0");

	// ---------- 바인딩 위젯 (WBP에서 같은 이름으로 생성) ----------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_NewGame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Continue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Quit;

	// 메인 버튼들을 감싸는 패널 (설정 열 때 숨김) - 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuPanel;

	// 설정 패널 - 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SettingsPanel;

	// 설정 패널 안의 뒤로가기 버튼 - 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_SettingsBack;

private:
	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleSettingsBackClicked();

	// 시작 모드 저장 후 게임 레벨로 이동
	void StartGameWithMode(ELCOGameStartMode InStartMode);

	// 레벨 이동 중 중복 클릭 방지
	bool bIsTransitioning = false;
};
