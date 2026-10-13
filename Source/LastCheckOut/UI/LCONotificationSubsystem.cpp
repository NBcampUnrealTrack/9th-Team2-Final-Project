#include "UI/LCONotificationSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "TimerManager.h"
#include "UI/LCONotificationPopupWidget.h"
#include "UObject/UObjectGlobals.h"

ULCONotificationSubsystem::ULCONotificationSubsystem()
{
	NotificationPopupWidgetClass = TSoftClassPtr<ULCONotificationPopupWidget>(
		FSoftObjectPath(TEXT("/Game/HJ/UI_Test/Widget_Test/WBP_NotificationPopup.WBP_NotificationPopup_C")));
}

void ULCONotificationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UGameInstance* GameInstance = LocalPlayer->GetGameInstance())
		{
			if (ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance->GetSubsystem<ULCOLocalizationSubsystem>())
			{
				LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
				LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
			}
		}

		TryResolveNotificationContainer(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCONotificationSubsystem::Deinitialize()
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UGameInstance* GameInstance = LocalPlayer->GetGameInstance())
		{
			if (ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance->GetSubsystem<ULCOLocalizationSubsystem>())
			{
				LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
			}
		}
	}

	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupNotifications();
	NotificationContainer = nullptr;

	Super::Deinitialize();
}

void ULCONotificationSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupNotifications();
	NotificationContainer = nullptr;
	TryResolveNotificationContainer(NewPlayerController);
}

bool ULCONotificationSubsystem::ShowNotification(FName NotificationId, float DisplayTime)
{
	if (NotificationId.IsNone() || DisplayTime <= 0.0f)
	{
		return false;
	}

	ActiveNotifications.Insert(FLCONotificationEntry(), 0);
	FLCONotificationEntry& Entry = ActiveNotifications[0];
	ULCONotificationPopupWidget* NotificationWidget = CreateNotificationWidget(DisplayTime, Entry.TimerHandle);
	if (!NotificationWidget)
	{
		ActiveNotifications.RemoveAt(0, 1, EAllowShrinking::No);
		return false;
	}

	NotificationWidget->SetNotificationId(NotificationId);
	Entry.NotificationId = NotificationId;
	Entry.Widget = NotificationWidget;
	RefreshNotificationOpacities();
	return true;
}

bool ULCONotificationSubsystem::ShowItemAcquiredNotification(FName ItemID, float DisplayTime)
{
	UE_LOG(LogTemp, Log, TEXT("[LCONotification] ShowItemAcquiredNotification called. ItemID=%s DisplayTime=%.2f"),
		*ItemID.ToString(),
		DisplayTime);

	if (ItemID.IsNone() || DisplayTime <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] ShowItemAcquiredNotification rejected invalid args. ItemID=%s DisplayTime=%.2f"),
			*ItemID.ToString(),
			DisplayTime);
		return false;
	}

	ActiveNotifications.Insert(FLCONotificationEntry(), 0);
	FLCONotificationEntry& Entry = ActiveNotifications[0];
	ULCONotificationPopupWidget* NotificationWidget = CreateNotificationWidget(DisplayTime, Entry.TimerHandle);
	if (!NotificationWidget)
	{
		ActiveNotifications.RemoveAt(0, 1, EAllowShrinking::No);
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] ShowItemAcquiredNotification failed to create popup. ItemID=%s"),
			*ItemID.ToString());
		return false;
	}

	NotificationWidget->SetItemAcquiredNotification(ItemID);
	Entry.NotificationId = ItemID;
	Entry.Widget = NotificationWidget;
	RefreshNotificationOpacities();
	UE_LOG(LogTemp, Log, TEXT("[LCONotification] ShowItemAcquiredNotification succeeded. ItemID=%s Widget=%s"),
		*ItemID.ToString(),
		*GetNameSafe(NotificationWidget));
	return true;
}

void ULCONotificationSubsystem::HandleLanguageChanged(FName /*NewLanguage*/)
{
	RefreshActiveNotifications();
}

