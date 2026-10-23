#include "UI/Settings/LCOGameplaySettingsPanel.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Core/LCOGameSettingsSubsystem.h"

void ULCOGameplaySettingsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeControls();
	BindControls();
	RefreshLocalizedTexts();

	UE_LOG(LogTemp, Warning, TEXT("[LCOGameplaySettingsPanel] Bound: Sensitivity=%s InvertY=%s ResetDefaults=%s"),
		LookSensitivitySlider ? TEXT("O") : TEXT("X"), InvertLookYCheckBox ? TEXT("O") : TEXT("X"),
		ResetDefaultsButton ? TEXT("O") : TEXT("X"));
}

void ULCOGameplaySettingsPanel::NativeDestruct()
{
	UnbindControls();

	Super::NativeDestruct();
}

void ULCOGameplaySettingsPanel::RefreshLocalizedTexts()
{
	SetUIText(GameplayTitleText, TEXT("SETTINGS_GAMEPLAY_TITLE"));
	SetUIText(LookSensitivityText, TEXT("SETTINGS_LOOK_SENSITIVITY"));
	SetUIText(InvertLookYText, TEXT("SETTINGS_INVERT_LOOK_Y"));
	SetUIText(ResetDefaultsButtonText, TEXT("SETTINGS_RESET_DEFAULTS"));
}

void ULCOGameplaySettingsPanel::HandleLookSensitivityChanged(float Value)
{
	if (bUpdatingControls)
	{
		return;
	}

	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetLookSensitivity(Value);
	}
	RefreshSensitivityValueText();
}

void ULCOGameplaySettingsPanel::HandleInvertLookYChanged(bool bIsChecked)
{
	if (bUpdatingControls)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOGameplaySettingsPanel] Invert look Y changed: %s"), bIsChecked ? TEXT("true") : TEXT("false"));
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetInvertLookY(bIsChecked);
	}
}

void ULCOGameplaySettingsPanel::HandleResetDefaultsClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOGameplaySettingsPanel] Reset defaults clicked"));

	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->ResetToDefaults();
	}

	InitializeControls();
}

void ULCOGameplaySettingsPanel::BindControls()
{
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
}

void ULCOGameplaySettingsPanel::UnbindControls()
{
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

void ULCOGameplaySettingsPanel::InitializeControls()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOGameplaySettingsPanel] Missing game settings subsystem"));
		return;
	}

	bUpdatingControls = true;
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
	bUpdatingControls = false;

	RefreshSensitivityValueText();
}

void ULCOGameplaySettingsPanel::RefreshSensitivityValueText()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (GameSettings && LookSensitivityValueText)
	{
		LookSensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), GameSettings->GetLookSensitivity())));
	}
}
