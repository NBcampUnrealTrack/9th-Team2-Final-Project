#include "UI/LCOQuickSlotEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void ULCOQuickSlotEntryWidget::SetSlotData(int32 InSlotIndex, FName InItemID, UTexture2D* InIcon, const FText& InItemName, int32 InCount)
{
	SlotIndex = InSlotIndex;
	ItemID = InItemID;
	ItemName = InItemName;

	const bool bHasItem = !ItemID.IsNone();
	const bool bUsable = bHasItem && InCount > 0;

	if (Txt_Key)
	{
		Txt_Key->SetText(FText::AsNumber(SlotIndex + 1));
	}

	if (Img_Icon)
	{
		if (bHasItem && InIcon)
		{
			Img_Icon->SetBrushFromTexture(InIcon);
			Img_Icon->SetRenderOpacity(bUsable ? 1.f : UnusableOpacity);
			Img_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Img_Icon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (Txt_Count)
	{
		// 1개짜리(열쇠 등)는 숫자 생략
		if (bHasItem && InCount > 1)
		{
			Txt_Count->SetText(FText::AsNumber(InCount));
			Txt_Count->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Txt_Count->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	OnSlotDataChanged(bHasItem, bUsable);
}

void ULCOQuickSlotEntryWidget::SetHighlighted(bool bInHighlighted)
{
	bHighlighted = bInHighlighted;

	if (Img_Highlight)
	{
		Img_Highlight->SetVisibility(bHighlighted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	OnHighlightChanged(bHighlighted);
}
