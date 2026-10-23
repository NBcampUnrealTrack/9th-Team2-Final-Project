#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/LCOSettingsPanelBase.h"
#include "LCOGameplaySettingsPanel.generated.h"

class UButton;
class USlider;
class UCheckBox;

// 설정창 게임플레이 탭
UCLASS()
class LASTCHECKOUT_API ULCOGameplaySettingsPanel : public ULCOSettingsPanelBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshLocalizedTexts() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GameplayTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LookSensitivityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> LookSensitivitySlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LookSensitivityValueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InvertLookYText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> InvertLookYCheckBox;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetDefaultsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResetDefaultsButtonText;

private:
	UFUNCTION()
	void HandleLookSensitivityChanged(float Value);

	UFUNCTION()
	void HandleInvertLookYChanged(bool bIsChecked);

	UFUNCTION()
	void HandleResetDefaultsClicked();

	void BindControls();
	void UnbindControls();
	void InitializeControls();
	void RefreshSensitivityValueText();

	bool bUpdatingControls = false;
};
