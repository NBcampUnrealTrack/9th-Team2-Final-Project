#include "UI/LCOPauseMenuSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"
#include "UI/LCOPauseMenuWidget.h"

void ULCOPauseMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreatePauseMenu(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCOPauseMenuSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupPauseMenu();

	Super::Deinitialize();
}

void ULCOPauseMenuSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupPauseMenu();
	TryCreatePauseMenu(NewPlayerController);
}

void ULCOPauseMenuSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (PauseMenuWidget && PauseMenuWidget->GetWorld() == World)
	{
		CleanupPauseMenu();
	}
}

void ULCOPauseMenuSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreatePauseMenu(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

void ULCOPauseMenuSubsystem::TryCreatePauseMenu(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		return;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!ShouldCreatePauseMenuForWorld(World))
	{
		return;
	}

	if (PauseMenuWidget && PauseMenuWidget->GetOwningPlayer() == PlayerController)
	{
		return;
	}

	if (PauseMenuWidget)
	{
		CleanupPauseMenu();
	}

	if (PauseMenuWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPauseMenuSubsystem] PauseMenuWidgetClass is not configured"));
		return;
	}

	TSubclassOf<ULCOPauseMenuWidget> LoadedWidgetClass = PauseMenuWidgetClass.LoadSynchronous();
	if (!LoadedWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPauseMenuSubsystem] Failed to load PauseMenuWidgetClass: %s"), *PauseMenuWidgetClass.ToString());
		return;
	}

	TArray<UUserWidget*> ExistingWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, ExistingWidgets, LoadedWidgetClass, false);
	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (ULCOPauseMenuWidget* ExistingPauseMenu = Cast<ULCOPauseMenuWidget>(ExistingWidget))
		{
			if (ExistingPauseMenu->GetOwningPlayer() == PlayerController)
			{
				PauseMenuWidget = ExistingPauseMenu;
				return;
			}
		}
	}

	PauseMenuWidget = CreateWidget<ULCOPauseMenuWidget>(PlayerController, LoadedWidgetClass);
}

void ULCOPauseMenuSubsystem::CleanupPauseMenu()
{
	if (!PauseMenuWidget)
	{
		return;
	}

	PauseMenuWidget->UnbindFromOptionsInput();

	if (PauseMenuWidget->IsInViewport())
	{
		PauseMenuWidget->ClosePauseMenu();
	}
	else
	{
		PauseMenuWidget->RemoveFromParent();
	}

	PauseMenuWidget = nullptr;
}

bool ULCOPauseMenuSubsystem::ShouldCreatePauseMenuForWorld(const UWorld* World) const
{
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	const FString MapName = GetComparableMapName(World);
	if (MapName.IsEmpty())
	{
		return false;
	}

	for (const FName& ExcludedMapName : ExcludedMapNames)
	{
		if (MapName.Equals(ExcludedMapName.ToString(), ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	return true;
}

FString ULCOPauseMenuSubsystem::GetComparableMapName(const UWorld* World) const
{
	if (!World)
	{
		return FString();
	}

	return UWorld::RemovePIEPrefix(World->GetMapName());
}
