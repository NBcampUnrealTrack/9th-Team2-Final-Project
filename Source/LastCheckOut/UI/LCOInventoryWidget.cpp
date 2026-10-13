#include "UI/LCOInventoryWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventorySlot.h"
#include "Item/ItemData.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Player/LCOPlayerController.h"
#include "UI/LCOInteractionPromptSubsystem.h"
#include "UI/LCOInventoryItemWidget.h"
#include "UI/LCOQuickSlotWidget.h"
#include "QuickSlot/LCOQuickSlotComponent.h"

void ULCOInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);

	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseButtonClicked);
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}

	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnInventoryRequested.RemoveDynamic(this, &ThisClass::HandleInventoryRequested);
		LCOPC->OnInventoryRequested.AddDynamic(this, &ThisClass::HandleInventoryRequested);
	}

	HideItemDetail();
}

void ULCOInventoryWidget::NativeDestruct()
{
	UnbindQuickSlotEvents();

	if (bInventoryInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	Super::NativeDestruct();
}

void ULCOInventoryWidget::OpenInventory()
{
	if (!IsInViewport())
	{
		AddToViewport(ViewportZOrder);
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, true);
		}
	}

	ApplyInventoryInputMode();
	RefreshLocalizedTexts();

	// 열 때마다 선택 초기화 (상세 패널 닫힌 상태로 시작)
	SelectedItemID = NAME_None;
	RefreshItems();

	BindQuickSlotEvents();
	RefreshQuickSlotPanel();
}

void ULCOInventoryWidget::CloseInventory()
{
	UnbindQuickSlotEvents();

	if (bInventoryInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	if (IsInViewport())
	{
		RemoveFromParent();
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, false);
		}
	}
}

void ULCOInventoryWidget::ToggleInventory()
{
	if (IsInViewport())
	{
		CloseInventory();
	}
	else
	{
		if (UGameplayStatics::IsGamePaused(this))
		{
			return;
		}

		OpenInventory();
	}
}

void ULCOInventoryWidget::UnbindFromInventoryInput()
{
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnInventoryRequested.RemoveDynamic(this, &ThisClass::HandleInventoryRequested);
	}
}

void ULCOInventoryWidget::RefreshItems()
{
	if (!ItemsPanel)
	{
		return;
	}

	ItemsPanel->ClearChildren();

	UInventoryComponent* Inventory = GetPlayerInventory();
	if (!Inventory || !ItemWidgetClass)
	{
		if (EmptyText)
		{
			EmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		return;
	}

	bool bHasVisibleItem = false;
	bool bSelectedItemFound = false;
	for (const FInventorySlot& InventorySlot : Inventory->InventorySlots)
	{
		if (InventorySlot.ItemID.IsNone() || InventorySlot.Quantity <= 0)
		{
			continue;
		}

		ULCOInventoryItemWidget* ItemWidget = CreateWidget<ULCOInventoryItemWidget>(GetOwningPlayer(), ItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetItem(InventorySlot.ItemID, InventorySlot.Quantity, ResolveItemName(InventorySlot.ItemID, Inventory), ResolveItemIcon(InventorySlot.ItemID, Inventory));
		ItemWidget->OnItemSelected.AddDynamic(this, &ThisClass::HandleItemSelected);
		ItemWidget->SetSelected(InventorySlot.ItemID == SelectedItemID);
		ItemsPanel->AddChild(ItemWidget);
		bHasVisibleItem = true;

		if (InventorySlot.ItemID == SelectedItemID)
		{
			bSelectedItemFound = true;
		}
	}

	// 선택했던 아이템이 사라졌으면(다 씀 등) 상세 패널 닫기, 남아 있으면 내용 갱신
	if (bSelectedItemFound)
	{
		ShowItemDetail(SelectedItemID);
	}
	else
	{
		HideItemDetail();
	}

	if (EmptyText)
	{
		EmptyText->SetVisibility(bHasVisibleItem ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void ULCOInventoryWidget::HandleInventoryRequested()
{
	ToggleInventory();
}

void ULCOInventoryWidget::HandleCloseButtonClicked()
{
	CloseInventory();
}

void ULCOInventoryWidget::ApplyInventoryInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC || bInventoryInputModeApplied)
	{
		return;
	}

	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
	{
		LCOPC->SetUIMode(true);
	}
	else
	{
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC, this, EMouseLockMode::DoNotLock, false);
		PC->SetShowMouseCursor(true);
	}

	bInventoryInputModeApplied = true;
}

void ULCOInventoryWidget::RestoreGameInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
	{
		LCOPC->SetUIMode(false);
	}
	else
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
		PC->SetShowMouseCursor(false);
	}

	bInventoryInputModeApplied = false;
}

ULCOQuickSlotComponent* ULCOInventoryWidget::GetPlayerQuickSlot() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return IsValid(Pawn) ? Pawn->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr;
}

void ULCOInventoryWidget::BindQuickSlotEvents()
{
	ULCOQuickSlotComponent* QuickSlot = GetPlayerQuickSlot();
	if (BoundQuickSlot.Get() == QuickSlot)
	{
		return;
	}

	UnbindQuickSlotEvents();

	if (QuickSlot)
	{
		QuickSlot->OnQuickSlotsChanged.AddUniqueDynamic(this, &ThisClass::HandleQuickSlotsChanged);
		BoundQuickSlot = QuickSlot;
	}
}

