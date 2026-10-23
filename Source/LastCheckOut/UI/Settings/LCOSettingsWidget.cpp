#include "UI/Settings/LCOSettingsWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"

namespace
{
	struct FLanguageOption
	{
		const TCHAR* DisplayName;
		const TCHAR* LanguageCode;
	};

	const FLanguageOption LanguageOptions[] =
	{
		{ TEXT("한국어"), TEXT("ko") },
		{ TEXT("English"), TEXT("en") },
		{ TEXT("日本語"), TEXT("ja") },
		{ TEXT("简体中文"), TEXT("zh-CN") },
		{ TEXT("繁體中文"), TEXT("zh-TW") },
		{ TEXT("Español"), TEXT("es") },
		{ TEXT("Français"), TEXT("fr") },
		{ TEXT("Deutsch"), TEXT("de") },
		{ TEXT("Português (Brasil)"), TEXT("pt-BR") },
		{ TEXT("Suomi"), TEXT("fi") }
	};

	const TCHAR* GetDisplayNameForLanguage(FName LanguageCode)
	{
		for (const FLanguageOption& Option : LanguageOptions)
		{
			if (LanguageCode == FName(Option.LanguageCode))
			{
				return Option.DisplayName;
			}
		}

		return LanguageOptions[0].DisplayName;
	}
}

void ULCOSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeLanguageOptions();
	SelectCurrentLanguage();
	RefreshLocalizedTexts();

	if (LanguageComboBox)
	{
		LanguageComboBox->OnSelectionChanged.RemoveDynamic(this, &ULCOSettingsWidget::HandleLanguageSelectionChanged);
		LanguageComboBox->OnSelectionChanged.AddDynamic(this, &ULCOSettingsWidget::HandleLanguageSelectionChanged);
	}

	if (BackButton)
	{
		BackButton->OnClicked.RemoveDynamic(this, &ULCOSettingsWidget::HandleBackClicked);
		BackButton->OnClicked.AddDynamic(this, &ULCOSettingsWidget::HandleBackClicked);
	}

	BindLocalizationEvents();
}

void ULCOSettingsWidget::NativeDestruct()
{
	UnbindLocalizationEvents();

	Super::NativeDestruct();
}

void ULCOSettingsWidget::HandleBackClicked()
{
	RemoveFromParent();
}

void ULCOSettingsWidget::HandleLanguageChanged(FName NewLanguage)
{
	SelectCurrentLanguage();
	RefreshLocalizedTexts();
}

void ULCOSettingsWidget::BindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOSettingsWidget::UnbindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOSettingsWidget::HandleLanguageSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();
	if (!LocalizationSubsystem)
	{
		return;
	}

	for (const FLanguageOption& Option : LanguageOptions)
	{
		if (SelectedItem == Option.DisplayName)
		{
			LocalizationSubsystem->SetLanguage(FName(Option.LanguageCode));
			break;
		}
	}

}

void ULCOSettingsWidget::InitializeLanguageOptions()
{
	if (!LanguageComboBox)
	{
		return;
	}

	LanguageComboBox->ClearOptions();
	for (const FLanguageOption& Option : LanguageOptions)
	{
		LanguageComboBox->AddOption(FString(Option.DisplayName));
	}
}

void ULCOSettingsWidget::SelectCurrentLanguage()
{
	if (!LanguageComboBox)
	{
		return;
	}

	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();
	if (!LocalizationSubsystem)
	{
		LanguageComboBox->SetSelectedOption(FString(LanguageOptions[0].DisplayName));
		return;
	}

	LanguageComboBox->SetSelectedOption(FString(GetDisplayNameForLanguage(LocalizationSubsystem->GetLanguage())));
}

void ULCOSettingsWidget::RefreshLocalizedTexts()
{
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();
	if (!LocalizationSubsystem)
	{
		return;
	}

	if (SettingsTitleText)
	{
		SettingsTitleText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_TITLE")));
	}

	if (LanguageText)
	{
		LanguageText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_LANGUAGE")));
	}

	if (GameplayText)
	{
		GameplayText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_GAMEPLAY")));
	}

	if (GraphicsText)
	{
		GraphicsText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_GRAPHICS")));
	}

	if (AudioText)
	{
		AudioText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_AUDIO")));
	}

	if (ControlsText)
	{
		ControlsText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_CONTROLS")));
	}

	if (BackButtonText)
	{
		BackButtonText->SetText(LocalizationSubsystem->GetUIText(TEXT("COMMON_BACK")));
	}
}
