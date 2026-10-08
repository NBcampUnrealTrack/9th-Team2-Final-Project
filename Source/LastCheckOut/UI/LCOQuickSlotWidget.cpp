#include "UI/LCOQuickSlotWidget.h"

#include "UI/LCOQuickSlotEntryWidget.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Item/ItemData.h"
#include "Components/TextBlock.h"

ULCOQuickSlotEntryWidget* ULCOQuickSlotWidget::GetEntry(int32 Index) const
{
	switch (Index)
	{
	case 0: return Slot_1;
	case 1: return Slot_2;
	case 2: return Slot_3;
	case 3: return Slot_4;
	default: return nullptr;
	}
}

void ULCOQuickSlotWidget::RefreshSlots(const ULCOQuickSlotComponent* QuickSlot)
{
	if (!QuickSlot)
	{
		return;
	}

	for (int32 i = 0; i < ULCOQuickSlotComponent::NumSlots; ++i)
	{
		ULCOQuickSlotEntryWidget* Entry = GetEntry(i);
		if (!Entry)
		{
			continue;
		}

		const FName ItemID = QuickSlot->GetSlotItem(i);
		const FItemData* Data = QuickSlot->GetSlotItemData(i);

		UTexture2D* Icon = Data ? Data->Icon.Get() : nullptr;
		const FText Name = Data ? Data->ItemName : FText::GetEmpty();

		Entry->SetSlotData(i, ItemID, Icon, Name, QuickSlot->GetSlotItemCount(i));
	}

	UpdateSelectedName();
}

void ULCOQuickSlotWidget::SetSelectedIndex(int32 NewIndex)
{
	SelectedIndex = NewIndex;

	for (int32 i = 0; i < ULCOQuickSlotComponent::NumSlots; ++i)
	{
		if (ULCOQuickSlotEntryWidget* Entry = GetEntry(i))
		{
			Entry->SetHighlighted(i == SelectedIndex);
		}
	}

	UpdateSelectedName();
	OnSelectedIndexChanged(SelectedIndex);
}

void ULCOQuickSlotWidget::UpdateSelectedName()
{
	if (!Txt_SelectedName)
	{
		return;
	}

	const ULCOQuickSlotEntryWidget* Entry = GetEntry(SelectedIndex);
	Txt_SelectedName->SetText(Entry ? Entry->GetItemName() : FText::GetEmpty());
}

void ULCOQuickSlotWidget::ShowQuickSlot_Implementation()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ULCOQuickSlotWidget::HideQuickSlot_Implementation()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
