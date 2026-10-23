#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/LCOSettingsPanelBase.h"
#include "Components/ComboBoxString.h"
#include "LCOGraphicsSettingsPanel.generated.h"

class UButton;
class USlider;
class ULCOGraphicsConfirmWidget;

// 설정창 그래픽 탭
UCLASS()
class LASTCHECKOUT_API ULCOGraphicsSettingsPanel : public ULCOSettingsPanelBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshLocalizedTexts() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GraphicsTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WindowModeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> WindowModeComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResolutionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ResolutionComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QualityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> QualityComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GraphicsApplyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GraphicsApplyButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GraphicsCancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GraphicsCancelButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BrightnessText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> BrightnessSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BrightnessValueText;
	
	UPROPERTY(EditAnywhere, Category = "LCO|Settings")
	TSubclassOf<ULCOGraphicsConfirmWidget> GraphicsConfirmWidgetClass;

private:
	UFUNCTION()
	void HandleWindowModeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleResolutionSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleGraphicsApplyClicked();

	UFUNCTION()
	void HandleGraphicsCancelClicked();

	UFUNCTION()
	void HandleBrightnessChanged(float Value);

	UFUNCTION()
	void HandleGraphicsConfirmFinished(bool bKept);

	UFUNCTION()
	void HandleGameSettingsChanged();

	void BindControls();
	void UnbindControls();
	void InitializeGraphicsOptions();
	void InitializeBrightness();
	void RefreshBrightnessValueText();
	void RevertUnappliedGraphicsSettings();
	
	bool bUpdatingControls = false;
	bool bChangingSettings = false;
	bool bGraphicsSettingsDirty = false;
	
	TArray<FIntPoint> ResolutionOptions;

	UPROPERTY()
	TObjectPtr<ULCOGraphicsConfirmWidget> ConfirmWidget;
};
