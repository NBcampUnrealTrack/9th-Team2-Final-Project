#include "UI/LCOQuickSlotWidget.h"

#include "UI/LCOQuickSlotEntryWidget.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Item/ItemData.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"

void ULCOQuickSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindLocalizationEvents();
	UpdateSelectedName();
}

void ULCOQuickSlotWidget::NativeDestruct()
{
	UnbindLocalizationEvents();

	Super::NativeDestruct();
}

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

void ULCOQuickSlotWidget::RefreshSlots(ULCOQuickSlotComponent* QuickSlot)
{
	if (!QuickSlot)
	{
		CachedQuickSlot.Reset();
		return;
	}

	CachedQuickSlot = QuickSlot;

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
		const FText Name = ResolveItemName(ItemID, Data ? Data->ItemName : FText::GetEmpty());

		Entry->SetSlotData(i, ItemID, Icon, Name, QuickSlot->GetSlotItemCount(i));
	}

	UpdateSelectedName();
}

void ULCOQuickSlotWidget::SetEntriesInteractive(bool bInteractive)
{
	for (int32 i = 0; i < ULCOQuickSlotComponent::NumSlots; ++i)
	{
		if (ULCOQuickSlotEntryWidget* Entry = GetEntry(i))
		{
			Entry->SetInteractive(bInteractive);
		}
	}
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

FText ULCOQuickSlotWidget::ResolveItemName(FName ItemID, const FText& FallbackName) const
{
	if (!ItemID.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const ULCOLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr;
		if (Localization)
		{
			const FText LocalizedName = Localization->GetItemText(ItemID);
			if (!LocalizedName.IsEmpty())
			{
				return LocalizedName;
			}
		}
	}

	return FallbackName;
}

void ULCOQuickSlotWidget::HandleLanguageChanged(FName NewLanguage)
{
	if (ULCOQuickSlotComponent* QuickSlot = CachedQuickSlot.Get())
	{
		RefreshSlots(QuickSlot);
		return;
	}

	UpdateSelectedName();
}

void ULCOQuickSlotWidget::BindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOQuickSlotWidget::UnbindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOQuickSlotWidget::UpdateSelectedName()
{
	if (!Txt_SelectedName)
	{
		return;
	}

	if (const ULCOQuickSlotEntryWidget* Entry = GetEntry(SelectedIndex))
	{
		Txt_SelectedName->SetText(Entry->GetItemName());
		return;
	}

	// 선택 없음 = 가운데 → "맨손"
	FText NoSelectionText = FText::GetEmpty();
	if (!NoSelectionTextKey.IsNone())
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const ULCOLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr;
		NoSelectionText = Localization ? Localization->GetUIText(NoSelectionTextKey) : FText::FromName(NoSelectionTextKey);
	}

	Txt_SelectedName->SetText(NoSelectionText);
}

void ULCOQuickSlotWidget::ShowQuickSlot_Implementation()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ULCOQuickSlotWidget::HideQuickSlot_Implementation()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
