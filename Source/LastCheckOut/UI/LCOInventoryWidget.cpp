#include "UI/LCOInventoryWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
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
}

void ULCOInventoryWidget::NativeDestruct()
{
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
	RefreshItems();
}

void ULCOInventoryWidget::CloseInventory()
{
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
		ItemsPanel->AddChild(ItemWidget);
		bHasVisibleItem = true;
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
