#include "UI/LCONotificationPopupWidget.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"

namespace
{
	const FName LCOItemAcquiredFormatKey(TEXT("NOTIFICATION_ITEM_ACQUIRED_FORMAT"));
}

void ULCONotificationPopupWidget::SetNotificationId(FName InNotificationId)
{
	NotificationId = InNotificationId;
	ItemID = NAME_None;
	bIsItemAcquiredNotification = false;
	RefreshText();
}

void ULCONotificationPopupWidget::SetItemAcquiredNotification(FName InItemID)
{
	NotificationId = NAME_None;
	ItemID = InItemID;
	bIsItemAcquiredNotification = true;
	RefreshText();
}

void ULCONotificationPopupWidget::RefreshText()
{
	if (!TextBlock_Notification)
	{
		return;
	}

	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem();
	if (bIsItemAcquiredNotification)
	{
		TextBlock_Notification->SetText(BuildItemAcquiredText(LocalizationSubsystem));
		return;
	}

	if (NotificationId.IsNone())
	{
		TextBlock_Notification->SetText(FText::GetEmpty());
		return;
	}

	if (LocalizationSubsystem)
	{
		TextBlock_Notification->SetText(LocalizationSubsystem->GetUIText(NotificationId));
		return;
	}

	TextBlock_Notification->SetText(FText::FromName(NotificationId));
}

ULCOLocalizationSubsystem* ULCONotificationPopupWidget::GetLocalizationSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance
		? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>()
		: nullptr;
}

FText ULCONotificationPopupWidget::BuildItemAcquiredText(const ULCOLocalizationSubsystem* LocalizationSubsystem) const
{
	if (ItemID.IsNone())
	{
		return FText::GetEmpty();
	}

	const FText ItemName = LocalizationSubsystem
		? LocalizationSubsystem->GetItemText(ItemID)
		: FText::FromName(ItemID);

	if (!LocalizationSubsystem)
	{
		return ItemName;
	}

	const FText FormatText = LocalizationSubsystem->GetUIText(LCOItemAcquiredFormatKey);
	if (FormatText.IsEmpty() || FormatText.ToString() == LCOItemAcquiredFormatKey.ToString())
	{
		return ItemName;
	}

	return FText::Format(FormatText, ItemName);
}
