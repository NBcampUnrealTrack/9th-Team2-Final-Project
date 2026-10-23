#include "UI/Settings/LCOGraphicsSettingsPanel.h"

#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Core/LCOGameSettingsSubsystem.h"
#include "UI/Settings/LCOGraphicsConfirmWidget.h"

namespace
{
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
	constexpr int32 ConfirmWidgetZOrder = 100;

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

void ULCOGraphicsSettingsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeGraphicsOptions();
	InitializeBrightness();
	BindControls();
	RefreshLocalizedTexts();

	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsSettingsPanel] Bound: WindowMode=%s Resolution=%s Quality=%s Brightness=%s ConfirmClass=%s"),
		WindowModeComboBox ? TEXT("O") : TEXT("X"), ResolutionComboBox ? TEXT("O") : TEXT("X"),
		QualityComboBox ? TEXT("O") : TEXT("X"), BrightnessSlider ? TEXT("O") : TEXT("X"),
		GraphicsConfirmWidgetClass ? TEXT("O") : TEXT("X"));
}

void ULCOGraphicsSettingsPanel::NativeDestruct()
{
	if (ConfirmWidget)
	{
		ConfirmWidget->OnFinished.RemoveDynamic(this, &ThisClass::HandleGraphicsConfirmFinished);
		ConfirmWidget->RevertAndClose();
		ConfirmWidget = nullptr;
	}

	RevertUnappliedGraphicsSettings();
	UnbindControls();

	Super::NativeDestruct();
}

void ULCOGraphicsSettingsPanel::RefreshLocalizedTexts()
{
	SetUIText(GraphicsTitleText, TEXT("SETTINGS_GRAPHICS_TITLE"));
	SetUIText(WindowModeText, TEXT("SETTINGS_WINDOW_MODE"));
	SetUIText(ResolutionText, TEXT("SETTINGS_RESOLUTION"));
	SetUIText(QualityText, TEXT("SETTINGS_QUALITY"));
	SetUIText(GraphicsApplyButtonText, TEXT("COMMON_APPLY"));
	SetUIText(GraphicsCancelButtonText, TEXT("COMMON_CANCEL"));
	SetUIText(BrightnessText, TEXT("SETTINGS_BRIGHTNESS"));
	
	InitializeGraphicsOptions();
}

void ULCOGraphicsSettingsPanel::HandleWindowModeSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls || !WindowModeComboBox)
	{
		return;
	}

	const int32 SelectedIndex = WindowModeComboBox->GetSelectedIndex();
	if (SelectedIndex < 0 || SelectedIndex >= NumWindowModeOptions)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsSettingsPanel] Window mode changed: %s"), *SelectedItem);
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetWindowMode(WindowModeOptions[SelectedIndex].Mode);
		bGraphicsSettingsDirty = true;
	}
}

void ULCOGraphicsSettingsPanel::HandleResolutionSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls || !ResolutionComboBox)
	{
		return;
	}

	const int32 SelectedIndex = ResolutionComboBox->GetSelectedIndex();
	if (!ResolutionOptions.IsValidIndex(SelectedIndex))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsSettingsPanel] Resolution changed: %s"), *SelectedItem);
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetResolution(ResolutionOptions[SelectedIndex]);
		bGraphicsSettingsDirty = true;
	}
}

void ULCOGraphicsSettingsPanel::HandleQualitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bUpdatingControls || !QualityComboBox)
	{
		return;
	}

	const int32 SelectedIndex = QualityComboBox->GetSelectedIndex();
	if (SelectedIndex < 0 || SelectedIndex >= NumQualityOptions)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsSettingsPanel] Quality changed: %s"), *SelectedItem);
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->SetOverallQuality(SelectedIndex);
		bGraphicsSettingsDirty = true;
	}
}

