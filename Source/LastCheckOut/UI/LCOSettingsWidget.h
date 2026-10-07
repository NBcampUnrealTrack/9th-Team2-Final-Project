#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "LCOSettingsWidget.generated.h"

UCLASS()
class LASTCHECKOUT_API ULCOSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

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

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ControlsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ControlsTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> BackButtonText;

private:
	UFUNCTION()
	void HandleLanguageSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void InitializeLanguageOptions();
	void SelectCurrentLanguage();
	void RefreshLocalizedTexts();
};