void ULCONotificationSubsystem::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	if (NotificationContainer && NotificationContainer->GetWorld() == World)
	{
		CleanupNotifications();
		NotificationContainer = nullptr;
	}
}

void ULCONotificationSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	NotificationContainer = nullptr;

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryResolveNotificationContainer(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

bool ULCONotificationSubsystem::TryResolveNotificationContainer(APlayerController* PlayerController)
{
	if (NotificationContainer)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCONotification] Notification container already cached: %s"),
			*GetNameSafe(NotificationContainer));
		return true;
	}

	if (!PlayerController || !PlayerController->IsLocalPlayerController() || NotificationContainerName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] Cannot resolve container. PlayerController=%s IsLocal=%s ContainerName=%s"),
			*GetNameSafe(PlayerController),
			PlayerController && PlayerController->IsLocalPlayerController() ? TEXT("true") : TEXT("false"),
			*NotificationContainerName.ToString());
		return false;
	}

	TArray<UUserWidget*> ExistingWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, ExistingWidgets, UUserWidget::StaticClass(), false);
	UE_LOG(LogTemp, Log, TEXT("[LCONotification] Searching notification container. Widgets=%d HudWidgetName=%s ContainerName=%s"),
		ExistingWidgets.Num(),
		*HudWidgetName.ToString(),
		*NotificationContainerName.ToString());

	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (!IsValid(ExistingWidget) || ExistingWidget->GetOwningPlayer() != PlayerController)
		{
			continue;
		}

		if (!HudWidgetName.IsNone() && ExistingWidget->GetFName() != HudWidgetName)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[LCONotification] Skipping widget by HUD name. Widget=%s Expected=%s"),
				*GetNameSafe(ExistingWidget),
				*HudWidgetName.ToString());
			continue;
		}

		if (UPanelWidget* FoundContainer = Cast<UPanelWidget>(ExistingWidget->GetWidgetFromName(NotificationContainerName)))
		{
			NotificationContainer = FoundContainer;
			UE_LOG(LogTemp, Log, TEXT("[LCONotification] Found notification container in HUD widget. HUD=%s Container=%s"),
				*GetNameSafe(ExistingWidget),
				*GetNameSafe(NotificationContainer));
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] HUD widget found but container missing. HUD=%s ContainerName=%s"),
			*GetNameSafe(ExistingWidget),
			*NotificationContainerName.ToString());
	}

	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (!IsValid(ExistingWidget) || ExistingWidget->GetOwningPlayer() != PlayerController)
		{
			continue;
		}

		if (UPanelWidget* FoundContainer = Cast<UPanelWidget>(ExistingWidget->GetWidgetFromName(NotificationContainerName)))
		{
			NotificationContainer = FoundContainer;
			UE_LOG(LogTemp, Log, TEXT("[LCONotification] Found notification container by fallback search. Widget=%s Container=%s"),
				*GetNameSafe(ExistingWidget),
				*GetNameSafe(NotificationContainer));
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCONotification] Notification container not found in any widget. ContainerName=%s Widgets=%d"),
		*NotificationContainerName.ToString(),
		ExistingWidgets.Num());
	return false;
}

