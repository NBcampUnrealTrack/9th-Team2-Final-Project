#include "UI/LCOSettingsWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "UI/LCOAudioSettingsSubsystem.h"
#include "Core/LCOGameSettingsSubsystem.h"

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
	
	struct FWindowModeOption
	{
		EWindowMode::Type Mode;
		const TCHAR* TextKey;
	};

	const FWindowModeOption WindowModeOptions[] =
	{
		{ EWindowMode::Fullscreen, TEXT("SETTINGS_WINDOW_FULLSCREEN") },
		{ EWindowMode::WindowedFullscreen, TEXT("SETTINGS_WINDOW_BORDERLESS") },
		{ EWindowMode::Windowed, TEXT("SETTINGS_WINDOW_WINDOWED") }
	};
	
	const TCHAR* QualityOptionKeys[] =
	{
		TEXT("SETTINGS_QUALITY_LOW"),
		TEXT("SETTINGS_QUALITY_MEDIUM"),
		TEXT("SETTINGS_QUALITY_HIGH"),
		TEXT("SETTINGS_QUALITY_EPIC")
	};

	constexpr int32 NumWindowModeOptions = UE_ARRAY_COUNT(WindowModeOptions);
	constexpr int32 NumQualityOptions = UE_ARRAY_COUNT(QualityOptionKeys);
	
	void SelectComboBoxIndex(UComboBoxString* ComboBox, int32 Index)
	{
		if (Index >= 0 && Index < ComboBox->GetOptionCount())
		{
			ComboBox->SetSelectedIndex(Index);
		}
		else
		{
			ComboBox->ClearSelection();
		}
	}

	FString GetResolutionDisplayName(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
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
	InitializeGraphicsOptions();
	InitializeGameSettingsControls();

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
	BindGameSettingsControls();
	BindLocalizationEvents();
}

void ULCOSettingsWidget::NativeDestruct()
{
	SaveAudioSettings();
	RevertUnappliedGraphicsSettings();
	UnbindAudioSliders();
	UnbindGameSettingsControls();
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
	SaveAudioSettings();
	RemoveFromParent();
}

void ULCOSettingsWidget::HandleLanguageChanged(FName NewLanguage)
{
	SelectCurrentLanguage();
	RefreshLocalizedTexts();
	
	InitializeGraphicsOptions();
}

void ULCOSettingsWidget::HandleMasterVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Master slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::Master, Value, false);
		bAudioSettingsDirty = true;
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
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::BGM, Value, false);
		bAudioSettingsDirty = true;
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
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::SFX, Value, false);
		bAudioSettingsDirty = true;
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
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::Voice, Value, false);
		bAudioSettingsDirty = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing audio settings subsystem for Voice"));
	}
}

void ULCOSettingsWidget::HandleAudioSliderCaptureEnd()
{
	SaveAudioSettings();
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
		MasterVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		MasterVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		MasterVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		MasterVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
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
		BGMVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		BGMVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		BGMVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		BGMVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
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
		SFXVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		SFXVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		SFXVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		SFXVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
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
		VoiceVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		VoiceVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		VoiceVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		VoiceVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
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
		MasterVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		MasterVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
	}

	if (BGMVolumeSlider)
	{
		BGMVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBGMVolumeChanged);
		BGMVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		BGMVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
	}

	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		SFXVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
	}

	if (VoiceVolumeSlider)
	{
		VoiceVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleVoiceVolumeChanged);
		VoiceVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		VoiceVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
	}
}

void ULCOSettingsWidget::SaveAudioSettings()
{
	if (!bAudioSettingsDirty)
	{
		return;
	}

	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SaveAudioSettings();
		bAudioSettingsDirty = false;
	}
}

ULCOAudioSettingsSubsystem* ULCOSettingsWidget::GetAudioSettingsSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<ULCOAudioSettingsSubsystem>() : nullptr;
}

