#include "UI/LCOInventoryItemWidget.h"

#include "UI/LCOQuickSlotDragOperation.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void ULCOInventoryItemWidget::SetItem(FName InItemID, int32 InQuantity, const FText& InDisplayName, UTexture2D* InIcon)
{
	ItemID = InItemID;
	Quantity = InQuantity;
	DisplayName = InDisplayName;
	Icon = InIcon;

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

// ======================================================================
// 퀵슬롯 드래그
// ======================================================================

ULCOQuickSlotComponent* ULCOInventoryItemWidget::GetQuickSlotComponent() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr;
}

bool ULCOInventoryItemWidget::CanDragToQuickSlot() const
{
	const ULCOQuickSlotComponent* QuickSlot = GetQuickSlotComponent();
	return QuickSlot && QuickSlot->CanAssignItem(ItemID);
}

FReply ULCOInventoryItemWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 열쇠 / 증거품은 기존처럼 버튼 클릭으로 처리
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && CanDragToQuickSlot())
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void ULCOInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!CanDragToQuickSlot())
	{
		return;
	}

	ULCOQuickSlotDragOperation* Operation = NewObject<ULCOQuickSlotDragOperation>(this);
	Operation->ItemID = ItemID;
	Operation->SourceSlotIndex = INDEX_NONE;
	Operation->QuickSlot = GetQuickSlotComponent();
	Operation->Pivot = EDragPivot::CenterCenter;

	// 마우스를 따라다니는 미리보기 (같은 WBP로 복사본 생성)
	if (ULCOInventoryItemWidget* DragVisual = CreateWidget<ULCOInventoryItemWidget>(GetOwningPlayer(), GetClass()))
	{
		DragVisual->SetItem(ItemID, Quantity, DisplayName, Icon);
		DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
		Operation->DefaultDragVisual = DragVisual;
	}

	OutOperation = Operation;
}
