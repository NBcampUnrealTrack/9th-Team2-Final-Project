#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCONotificationSubsystem.generated.h"

class APlayerController;
class ULCONotificationPopupWidget;
class UPanelWidget;
class UUserWidget;
class UWorld;

USTRUCT()
struct FLCONotificationEntry
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FName NotificationId;

	UPROPERTY(Transient)
	TObjectPtr<ULCONotificationPopupWidget> Widget = nullptr;

	FTimerHandle TimerHandle;
};

UCLASS(Config = Game)
class LASTCHECKOUT_API ULCONotificationSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	ULCONotificationSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	UFUNCTION(BlueprintCallable, Category = "LCO|Notification", meta = (DisplayName = "Show Notification", CPP_Default_DisplayTime = "3.0"))
	bool ShowNotification(FName NotificationId, float DisplayTime = 3.0f);

	UFUNCTION(BlueprintCallable, Category = "LCO|Notification", meta = (DisplayName = "Show Item Acquired Notification", CPP_Default_DisplayTime = "3.0"))
	bool ShowItemAcquiredNotification(FName ItemID, float DisplayTime = 3.0f);

private:
	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	bool TryResolveNotificationContainer(APlayerController* PlayerController);
	ULCONotificationPopupWidget* CreateNotificationWidget(float DisplayTime, FTimerHandle& OutTimerHandle);
	void CleanupNotifications();
	void RemoveNotification(ULCONotificationPopupWidget* NotificationWidget);
	void RefreshActiveNotifications();
	void RefreshNotificationOpacities();

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Notification")
	TSoftClassPtr<ULCONotificationPopupWidget> NotificationPopupWidgetClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Notification")
	FName HudWidgetName = TEXT("WBP_TestHUD");

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Notification")
	FName NotificationContainerName = TEXT("VerticalBox_Notifications");

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Notification", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float OpacityStep = 0.2f;

	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Notification", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumOpacity = 0.2f;

	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> NotificationContainer;

	UPROPERTY(Transient)
	TArray<FLCONotificationEntry> ActiveNotifications;
};
