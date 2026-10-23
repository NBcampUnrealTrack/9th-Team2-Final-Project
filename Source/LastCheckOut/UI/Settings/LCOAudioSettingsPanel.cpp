#include "UI/Settings/LCOAudioSettingsPanel.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/LCOAudioSettingsSubsystem.h"

void ULCOAudioSettingsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveAudioSliders();
	InitializeAudioSliders();
	BindAudioSliders();
	RefreshLocalizedTexts();
}

void ULCOAudioSettingsPanel::NativeDestruct()
{
	SaveAudioSettings();
	UnbindAudioSliders();

	Super::NativeDestruct();
}

void ULCOAudioSettingsPanel::RefreshLocalizedTexts()
{
	SetUIText(AudioTitleText, TEXT("SETTINGS_AUDIO_TITLE"));
	SetUIText(MasterVolumeText, TEXT("SETTINGS_MASTER_VOLUME"));
	SetUIText(BGMVolumeText, TEXT("SETTINGS_BGM_VOLUME"));
	SetUIText(SFXVolumeText, TEXT("SETTINGS_SFX_VOLUME"));
	SetUIText(VoiceVolumeText, TEXT("SETTINGS_VOICE_VOLUME"));
}

void ULCOAudioSettingsPanel::HandleMasterVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Master slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::Master, Value, false);
		bAudioSettingsDirty = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing audio settings subsystem for Master"));
	}
}

void ULCOAudioSettingsPanel::HandleBGMVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] BGM slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::BGM, Value, false);
		bAudioSettingsDirty = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing audio settings subsystem for BGM"));
	}
}

void ULCOAudioSettingsPanel::HandleSFXVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] SFX slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::SFX, Value, false);
		bAudioSettingsDirty = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing audio settings subsystem for SFX"));
	}
}

void ULCOAudioSettingsPanel::HandleVoiceVolumeChanged(float Value)
{
	if (bUpdatingAudioSliders)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Voice slider changed: %.3f"), Value);
	if (ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem())
	{
		AudioSettings->SetVolume(ELCOAudioVolumeChannel::Voice, Value, false);
		bAudioSettingsDirty = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing audio settings subsystem for Voice"));
	}
}

void ULCOAudioSettingsPanel::HandleAudioSliderCaptureEnd()
{
	SaveAudioSettings();
}

void ULCOAudioSettingsPanel::ResolveAudioSliders()
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

void ULCOAudioSettingsPanel::BindAudioSliders()
{
	if (MasterVolumeSlider)
	{
		MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleMasterVolumeChanged);
		MasterVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleMasterVolumeChanged);
		MasterVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		MasterVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		MasterVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		MasterVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Bound MasterVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing MasterVolumeSlider binding"));
	}

	if (BGMVolumeSlider)
	{
		BGMVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleBGMVolumeChanged);
		BGMVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleBGMVolumeChanged);
		BGMVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		BGMVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		BGMVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		BGMVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Bound BGMVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing BGMVolumeSlider binding"));
	}

	if (SFXVolumeSlider)
	{
		SFXVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleSFXVolumeChanged);
		SFXVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		SFXVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		SFXVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		SFXVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Bound SFXVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing SFXVolumeSlider binding"));
	}

	if (VoiceVolumeSlider)
	{
		VoiceVolumeSlider->OnValueChanged.RemoveDynamic(this, &ThisClass::HandleVoiceVolumeChanged);
		VoiceVolumeSlider->OnValueChanged.AddDynamic(this, &ThisClass::HandleVoiceVolumeChanged);
		VoiceVolumeSlider->OnMouseCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		VoiceVolumeSlider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		VoiceVolumeSlider->OnControllerCaptureEnd.RemoveDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		VoiceVolumeSlider->OnControllerCaptureEnd.AddDynamic(this, &ThisClass::HandleAudioSliderCaptureEnd);
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Bound VoiceVolumeSlider"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing VoiceVolumeSlider binding"));
	}
}

void ULCOAudioSettingsPanel::UnbindAudioSliders()
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

void ULCOAudioSettingsPanel::SaveAudioSettings()
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

ULCOAudioSettingsSubsystem* ULCOAudioSettingsPanel::GetAudioSettingsSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<ULCOAudioSettingsSubsystem>() : nullptr;
}

void ULCOAudioSettingsPanel::InitializeAudioSliders()
{
	ULCOAudioSettingsSubsystem* AudioSettings = GetAudioSettingsSubsystem();
	if (!AudioSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Missing audio settings subsystem during slider init"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettingsPanel] Initializing audio sliders and applying saved settings"));
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
