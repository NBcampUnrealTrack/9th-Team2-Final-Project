#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "LCOPhoneMenuWidget.generated.h"

class ALCOPhone;
class UButton;
class UBorder;
class UTextBlock;

// OnMenuRequestedDelegate의 Phone을 전달한다. 위젯 생성은 BP에서 담당한다.
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOPhoneMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 상호작용한 플레이어를 OwningPlayer로 지정한 뒤 호출한다. 초기화와 화면 표시를 함께 처리한다.
	// 다른 UI/대화 또는 일시정지 중에는 열지 않는다.
	UFUNCTION(BlueprintCallable, Category = "LCO|PhoneMenu")
	bool OpenPhoneMenu(ALCOPhone* InPhone);

	// 취소: UI 정리 후 전화기의 CloseMenu를 호출한다.
	UFUNCTION(BlueprintCallable, Category = "LCO|PhoneMenu")
	void ClosePhoneMenu();

	// UI만 정리한다. BP_TestPhone의 OnMenuClosed에서 호출할 수 있다.
	// StartCall/SaveAtPhone 전에 사용하며 전화기의 메뉴 사용자 정보는 소비하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "LCO|PhoneMenu")
	void DismissPhoneMenu();

	UFUNCTION(BlueprintCallable, Category = "LCO|PhoneMenu")
	void RefreshButtonStates();

	// BP_TestPhone.OnGameSaved의 실제 결과만 전달한다. 저장 반환값으로 성공을 추정하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "LCO|PhoneMenu")
	void ShowSaveResult(bool bSuccess);

	UFUNCTION(BlueprintPure, Category = "LCO|PhoneMenu")
	bool IsSaveFeedbackActive() const { return bWaitingForSaveResult || bShowingSaveResult; }

	UFUNCTION(BlueprintPure, Category = "LCO|PhoneMenu")
	ALCOPhone* GetPhone() const { return Phone.Get(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Call;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Save;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Cancel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Description;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Choices;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Dialogue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_MenuTitle;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|PhoneMenu")
	int32 ViewportZOrder = 19;

private:
	UFUNCTION()
	void HandleCallClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleCancelClicked();

	void RestoreInput();
	void NotifyPromptVisibility(bool bVisible);
	void FinishSaveFeedback();
	void ResetSaveFeedback();
	void HandleMinimumSaveWaitElapsed();
	void TryShowSaveResult();
	void AdvanceSavePendingAnimation();
	void RefreshSavePendingText();
	void ApplySaveFeedbackLayout();
	FText ResolveFeedbackText(FName Key, const FText& Fallback) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALCOPhone> Phone;

	bool bMenuActive = false;
	bool bPreviousShowMouseCursor = false;
	bool bMoveInputBlockedByThisWidget = false;
	bool bLookInputBlockedByThisWidget = false;
	bool bWaitingForSaveResult = false;
	bool bShowingSaveResult = false;
	bool bMinimumSaveWaitElapsed = false;
	bool bHasReceivedSaveResult = false;
	bool bPendingSaveSuccess = false;
	FTimerHandle MinimumSaveWaitTimerHandle;
	FTimerHandle SavePendingAnimationTimerHandle;
	int32 SavePendingAnimationIndex = 0;
	FText OriginalDescription;
	TArray<TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>> OriginalFeedbackVisibility;
};