void ULCOSettingsWidget::InitializeAudioSliders()
{
	ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem();
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

	if (WindowModeText)
	{
		WindowModeText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_WINDOW_MODE")));
	}

	if (ResolutionText)
	{
		ResolutionText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_RESOLUTION")));
	}

	if (QualityText)
	{
		QualityText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_QUALITY")));
	}

	if (GraphicsApplyButtonText)
	{
		GraphicsApplyButtonText->SetText(LocalizationSubsystem->GetUIText(TEXT("COMMON_APPLY")));
	}

	if (GraphicsCancelButtonText)
	{
		GraphicsCancelButtonText->SetText(LocalizationSubsystem->GetUIText(TEXT("COMMON_CANCEL")));
	}

	if (BrightnessText)
	{
		BrightnessText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_BRIGHTNESS")));
	}

	if (LookSensitivityText)
	{
		LookSensitivityText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_LOOK_SENSITIVITY")));
	}

	if (InvertLookYText)
	{
		InvertLookYText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_INVERT_LOOK_Y")));
	}

	if (ResetDefaultsButtonText)
	{
		ResetDefaultsButtonText->SetText(LocalizationSubsystem->GetUIText(TEXT("SETTINGS_RESET_DEFAULTS")));
	}
}

void ULCOSettingsWidget::HandleWindowModeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingGameSettings || !WindowModeComboBox)
	{
		return;
	}

	const int32 SelectedIndex = WindowModeComboBox->GetSelectedIndex();
	if (SelectedIndex < 0 || SelectedIndex >= NumWindowModeOptions)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Window mode changed: %s"), *SelectedItem);
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetWindowMode(WindowModeOptions[SelectedIndex].Mode);
		bGraphicsSettingsDirty = true;
	}
}

void ULCOSettingsWidget::HandleResolutionSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingGameSettings || !ResolutionComboBox)
	{
		return;
	}

	const int32 SelectedIndex = ResolutionComboBox->GetSelectedIndex();
	if (!ResolutionOptions.IsValidIndex(SelectedIndex))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Resolution changed: %s"), *SelectedItem);
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetResolution(ResolutionOptions[SelectedIndex]);
		bGraphicsSettingsDirty = true;
	}
}

void ULCOSettingsWidget::HandleQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingGameSettings || !QualityComboBox)
	{
		return;
	}

	const int32 SelectedIndex = QualityComboBox->GetSelectedIndex();
	if (SelectedIndex < 0 || SelectedIndex >= NumQualityOptions)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Quality changed: %s"), *SelectedItem);
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetOverallQuality(SelectedIndex);
		bGraphicsSettingsDirty = true;
	}
}

void ULCOSettingsWidget::HandleGraphicsApplyClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Graphics apply clicked"));
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->ApplyGraphicsSettings();
		bGraphicsSettingsDirty = false;
	}

	InitializeGraphicsOptions();
}

void ULCOSettingsWidget::HandleGraphicsCancelClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Graphics cancel clicked"));
	RevertUnappliedGraphicsSettings();
	InitializeGraphicsOptions();
}

void ULCOSettingsWidget::HandleBrightnessChanged(float Value)
{
	if (bUpdatingGameSettings)
	{
		return;
	}

	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetBrightness(Value);
	}
	RefreshGameSettingsValueTexts();
}

void ULCOSettingsWidget::HandleLookSensitivityChanged(float Value)
{
	if (bUpdatingGameSettings)
	{
		return;
	}

	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetLookSensitivity(Value);
	}
	RefreshGameSettingsValueTexts();
}

void ULCOSettingsWidget::HandleInvertLookYChanged(bool bIsChecked)
{
	if (bUpdatingGameSettings)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Invert look Y changed: %s"), bIsChecked ? TEXT("true") : TEXT("false"));
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetInvertLookY(bIsChecked);
	}
}

