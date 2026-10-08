#include "UI/LCOInventorySubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/LCOInventoryWidget.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

ULCOInventorySubsystem::ULCOInventorySubsystem()
{
	InventoryWidgetClass = TSoftClassPtr<ULCOInventoryWidget>(FSoftObjectPath(TEXT("/Game/HJ/UI_Test/Widget_Test/WBP_Inventory.WBP_Inventory_C")));
}

void ULCOInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateInventory(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCOInventorySubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupInventory();

	Super::Deinitialize();
}

void ULCOInventorySubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupInventory();
	TryCreateInventory(NewPlayerController);
}

void ULCOInventorySubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (InventoryWidget && InventoryWidget->GetWorld() == World)
	{
		CleanupInventory();
	}
}

void ULCOInventorySubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateInventory(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

void ULCOInventorySubsystem::TryCreateInventory(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldCreateInventoryForWorld(PlayerController->GetWorld()))
	{
		return;
	}

	if (InventoryWidget && InventoryWidget->GetOwningPlayer() == PlayerController)
	{
		return;
	}

	if (InventoryWidget)
	{
		CleanupInventory();
	}

	if (InventoryWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOInventorySubsystem] InventoryWidgetClass is not configured"));
		return;
	}

	TSubclassOf<ULCOInventoryWidget> LoadedWidgetClass = InventoryWidgetClass.LoadSynchronous();
	if (!LoadedWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOInventorySubsystem] Failed to load InventoryWidgetClass: %s"), *InventoryWidgetClass.ToString());
		return;
	}

	TArray<UUserWidget*> ExistingWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, ExistingWidgets, LoadedWidgetClass, false);
	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (ULCOInventoryWidget* ExistingInventory = Cast<ULCOInventoryWidget>(ExistingWidget))
		{
			if (ExistingInventory->GetOwningPlayer() == PlayerController)
			{
				InventoryWidget = ExistingInventory;
				return;
			}
		}
	}

	InventoryWidget = CreateWidget<ULCOInventoryWidget>(PlayerController, LoadedWidgetClass);
}

void ULCOInventorySubsystem::CleanupInventory()
{
	if (!InventoryWidget)
	{
		return;
	}

	InventoryWidget->UnbindFromInventoryInput();

	if (InventoryWidget->IsInViewport())
	{
		InventoryWidget->CloseInventory();
	}
	else
	{
		InventoryWidget->RemoveFromParent();
	}

	InventoryWidget = nullptr;
}

bool ULCOInventorySubsystem::ShouldCreateInventoryForWorld(const UWorld* World) const
{
	return World && World->IsGameWorld();
}
