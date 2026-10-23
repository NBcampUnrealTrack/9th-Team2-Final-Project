#include "UI/Settings/LCOKeyBindingRowWidget.h"

#include "Components/InputKeySelector.h"
#include "Components/TextBlock.h"

void ULCOKeyBindingRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (KeySelector)
	{
		KeySelector->SetAllowModifierKeys(false);
		KeySelector->SetAllowGamepadKeys(false);
		KeySelector->SetEscapeKeys({ EKeys::Escape });

		KeySelector->OnKeySelected.RemoveDynamic(this, &ThisClass::HandleKeySelected);
		KeySelector->OnKeySelected.AddDynamic(this, &ThisClass::HandleKeySelected);
	}
}

void ULCOKeyBindingRowWidget::SetKeyBinding(FName InMappingName, const FText& InDisplayName, const FKey& InKey, const FText& KeySelectionText)
{
	MappingName = InMappingName;

	if (ActionNameText)
	{
		ActionNameText->SetText(InDisplayName);
	}

	if (KeySelector)
	{
		bUpdatingKey = true;
		KeySelector->SetKeySelectionText(KeySelectionText);
		KeySelector->SetSelectedKey(FInputChord(InKey));
		bUpdatingKey = false;
	}
}

void ULCOKeyBindingRowWidget::HandleKeySelected(FInputChord SelectedKey)
{
	if (bUpdatingKey)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOKeyBindingRow] %s key selected: %s"), *MappingName.ToString(), *SelectedKey.Key.ToString());
	OnKeySelected.Broadcast(MappingName, SelectedKey.Key);
}
