#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "LCOSettingsWidget.generated.h"

class UButton;
class UTextBlock;

// 설정창 본체 (제목 / 언어 / 탭 버튼 글자 / 뒤로 가기)
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
	TObjectPtr<UTextBlock> SettingsTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LanguageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GameplayText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GraphicsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AudioText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ControlsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BackButtonText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

private:
	UFUNCTION()
	void HandleLanguageSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	void BindLocalizationEvents();
	void UnbindLocalizationEvents();
	void InitializeLanguageOptions();
	void SelectCurrentLanguage();
	void RefreshLocalizedTexts();
};