void ULCOInventoryWidget::UnbindQuickSlotEvents()
{
	if (ULCOQuickSlotComponent* QuickSlot = BoundQuickSlot.Get())
	{
		QuickSlot->OnQuickSlotsChanged.RemoveDynamic(this, &ThisClass::HandleQuickSlotsChanged);
	}

	BoundQuickSlot.Reset();
}

void ULCOInventoryWidget::HandleQuickSlotsChanged()
{
	RefreshQuickSlotPanel();
}

void ULCOInventoryWidget::RefreshQuickSlotPanel()
{
	if (!QuickSlotPanel)
	{
		return;
	}

	ULCOQuickSlotComponent* QuickSlot = GetPlayerQuickSlot();
	if (!QuickSlot)
	{
		QuickSlotPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// HUD용 WBP를 재사용해도 항상 보이고, 칸은 드래그 앤 드롭 가능하게
	QuickSlotPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	QuickSlotPanel->SetEntriesInteractive(true);
	QuickSlotPanel->SetSelectedIndex(INDEX_NONE);
	QuickSlotPanel->RefreshSlots(QuickSlot);
}

UInventoryComponent* ULCOInventoryWidget::GetPlayerInventory() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return IsValid(Pawn) ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

FText ULCOInventoryWidget::ResolveItemName(FName ItemID, const UInventoryComponent* Inventory) const
{
	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		const FText LocalizedItemName = LocalizationSubsystem->GetItemText(ItemID);
		if (!LocalizedItemName.IsEmpty())
		{
			return LocalizedItemName;
		}
	}

	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	return ItemData ? ItemData->ItemName : FText::FromString(ItemID.ToString());
}

UTexture2D* ULCOInventoryWidget::ResolveItemIcon(FName ItemID, const UInventoryComponent* Inventory) const
{
	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	return ItemData ? ItemData->Icon : nullptr;
}

FText ULCOInventoryWidget::ResolveItemDescription(FName ItemID, const UInventoryComponent* Inventory) const
{
	// Items.csv에 "아이템ID_DESC" 키가 있으면 번역 문구 우선
	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		const FName DescKey(*FString::Printf(TEXT("%s_DESC"), *ItemID.ToString()));
		const FText LocalizedDesc = LocalizationSubsystem->GetItemText(DescKey);
		if (!LocalizedDesc.IsEmpty())
		{
			return LocalizedDesc;
		}
	}

	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	return ItemData ? ItemData->Description : FText::GetEmpty();
}

// ======================================================================
// 아이템 상세 설명
// ======================================================================

void ULCOInventoryWidget::HandleItemSelected(FName ItemID)
{
	if (ItemID.IsNone())
	{
		return;
	}

	ShowItemDetail(ItemID);
}

void ULCOInventoryWidget::ShowItemDetail(FName ItemID)
{
	const UInventoryComponent* Inventory = GetPlayerInventory();
	if (ItemID.IsNone() || !Inventory)
	{
		HideItemDetail();
		return;
	}

	SelectedItemID = ItemID;

	if (Img_DetailIcon)
	{
		if (UTexture2D* DetailIcon = ResolveItemIcon(ItemID, Inventory))
		{
			Img_DetailIcon->SetBrushFromTexture(DetailIcon);
			Img_DetailIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Img_DetailIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (Txt_DetailName)
	{
		Txt_DetailName->SetText(ResolveItemName(ItemID, Inventory));
	}

	if (Txt_DetailDescription)
	{
		Txt_DetailDescription->SetText(ResolveItemDescription(ItemID, Inventory));
	}

	if (DetailPanel)
	{
		DetailPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	UpdateItemSelectionVisuals();
	OnItemDetailChanged(SelectedItemID);
}

void ULCOInventoryWidget::HideItemDetail()
{
	SelectedItemID = NAME_None;

	if (DetailPanel)
	{
		DetailPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	UpdateItemSelectionVisuals();
	OnItemDetailChanged(NAME_None);
}

void ULCOInventoryWidget::UpdateItemSelectionVisuals()
{
	if (!ItemsPanel)
	{
		return;
	}

	for (UWidget* Child : ItemsPanel->GetAllChildren())
	{
		if (ULCOInventoryItemWidget* ItemWidget = Cast<ULCOInventoryItemWidget>(Child))
		{
			ItemWidget->SetSelected(!SelectedItemID.IsNone() && ItemWidget->GetItemID() == SelectedItemID);
		}
	}
}

void ULCOInventoryWidget::RefreshLocalizedTexts()
{
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();
	if (!LocalizationSubsystem)
	{
		return;
	}

	if (InventoryTitleText)
	{
		const FName InventoryTitleKey = TEXT("INVENTORY_TITLE");
		const FText InventoryTitle = LocalizationSubsystem->GetUIText(InventoryTitleKey);
		InventoryTitleText->SetText(InventoryTitle);
	}

	if (EmptyText)
	{
		const FName EmptyKey = TEXT("INVENTORY_EMPTY");
		const FText EmptyMessage = LocalizationSubsystem->GetUIText(EmptyKey);
		EmptyText->SetText(EmptyMessage);
	}
}