void ULCOSettingsWidget::HandleResetDefaultsClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Reset defaults clicked"));
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->ResetToDefaults();
		bGraphicsSettingsDirty = false;
	}

	InitializeGraphicsOptions();
	InitializeGameSettingsControls();
}

void ULCOSettingsWidget::BindGameSettingsControls()
{
	if (WindowModeComboBox)
	{
		WindowModeComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleWindowModeSelectionChanged);
		WindowModeComboBox->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleWindowModeSelectionChanged);
	}

	if (ResolutionComboBox)
	{
		ResolutionComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleResolutionSelectionChanged);
		ResolutionComboBox->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleResolutionSelectionChanged);
	}

	if (QualityComboBox)
	{
		QualityComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleQualitySelectionChanged);
		QualityComboBox->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleQualitySelectionChanged);
	}

	if (GraphicsApplyButton)
	{
		GraphicsApplyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleGraphicsApplyClicked);
		GraphicsApplyButton->OnClicked.AddDynamic(this, &ThisClass::HandleGraphicsApplyClicked);
	}

	if (GraphicsCancelButton)
	{
		GraphicsCancelButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleGraphicsCancelClicked);
		GraphicsCancelButton->OnClicked.AddDynamic(this, &ThisClass::HandleGraphicsCancelClicked);
	}

	if (BrightnessSlider)
	{
		BrightnessSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBrightnessChanged);
		BrightnessSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleBrightnessChanged);
	}

	if (LookSensitivitySlider)
	{
		LookSensitivitySlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleLookSensitivityChanged);
		LookSensitivitySlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleLookSensitivityChanged);
	}

	if (InvertLookYCheckBox)
	{
		InvertLookYCheckBox->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::HandleInvertLookYChanged);
		InvertLookYCheckBox->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleInvertLookYChanged);
	}

	if (ResetDefaultsButton)
	{
		ResetDefaultsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetDefaultsClicked);
		ResetDefaultsButton->OnClicked.AddDynamic(this, &ThisClass::HandleResetDefaultsClicked);
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Game settings bound: WindowMode=%s Resolution=%s Quality=%s Brightness=%s Sensitivity=%s InvertY=%s"),
		WindowModeComboBox ? TEXT("O") : TEXT("X"), ResolutionComboBox ? TEXT("O") : TEXT("X"),
		QualityComboBox ? TEXT("O") : TEXT("X"), BrightnessSlider ? TEXT("O") : TEXT("X"),
		LookSensitivitySlider ? TEXT("O") : TEXT("X"), InvertLookYCheckBox ? TEXT("O") : TEXT("X"));
}

void ULCOSettingsWidget::UnbindGameSettingsControls()
{
	if (WindowModeComboBox)
	{
		WindowModeComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleWindowModeSelectionChanged);
	}

	if (ResolutionComboBox)
	{
		ResolutionComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleResolutionSelectionChanged);
	}

	if (QualityComboBox)
	{
		QualityComboBox->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleQualitySelectionChanged);
	}

	if (GraphicsApplyButton)
	{
		GraphicsApplyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleGraphicsApplyClicked);
	}

	if (GraphicsCancelButton)
	{
		GraphicsCancelButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleGraphicsCancelClicked);
	}

	if (BrightnessSlider)
	{
		BrightnessSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBrightnessChanged);
	}

	if (LookSensitivitySlider)
	{
		LookSensitivitySlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleLookSensitivityChanged);
	}

	if (InvertLookYCheckBox)
	{
		InvertLookYCheckBox->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::HandleInvertLookYChanged);
	}

	if (ResetDefaultsButton)
	{
		ResetDefaultsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetDefaultsClicked);
	}
}

ULCOGameSettingsSubsystem* ULCOSettingsWidget::GetGameSettingsSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<ULCOGameSettingsSubsystem>() : nullptr;
}

