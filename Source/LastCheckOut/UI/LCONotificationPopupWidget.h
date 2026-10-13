#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCONotificationPopupWidget.generated.h"

class ULCOLocalizationSubsystem;
class UTextBlock;

UCLASS(Abstract)
class LASTCHECKOUT_API ULCONotificationPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Notification")
	void SetNotificationId(FName InNotificationId);

	UFUNCTION(BlueprintCallable, Category = "LCO|Notification")
	void SetItemAcquiredNotification(FName InItemID);

	UFUNCTION(BlueprintCallable, Category = "LCO|Notification")
	void RefreshText();

	UFUNCTION(BlueprintPure, Category = "LCO|Notification")
	FName GetNotificationId() const { return NotificationId; }

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TextBlock_Notification;

private:
	ULCOLocalizationSubsystem* GetLocalizationSubsystem() const;
	FText BuildItemAcquiredText(const ULCOLocalizationSubsystem* LocalizationSubsystem) const;

	UPROPERTY(Transient)
	FName NotificationId;

	UPROPERTY(Transient)
	FName ItemID;

	UPROPERTY(Transient)
	bool bIsItemAcquiredNotification = false;
};
