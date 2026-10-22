#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "LCOSettingsWidget.generated.h"

class UButton;
class USlider;
class UCheckBox;
class ULCOAudioSettingsSubsystem;
class ULCOGameSettingsSubsystem;

UCLASS()
class LASTCHECKOUT_API ULCOSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> LanguageComboBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> SettingsTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> LanguageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> GameplayText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> GameplayTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> GraphicsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> GraphicsTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> AudioText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> AudioTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> MasterVolumeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> BGMVolumeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> SFXVolumeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> VoiceVolumeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> BGMVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> SFXVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> VoiceVolumeSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ControlsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ControlsTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> BackButtonText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> WindowModeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> WindowModeComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> ResolutionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> ResolutionComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> QualityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> QualityComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GraphicsApplyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> GraphicsApplyButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GraphicsCancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> GraphicsCancelButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> BrightnessText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> BrightnessSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> BrightnessValueText;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> LookSensitivityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> LookSensitivitySlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> LookSensitivityValueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> InvertLookYText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> InvertLookYCheckBox;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetDefaultsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> ResetDefaultsButtonText;

private:
	UFUNCTION()
	void HandleLanguageSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleBGMVolumeChanged(float Value);

	UFUNCTION()
	void HandleSFXVolumeChanged(float Value);

	UFUNCTION()
	void HandleVoiceVolumeChanged(float Value);

	UFUNCTION()
	void HandleAudioSliderCaptureEnd();

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
	void HandleLookSensitivityChanged(float Value);

	UFUNCTION()
	void HandleInvertLookYChanged(bool bIsChecked);

	UFUNCTION()
	void HandleResetDefaultsClicked();

	void BindLocalizationEvents();
	void UnbindLocalizationEvents();
	void ResolveAudioSliders();
	void BindAudioSliders();
	void UnbindAudioSliders();
	void SaveAudioSettings();
	ULCOAudioSettingsSubsystem* GetAudioSettingsSubsystem() const;
	void InitializeAudioSliders();
	void InitializeLanguageOptions();
	void SelectCurrentLanguage();
	void RefreshLocalizedTexts();
	void BindGameSettingsControls();
	void UnbindGameSettingsControls();
	ULCOGameSettingsSubsystem* GetGameSettingsSubsystem() const;
	void InitializeGraphicsOptions();
	void InitializeGameSettingsControls();
	void RefreshGameSettingsValueTexts();
	void RevertUnappliedGraphicsSettings();

	bool bUpdatingAudioSliders = false;
	bool bAudioSettingsDirty = false;
	bool bUpdatingGameSettings = false;
	bool bGraphicsSettingsDirty = false;
	
	TArray<FIntPoint> ResolutionOptions;
};