ULCONotificationPopupWidget* ULCONotificationSubsystem::CreateNotificationWidget(float DisplayTime, FTimerHandle& OutTimerHandle)
{
	UWorld* World = GetWorld();
	if (!World || DisplayTime <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] CreateNotificationWidget failed: invalid world or DisplayTime. World=%s DisplayTime=%.2f"),
			*GetNameSafe(World),
			DisplayTime);
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
	if (!TryResolveNotificationContainer(PlayerController) || !NotificationContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] CreateNotificationWidget failed: container unresolved. LocalPlayer=%s PlayerController=%s"),
			*GetNameSafe(LocalPlayer),
			*GetNameSafe(PlayerController));
		return nullptr;
	}

	if (NotificationPopupWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] CreateNotificationWidget failed: NotificationPopupWidgetClass is null"));
		return nullptr;
	}

	TSubclassOf<ULCONotificationPopupWidget> LoadedWidgetClass = NotificationPopupWidgetClass.LoadSynchronous();
	if (!LoadedWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] CreateNotificationWidget failed: class load failed. Path=%s"),
			*NotificationPopupWidgetClass.ToString());
		return nullptr;
	}
	UE_LOG(LogTemp, Log, TEXT("[LCONotification] Notification popup class loaded. Class=%s Path=%s"),
		*GetNameSafe(LoadedWidgetClass),
		*NotificationPopupWidgetClass.ToString());

	ULCONotificationPopupWidget* NotificationWidget = CreateWidget<ULCONotificationPopupWidget>(PlayerController, LoadedWidgetClass);
	if (!NotificationWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotification] CreateNotificationWidget failed: CreateWidget returned null. Class=%s PlayerController=%s"),
			*GetNameSafe(LoadedWidgetClass),
			*GetNameSafe(PlayerController));
		return nullptr;
	}
	UE_LOG(LogTemp, Log, TEXT("[LCONotification] Notification popup widget created. Widget=%s"),
		*GetNameSafe(NotificationWidget));

	NotificationContainer->InsertChildAt(0, NotificationWidget);
	UE_LOG(LogTemp, Log, TEXT("[LCONotification] Notification popup inserted into container. Widget=%s Container=%s Children=%d"),
		*GetNameSafe(NotificationWidget),
		*GetNameSafe(NotificationContainer),
		NotificationContainer->GetChildrenCount());
	World->GetTimerManager().SetTimer(
		OutTimerHandle,
		FTimerDelegate::CreateUObject(this, &ThisClass::RemoveNotification, NotificationWidget),
		DisplayTime,
		false);

	return NotificationWidget;
}

void ULCONotificationSubsystem::CleanupNotifications()
{
	if (UWorld* World = GetWorld())
	{
		for (FLCONotificationEntry& Entry : ActiveNotifications)
		{
			World->GetTimerManager().ClearTimer(Entry.TimerHandle);
		}
	}

	for (FLCONotificationEntry& Entry : ActiveNotifications)
	{
		if (Entry.Widget)
		{
			Entry.Widget->RemoveFromParent();
		}
	}

	ActiveNotifications.Empty();
}

void ULCONotificationSubsystem::RemoveNotification(ULCONotificationPopupWidget* NotificationWidget)
{
	if (!NotificationWidget)
	{
		return;
	}

	for (int32 Index = ActiveNotifications.Num() - 1; Index >= 0; --Index)
	{
		FLCONotificationEntry& Entry = ActiveNotifications[Index];
		if (Entry.Widget == NotificationWidget)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(Entry.TimerHandle);
			}

			NotificationWidget->RemoveFromParent();
			ActiveNotifications.RemoveAt(Index);
			RefreshNotificationOpacities();
			return;
		}
	}

	NotificationWidget->RemoveFromParent();
}

void ULCONotificationSubsystem::RefreshActiveNotifications()
{
	for (int32 Index = ActiveNotifications.Num() - 1; Index >= 0; --Index)
	{
		ULCONotificationPopupWidget* NotificationWidget = ActiveNotifications[Index].Widget;
		if (!IsValid(NotificationWidget))
		{
			ActiveNotifications.RemoveAt(Index);
			continue;
		}

		NotificationWidget->RefreshText();
	}
}

void ULCONotificationSubsystem::RefreshNotificationOpacities()
{
	for (int32 Index = ActiveNotifications.Num() - 1; Index >= 0; --Index)
	{
		ULCONotificationPopupWidget* NotificationWidget = ActiveNotifications[Index].Widget;
		if (!IsValid(NotificationWidget))
		{
			ActiveNotifications.RemoveAt(Index);
		}
	}

	for (int32 Index = 0; Index < ActiveNotifications.Num(); ++Index)
	{
		if (ULCONotificationPopupWidget* NotificationWidget = ActiveNotifications[Index].Widget)
		{
			const float Opacity = FMath::Max(MinimumOpacity, 1.0f - (OpacityStep * Index));
			NotificationWidget->SetRenderOpacity(Opacity);
		}
	}
}
