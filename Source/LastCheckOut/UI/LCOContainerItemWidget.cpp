#include "UI/LCOContainerItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void ULCOContainerItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleItemButtonClicked);
		ItemButton->OnClicked.AddDynamic(this, &ThisClass::HandleItemButtonClicked);
	}
}

void ULCOContainerItemWidget::SetItem(FName InItemID, int32 InQuantity, const FText& InDisplayName, UTexture2D* InIcon)
{
	ItemID = InItemID;
	Quantity = InQuantity;

	if (ItemNameText)
	{
		ItemNameText->SetText(InDisplayName);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Quantity));
	}

	if (ItemIconImage)
	{
		if (InIcon)
		{
			ItemIconImage->SetBrushFromTexture(InIcon);
			ItemIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ItemIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void ULCOContainerItemWidget::HandleItemButtonClicked()
{
	if (!ItemID.IsNone() && Quantity > 0)
	{
		OnItemClicked.Broadcast(ItemID, Quantity);
	}
}
