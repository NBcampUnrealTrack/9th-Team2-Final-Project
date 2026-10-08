#include "UI/LCOContainerWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interaction/LCOContainer.h"
#include "Inventory/InventoryComponent.h"
#include "Item/ItemData.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "UI/LCOContainerItemWidget.h"
#include "UI/LCOInteractionPromptSubsystem.h"

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

void ULCOContainerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindLocalizationEvents();
	RefreshLocalizedTexts();
}

void ULCOContainerWidget::NativeDestruct()
{
	UnbindLocalizationEvents();

	if (bContainerInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	Container.Reset();
	Interactor.Reset();

	Super::NativeDestruct();
}

FReply ULCOContainerWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsInViewport() && InKeyEvent.GetKey() == EKeys::F)
	{
		CloseContainer();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
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

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, true);
		}
	}

	ApplyContainerInputMode();
	RefreshLocalizedTexts();
	RefreshItems();
}

void ULCOContainerWidget::CloseContainer()
{
	ALCOContainer* ContainerActor = Container.Get();
	if (IsValid(ContainerActor) && ContainerActor->IsOpen())
	{
		ContainerActor->Close();
	}

	if (!IsInViewport())
	{
		if (bContainerInputModeApplied && bRestoreGameInputOnClose)
		{
			RestoreGameInputMode();
		}

		Container.Reset();
		Interactor.Reset();
		if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
			{
				InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, false);
			}
		}
		return;
	}

	if (bContainerInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	Container.Reset();
	Interactor.Reset();
	RemoveFromParent();
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, false);
		}
	}
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

void ULCOContainerWidget::HandleLanguageChanged(FName NewLanguage)
{
	RefreshLocalizedTexts();
	RefreshItems();
}

void ULCOContainerWidget::BindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOContainerWidget::UnbindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOContainerWidget::RefreshLocalizedTexts()
{
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();
	if (!LocalizationSubsystem)
	{
		if (ContainerTitleText)
		{
			ContainerTitleText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (ContainerTitleText)
	{
		const FName ContainerNameKey = TEXT("CONTAINER_STORAGE_CABINET");
		const FText ContainerTitle = LocalizationSubsystem->GetUIText(ContainerNameKey);

		if (!ContainerNameKey.IsNone() && !ContainerTitle.IsEmpty() && ContainerTitle.ToString() != ContainerNameKey.ToString())
		{
			ContainerTitleText->SetText(ContainerTitle);
			ContainerTitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ContainerTitleText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (EmptyText)
	{
		EmptyText->SetText(LocalizationSubsystem->GetUIText(TEXT("CONTAINER_EMPTY")));
	}
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
	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		const FText LocalizedItemName = LocalizationSubsystem->GetItemText(ItemID);
		if (!LocalizedItemName.IsEmpty())
		{
			return LocalizedItemName;
		}
	}

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
