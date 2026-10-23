#include "UI/Settings/LCOControlsSettingsPanel.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/LCOGameSettingsSubsystem.h"
#include "InputMappingContext.h"
#include "UI/Settings/LCOKeyBindingRowWidget.h"

void ULCOControlsSettingsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	RegisterKeyBindingMappingContexts();
	SetKeyBindingMessage(FText::GetEmpty());
	RefreshLocalizedTexts();

	if (ResetKeysButton)
	{
		ResetKeysButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetKeysClicked);
		ResetKeysButton->OnClicked.AddDynamic(this, &ThisClass::HandleResetKeysClicked);
	}
}

void ULCOControlsSettingsPanel::NativeDestruct()
{
	if (ResetKeysButton)
	{
		ResetKeysButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetKeysClicked);
	}
	UnbindKeyBindingRows();

	Super::NativeDestruct();
}

void ULCOControlsSettingsPanel::RefreshLocalizedTexts()
{
	SetUIText(ControlsTitleText, TEXT("SETTINGS_CONTROLS_TITLE"));
	SetUIText(ResetKeysButtonText, TEXT("SETTINGS_RESET_KEYS"));
	
	RefreshKeyBindings();
}

void ULCOControlsSettingsPanel::HandleKeyBindingRowKeySelected(FName MappingName, FKey NewKey)
{
	ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings)
	{
		return;
	}

	FName SwappedMappingName;
	const ELCOKeyRemapResult Result = GameSettings->RemapKey(MappingName, NewKey, SwappedMappingName);
	UE_LOG(LogTemp, Warning, TEXT("[LCOControlsSettingsPanel] Key remap: %s -> %s (Result = %d, Swapped = %s)"),
		*MappingName.ToString(), *NewKey.ToString(), static_cast<int32>(Result), *SwappedMappingName.ToString());

	switch (Result)
	{
	case ELCOKeyRemapResult::Swapped:
		SetKeyBindingMessage(FText::Format(FTextFormat(GetUIText(TEXT("KEYBIND_SWAPPED"))),
			GetKeyBindingDisplayName(SwappedMappingName, FText::FromName(SwappedMappingName))));
		break;

	case ELCOKeyRemapResult::Blocked:
		SetKeyBindingMessage(GetUIText(TEXT("KEYBIND_BLOCKED")));
		break;

	case ELCOKeyRemapResult::Failed:
		SetKeyBindingMessage(GetUIText(TEXT("KEYBIND_FAILED")));
		break;

	default:
		SetKeyBindingMessage(FText::GetEmpty());
		break;
	}
	
	RefreshKeyBindings();
}

void ULCOControlsSettingsPanel::HandleResetKeysClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOControlsSettingsPanel] Reset keys clicked"));
	if (ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem())
	{
		GameSettings->ResetKeysToDefault();
	}

	SetKeyBindingMessage(FText::GetEmpty());
	RefreshKeyBindings();
}

void ULCOControlsSettingsPanel::RegisterKeyBindingMappingContexts()
{
	ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!GameSettings)
	{
		return;
	}

	for (UInputMappingContext* MappingContext : KeyBindingMappingContexts)
	{
		GameSettings->RegisterKeyMappingContext(MappingContext);
	}
}

void ULCOControlsSettingsPanel::RefreshKeyBindings()
{
	const ULCOGameSettingsSubsystem* GameSettings = GetGameSettingsSubsystem();
	if (!KeyBindingList || !GameSettings)
	{
		return;
	}

	if (!KeyBindingRowClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOControlsSettingsPanel] KeyBindingRowClass is not set"));
		return;
	}

	const TArray<FLCOKeyMappingInfo> Mappings = GameSettings->GetKeyMappings();
	const FText KeySelectionText = GetUIText(TEXT("KEYBIND_PRESS_KEY"));
	
	if (KeyBindingRows.Num() != Mappings.Num())
	{
		UnbindKeyBindingRows();
		KeyBindingRows.Reset();
		KeyBindingList->ClearChildren();

		for (int32 Index = 0; Index < Mappings.Num(); ++Index)
		{
			ULCOKeyBindingRowWidget* Row = CreateWidget<ULCOKeyBindingRowWidget>(this, KeyBindingRowClass);
			if (!Row)
			{
				continue;
			}

			Row->OnKeySelected.AddDynamic(this, &ThisClass::HandleKeyBindingRowKeySelected);
			KeyBindingList->AddChild(Row);
			KeyBindingRows.Add(Row);
		}
	}

	for (int32 Index = 0; Index < Mappings.Num() && Index < KeyBindingRows.Num(); ++Index)
	{
		const FLCOKeyMappingInfo& Mapping = Mappings[Index];
		if (ULCOKeyBindingRowWidget* Row = KeyBindingRows[Index])
		{
			Row->SetKeyBinding(Mapping.MappingName, GetKeyBindingDisplayName(Mapping.MappingName, Mapping.DisplayName), Mapping.CurrentKey, KeySelectionText);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOControlsSettingsPanel] Key bindings refreshed: Rows=%d"), KeyBindingRows.Num());
}

void ULCOControlsSettingsPanel::UnbindKeyBindingRows()
{
	for (ULCOKeyBindingRowWidget* Row : KeyBindingRows)
	{
		if (Row)
		{
			Row->OnKeySelected.RemoveDynamic(this, &ThisClass::HandleKeyBindingRowKeySelected);
		}
	}
}

FText ULCOControlsSettingsPanel::GetKeyBindingDisplayName(FName MappingName, const FText& Fallback) const
{
	const FString TextKey = FString(TEXT("KEYBIND_")) + MappingName.ToString().ToUpper();
	const FText Text = GetUIText(FName(*TextKey));
	return Text.ToString() == TextKey ? Fallback : Text;
}

void ULCOControlsSettingsPanel::SetKeyBindingMessage(const FText& Message)
{
	if (KeyBindingMessageText)
	{
		KeyBindingMessageText->SetText(Message);
	}
}