void ULCOGraphicsSettingsPanel::HandleGraphicsApplyClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsSettingsPanel] Apply clicked"));
	ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings || ConfirmWidget)
	{
		return;
	}

	bGraphicsSettingsDirty = false;
	bChangingSettings = true;

	if (GraphicsConfirmWidgetClass)
	{
		GameSettings->ApplyGraphicsSettingsForConfirm();

		ConfirmWidget = CreateWidget<ULCOGraphicsConfirmWidget>(GetOwningPlayer(), GraphicsConfirmWidgetClass);
		if (ConfirmWidget)
		{
			ConfirmWidget->OnFinished.AddDynamic(this, &ThisClass::HandleGraphicsConfirmFinished);
			ConfirmWidget->AddToViewport(ConfirmWidgetZOrder);
		}
		else
		{
			GameSettings->ConfirmGraphicsSettings();
		}
	}
	else
	{
		GameSettings->ApplyGraphicsSettings();
	}

	bChangingSettings = false;
	
	InitializeGraphicsOptions();
}

void ULCOGraphicsSettingsPanel::HandleGraphicsCancelClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsSettingsPanel] Cancel clicked"));
	RevertUnappliedGraphicsSettings();
	InitializeGraphicsOptions();
}

void ULCOGraphicsSettingsPanel::HandleBrightnessChanged(float Value)
{
	if (bUpdatingControls)
	{
		return;
	}

	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		bChangingSettings = true;
		GameSettings->SetBrightness(Value);
		bChangingSettings = false;
	}
	RefreshBrightnessValueText();
}

void ULCOGraphicsSettingsPanel::HandleGraphicsConfirmFinished(bool bKept)
{
	if (ConfirmWidget)
	{
		ConfirmWidget->OnFinished.RemoveDynamic(this, &ThisClass::HandleGraphicsConfirmFinished);
		ConfirmWidget = nullptr;
	}

	InitializeGraphicsOptions();
}

void ULCOGraphicsSettingsPanel::HandleGameSettingsChanged()
{
	if (bChangingSettings || bGraphicsSettingsDirty || ConfirmWidget)
	{
		return;
	}

	InitializeGraphicsOptions();
	InitializeBrightness();
}

void ULCOGraphicsSettingsPanel::BindControls()
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

	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->OnSettingsChanged.RemoveDynamic(this, &ThisClass::HandleGameSettingsChanged);
		GameSettings->OnSettingsChanged.AddDynamic(this, &ThisClass::HandleGameSettingsChanged);
	}
}

void ULCOGraphicsSettingsPanel::UnbindControls()
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

	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->OnSettingsChanged.RemoveDynamic(this, &ThisClass::HandleGameSettingsChanged);
	}
}

void ULCOGraphicsSettingsPanel::InitializeGraphicsOptions()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsSettingsPanel] Missing game settings subsystem"));
		return;
	}

	bUpdatingControls = true;

	if (WindowModeComboBox)
	{
		WindowModeComboBox->ClearOptions();
		int32 SelectedIndex = INDEX_NONE;
		const EWindowMode::Type CurrentMode = GameSettings->GetWindowMode();
		for (int32 Index = 0; Index < NumWindowModeOptions; ++Index)
		{
			WindowModeComboBox->AddOption(GetUIText(WindowModeOptions[Index].TextKey).ToString());
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
			QualityComboBox->AddOption(GetUIText(TextKey).ToString());
		}
		SelectComboBoxIndex(QualityComboBox, GameSettings->GetOverallQuality());
	}

	bUpdatingControls = false;
}

void ULCOGraphicsSettingsPanel::InitializeBrightness()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings || !BrightnessSlider)
	{
		return;
	}

	bUpdatingControls = true;
	BrightnessSlider->SetMinValue(ULCOGameSettingsSubsystem::MinBrightness);
	BrightnessSlider->SetMaxValue(ULCOGameSettingsSubsystem::MaxBrightness);
	BrightnessSlider->SetStepSize(0.05f);
	BrightnessSlider->SetValue(GameSettings->GetBrightness());
	bUpdatingControls = false;

	RefreshBrightnessValueText();
}

void ULCOGraphicsSettingsPanel::RefreshBrightnessValueText()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (GameSettings && BrightnessValueText)
	{
		BrightnessValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), GameSettings->GetBrightness())));
	}
}

void ULCOGraphicsSettingsPanel::RevertUnappliedGraphicsSettings()
{
	if (!bGraphicsSettingsDirty)
	{
		return;
	}
	
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		bChangingSettings = true;
		GameSettings->RevertGraphicsSettings();
		bChangingSettings = false;
		bGraphicsSettingsDirty = false;
	}
}