void ULCOSettingsWidget::InitializeGraphicsOptions()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing game settings subsystem during graphics init"));
		return;
	}

	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();

	bUpdatingGameSettings = true;
	
	if (WindowModeComboBox)
	{
		WindowModeComboBox->ClearOptions();
		int32 SelectedIndex = INDEX_NONE;
		const EWindowMode::Type CurrentMode = GameSettings->GetWindowMode();
		for (int32 Index = 0; Index < NumWindowModeOptions; ++Index)
		{
			const FString OptionName = LocalizationSubsystem
				? LocalizationSubsystem->GetUIText(WindowModeOptions[Index].TextKey).ToString()
				: FString(WindowModeOptions[Index].TextKey);
			WindowModeComboBox->AddOption(OptionName);

			if (WindowModeOptions[Index].Mode == CurrentMode)
			{
				SelectedIndex = Index;
			}
		}
		SelectComboBoxIndex(WindowModeComboBox, SelectedIndex);
	}
	
	if (ResolutionComboBox)
	{
		ResolutionComboBox->ClearOptions();
		ResolutionOptions = GameSettings->GetSupportedResolutions();

		const FIntPoint CurrentResolution = GameSettings->GetResolution();
		if (!ResolutionOptions.Contains(CurrentResolution))
		{
			ResolutionOptions.Add(CurrentResolution);
		}

		for (const FIntPoint& Resolution : ResolutionOptions)
		{
			ResolutionComboBox->AddOption(GetResolutionDisplayName(Resolution));
		}
		SelectComboBoxIndex(ResolutionComboBox, ResolutionOptions.IndexOfByKey(CurrentResolution));
	}

	if (QualityComboBox)
	{
		QualityComboBox->ClearOptions();
		for (const TCHAR* TextKey : QualityOptionKeys)
		{
			const FString OptionName = LocalizationSubsystem
				? LocalizationSubsystem->GetUIText(TextKey).ToString()
				: FString(TextKey);
			QualityComboBox->AddOption(OptionName);
		}

		const int32 CurrentQuality = GameSettings->GetOverallQuality();
		SelectComboBoxIndex(QualityComboBox, CurrentQuality);
	}

	bUpdatingGameSettings = false;
}

void ULCOSettingsWidget::InitializeGameSettingsControls()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSettingsWidget] Missing game settings subsystem during gameplay init"));
		return;
	}

	bUpdatingGameSettings = true;
	if (BrightnessSlider)
	{
		BrightnessSlider->SetMinValue(ULCOGameSettingsSubsystem::MinBrightness);
		BrightnessSlider->SetMaxValue(ULCOGameSettingsSubsystem::MaxBrightness);
		BrightnessSlider->SetStepSize(0.05f);
		BrightnessSlider->SetValue(GameSettings->GetBrightness());
	}
	if (LookSensitivitySlider)
	{
		LookSensitivitySlider->SetMinValue(ULCOGameSettingsSubsystem::MinLookSensitivity);
		LookSensitivitySlider->SetMaxValue(ULCOGameSettingsSubsystem::MaxLookSensitivity);
		LookSensitivitySlider->SetStepSize(0.1f);
		LookSensitivitySlider->SetValue(GameSettings->GetLookSensitivity());
	}
	if (InvertLookYCheckBox)
	{
		InvertLookYCheckBox->SetIsChecked(GameSettings->IsInvertLookY());
	}
	bUpdatingGameSettings = false;

	RefreshGameSettingsValueTexts();
}

void ULCOSettingsWidget::RefreshGameSettingsValueTexts()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings)
	{
		return;
	}

	if (BrightnessValueText)
	{
		BrightnessValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), GameSettings->GetBrightness())));
	}

	if (LookSensitivityValueText)
	{
		LookSensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), GameSettings->GetLookSensitivity())));
	}
}

void ULCOSettingsWidget::RevertUnappliedGraphicsSettings()
{
	if (!bGraphicsSettingsDirty)
	{
		return;
	}
	
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->RevertGraphicsSettings();
		bGraphicsSettingsDirty = false;
	}
}
