#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "LCOSettingsWidget.generated.h"

class UButton;
class USlider;
class ULCOAudioSettingsSubsystem;

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

	bool bUpdatingAudioSliders = false;
	bool bAudioSettingsDirty = false;
};
