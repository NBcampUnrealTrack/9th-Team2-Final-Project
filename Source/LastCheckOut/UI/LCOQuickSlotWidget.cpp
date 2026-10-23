#include "UI/LCOQuickSlotWidget.h"

#include "UI/LCOQuickSlotEntryWidget.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Item/ItemData.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryComponent.h"
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

void ULCOQuickSlotWidget::SetWheelMode(bool bInWheelMode)
{
	// 위젯의 표시 모드를 설정한다.
	// 실제 슬롯 표시 방식은 RefreshSlots()에서 이 값을 활용한다.
	bIsWheelMode = bInWheelMode;
}

void ULCOQuickSlotWidget::RefreshSlots(ULCOQuickSlotComponent* QuickSlot)
{
	if (!QuickSlot)
	{
		CachedQuickSlot.Reset();
		return;
	}

	CachedQuickSlot = QuickSlot;
	
	// 인벤토리 정보를 가져온다.
	UInventoryComponent* Inventory = QuickSlot->GetInventory();

	for (int32 i = 0; i < ULCOQuickSlotComponent::NumSlots; ++i)
	{
		ULCOQuickSlotEntryWidget* Entry = GetEntry(i);

		if (!Entry)
		{
			continue;
		}

		// 기본값은 빈 슬롯으로 설정한다.
		FName ItemID = NAME_None;
		const FItemData* Data = nullptr;
		int Count = 0;

		if (bIsWheelMode)
		{
			// Q 휠은 소비 아이템 슬롯 데이터를 그대로 표시한다.
			ItemID = QuickSlot->GetSlotItem(i);
			Data = QuickSlot->GetSlotItemData(i);
			Count = QuickSlot->GetSlotItemCount(i);
		}
		else if (Inventory)
		{
			// HUD 1번 칸은 카메라를 표시한다.
			if (i == 0)
			{
				ItemID = FName(TEXT("Camera"));
				Data = Inventory->GetItemData(ItemID);
				Count = 1;
			}
			// HUD 2번 칸은 보유 중인 손전등을 표시한다.
			else if (i == 1)
			{
				const FName FlashlightID(TEXT("Flashlight"));

				if (Inventory->HasItem(FlashlightID))
				{
					ItemID = FlashlightID;
					Data = Inventory->GetItemData(ItemID);
					Count = 1;
				}
			}

		}

		// 아이템 데이터에서 아이콘과 이름을 가져온다.
		UTexture2D* Icon = Data ? Data->Icon.Get() : nullptr;

		const FText Name = ResolveItemName(
			ItemID,
			Data ? Data->ItemName : FText::GetEmpty()
		);

		// 각 슬롯 위젯에 표시 정보를 전달한다.
		Entry->SetSlotData(i, ItemID, Icon, Name, Count);
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
