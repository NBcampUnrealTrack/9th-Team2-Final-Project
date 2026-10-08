#include "UI/LCOContainerWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/LCOContainer.h"
#include "Inventory/InventoryComponent.h"
#include "Item/ItemData.h"
#include "UI/LCOContainerItemWidget.h"

void ULCOContainerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);

	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseButtonClicked);
		CloseButton->OnClicked.AddDynamic(this, &ThisClass::HandleCloseButtonClicked);
	}
}

void ULCOContainerWidget::NativeDestruct()
{
	if (bContainerInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	Container.Reset();
	Interactor.Reset();

	Super::NativeDestruct();
}

void ULCOContainerWidget::OpenContainer(ALCOContainer* InContainer, AActor* InInteractor)
{
	if (!IsValid(InContainer))
	{
		return;
	}

	Container = InContainer;
	Interactor = InInteractor;

	if (!IsInViewport())
	{
		AddToViewport(ViewportZOrder);
	}

	ApplyContainerInputMode();
	RefreshItems();
}

void ULCOContainerWidget::CloseContainer()
{
	if (!IsInViewport())
	{
		if (bContainerInputModeApplied && bRestoreGameInputOnClose)
		{
			RestoreGameInputMode();
		}

		Container.Reset();
		Interactor.Reset();
		return;
	}

	if (bContainerInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	Container.Reset();
	Interactor.Reset();
	RemoveFromParent();
	OnContainerWidgetClosed.Broadcast();
}

void ULCOContainerWidget::RefreshItems()
{
	if (!ItemsPanel)
	{
		return;
	}

	ItemsPanel->ClearChildren();

	ALCOContainer* ContainerActor = Container.Get();
	if (!IsValid(ContainerActor) || !ItemWidgetClass)
	{
		if (EmptyText)
		{
			EmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		return;
	}

	const TArray<FLCOContainerItem>& Items = ContainerActor->GetContainedItems();
	if (EmptyText)
	{
		EmptyText->SetVisibility(Items.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	for (const FLCOContainerItem& Item : Items)
	{
		if (Item.ItemID.IsNone() || Item.Quantity <= 0)
		{
			continue;
		}

		ULCOContainerItemWidget* ItemWidget = CreateWidget<ULCOContainerItemWidget>(GetOwningPlayer(), ItemWidgetClass);
		if (!ItemWidget)
		{
			continue;
		}

		ItemWidget->SetItem(Item.ItemID, Item.Quantity, ResolveItemName(Item.ItemID), ResolveItemIcon(Item.ItemID));
		ItemWidget->OnItemClicked.RemoveDynamic(this, &ThisClass::HandleItemClicked);
		ItemWidget->OnItemClicked.AddDynamic(this, &ThisClass::HandleItemClicked);
		ItemsPanel->AddChild(ItemWidget);
	}
}

bool ULCOContainerWidget::TakeContainerItem(FName ItemID, int32 Quantity)
{
	ALCOContainer* ContainerActor = Container.Get();
	AActor* InteractorActor = Interactor.Get();
	if (!IsValid(ContainerActor) || !IsValid(InteractorActor) || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	if (!ContainerActor->TakeItem(InteractorActor, ItemID, Quantity))
	{
		return false;
	}

	OnContainerWidgetItemTaken.Broadcast(ContainerActor, ItemID, Quantity);
	RefreshItems();
	return true;
}

void ULCOContainerWidget::HandleCloseButtonClicked()
{
	CloseContainer();
}

void ULCOContainerWidget::HandleItemClicked(FName ItemID, int32 ListedQuantity)
{
	const int32 Quantity = FMath::Clamp(QuantityToTakeOnClick, 1, ListedQuantity);
	TakeContainerItem(ItemID, Quantity);
}

void ULCOContainerWidget::ApplyContainerInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetWidgetToFocus(TakeWidget());

	if (!bContainerInputModeApplied)
	{
		bPreviousShowMouseCursor = PC->bShowMouseCursor;

		if (bBlockLookInputWhileOpen && !PC->IsLookInputIgnored())
		{
			PC->SetIgnoreLookInput(true);
			bLookInputBlockedByThisWidget = true;
		}
	}

	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	bContainerInputModeApplied = true;
}

void ULCOContainerWidget::RestoreGameInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (bLookInputBlockedByThisWidget)
	{
		PC->SetIgnoreLookInput(false);
		bLookInputBlockedByThisWidget = false;
	}

	if (ShouldRestoreGameOnlyInputMode(PC))
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
		PC->SetShowMouseCursor(bPreviousShowMouseCursor);
	}

	bContainerInputModeApplied = false;
}

bool ULCOContainerWidget::ShouldRestoreGameOnlyInputMode(APlayerController* PC) const
{
	if (!PC || bPreviousShowMouseCursor)
	{
		return false;
	}

	return HasUserFocus(PC) || HasUserFocusedDescendants(PC);
}

FText ULCOContainerWidget::ResolveItemName(FName ItemID) const
{
	const AActor* InteractorActor = Interactor.Get();
	const UInventoryComponent* Inventory = IsValid(InteractorActor) ? InteractorActor->FindComponentByClass<UInventoryComponent>() : nullptr;
	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	return ItemData ? ItemData->ItemName : FText::FromString(ItemID.ToString());
}

UTexture2D* ULCOContainerWidget::ResolveItemIcon(FName ItemID) const
{
	const AActor* InteractorActor = Interactor.Get();
	const UInventoryComponent* Inventory = IsValid(InteractorActor) ? InteractorActor->FindComponentByClass<UInventoryComponent>() : nullptr;
	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	return ItemData ? ItemData->Icon : nullptr;
}
