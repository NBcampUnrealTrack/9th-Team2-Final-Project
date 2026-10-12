#include "UI/LCOSettingsWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "UI/LCOAudioSettingsSubsystem.h"

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
	ResolveAudioSliders();
	InitializeAudioSliders();
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

	BindAudioSliders();
	BindLocalizationEvents();
}

void ULCOSettingsWidget::NativeDestruct()
{
	UnbindAudioSliders();
	UnbindLocalizationEvents();

	Super::NativeDestruct();
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

void ULCOSettingsWidget::HandleBackClicked()
{
	RemoveFromParent();
}

void ULCOSettingsWidget::HandleLanguageChanged(FName NewLanguage)
{
	SelectCurrentLanguage();
	RefreshLocalizedTexts();
}

void ULCOSettingsWidget::HandleMasterVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Master slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetGameInstance()->GetSubsystem<ULCOAudioSettingsSubsystem>())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::Master, Value);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing audio settings subsystem for Master"));
	}
}

void ULCOSettingsWidget::HandleBGMVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] BGM slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetGameInstance()->GetSubsystem<ULCOAudioSettingsSubsystem>())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::BGM, Value);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing audio settings subsystem for BGM"));
	}
}

void ULCOSettingsWidget::HandleSFXVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] SFX slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetGameInstance()->GetSubsystem<ULCOAudioSettingsSubsystem>())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::SFX, Value);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing audio settings subsystem for SFX"));
	}
}

void ULCOSettingsWidget::HandleVoiceVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Voice slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetGameInstance()->GetSubsystem<ULCOAudioSettingsSubsystem>())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::Voice, Value);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing audio settings subsystem for Voice"));
	}
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

void ULCOSettingsWidget::ResolveAudioSliders()
{
	if (!MasterVolumeSlider)
	{
		MasterVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("Slider_MasterVolume")));
	}
	if (!MasterVolumeSlider)
	{
		MasterVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("MasterSlider")));
	}

	if (!BGMVolumeSlider)
	{
		BGMVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("Slider_BGMVolume")));
	}
	if (!BGMVolumeSlider)
	{
		BGMVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("BGMSlider")));
	}

	if (!SFXVolumeSlider)
	{
		SFXVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("Slider_SFXVolume")));
	}
	if (!SFXVolumeSlider)
	{
		SFXVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("SFXSlider")));
	}

	if (!VoiceVolumeSlider)
	{
		VoiceVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("Slider_VoiceVolume")));
	}
	if (!VoiceVolumeSlider)
	{
		VoiceVolumeSlider = Cast<USlider>(GetWidgetFromName(TEXT("VoiceSlider")));
	}
}

void ULCOSettingsWidget::BindAudioSliders()
{
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleMasterVolumeChanged);
		MasterVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleMasterVolumeChanged);
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Bound MasterVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing MasterVolumeSlider binding"));
	}

	if (BGMVolumeSlider)
	{
		BGMVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBGMVolumeChanged);
		BGMVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleBGMVolumeChanged);
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Bound BGMVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing BGMVolumeSlider binding"));
	}

	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSFXVolumeChanged);
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Bound SFXVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing SFXVolumeSlider binding"));
	}

	if (VoiceVolumeSlider)
	{
		VoiceVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleVoiceVolumeChanged);
		VoiceVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleVoiceVolumeChanged);
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Bound VoiceVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing VoiceVolumeSlider binding"));
	}
}

void ULCOSettingsWidget::UnbindAudioSliders()
{
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleMasterVolumeChanged);
	}

	if (BGMVolumeSlider)
	{
		BGMVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBGMVolumeChanged);
	}

	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSFXVolumeChanged);
	}

	if (VoiceVolumeSlider)
	{
		VoiceVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleVoiceVolumeChanged);
	}
}

void ULCOSettingsWidget::InitializeAudioSliders()
{
	ULCOAudioSettingsSubsystem* AudioSettings = GetGameInstance()->GetSubsystem<ULCOAudioSettingsSubsystem>();
	if (!AudioSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing audio settings subsystem during slider init"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Initializing audio sliders and applying saved settings"));
	AudioSettings->ApplyAudioSettings();

	bUpdatingAudioSliders = true;
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->SetMinValue(0.0f);
		MasterVolumeSlider->SetMaxValue(1.0f);
		MasterVolumeSlider->SetValue(AudioSettings->GetVolume(ELCOAudioVolumeChannel::Master));
	}
	if (BGMVolumeSlider)
	{
		BGMVolumeSlider->SetMinValue(0.0f);
		BGMVolumeSlider->SetMaxValue(1.0f);
		BGMVolumeSlider->SetValue(AudioSettings->GetVolume(ELCOAudioVolumeChannel::BGM));
	}
	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->SetMinValue(0.0f);
		SFXVolumeSlider->SetMaxValue(1.0f);
		SFXVolumeSlider->SetValue(AudioSettings->GetVolume(ELCOAudioVolumeChannel::SFX));
	}
	if (VoiceVolumeSlider)
	{
		VoiceVolumeSlider->SetMinValue(0.0f);
		VoiceVolumeSlider->SetMaxValue(1.0f);
		VoiceVolumeSlider->SetValue(AudioSettings->GetVolume(ELCOAudioVolumeChannel::Voice));
	}
	bUpdatingAudioSliders = false;
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

	if (GameplayTitleText)
	{
		GameplayTitleText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_GAMEPLAY_TITLE")));
	}

	if (GraphicsText)
	{
		GraphicsText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_GRAPHICS")));
	}

	if (GraphicsTitleText)
	{
		GraphicsTitleText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_GRAPHICS_TITLE")));
	}

	if (AudioText)
	{
		AudioText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_AUDIO")));
	}

	if (AudioTitleText)
	{
		AudioTitleText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_AUDIO_TITLE")));
	}

	if (MasterVolumeText)
	{
		MasterVolumeText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_MASTER_VOLUME")));
	}

	if (BGMVolumeText)
	{
		BGMVolumeText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_BGM_VOLUME")));
	}

	if (SFXVolumeText)
	{
		SFXVolumeText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_SFX_VOLUME")));
	}

	if (VoiceVolumeText)
	{
		VoiceVolumeText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_VOICE_VOLUME")));
	}

	if (ControlsText)
	{
		ControlsText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_CONTROLS")));
	}

	if (ControlsTitleText)
	{
		ControlsTitleText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_CONTROLS_TITLE")));
	}

	if (BackButtonText)
	{
		BackButtonText->SetText(LocalizationSubsystem->GetUIText(TEXT("COMMON_BACK")));
	}
}
