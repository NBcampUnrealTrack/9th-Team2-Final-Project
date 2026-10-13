#include "UI/LCOInventoryItemWidget.h"

#include "UI/LCOQuickSlotDragOperation.h"
#include "UI/LCOQuickSlotEntryWidget.h"
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

void ULCOInventoryItemWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;

	if (Img_Selected)
	{
		Img_Selected->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	OnSelectedChanged(bSelected);
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
	// 좌클릭: 선택 → 상세 설명 (드래그 감지가 버튼 클릭을 가져가므로 누르는 순간 처리)
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnItemSelected.Broadcast(ItemID);
	}

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

	// 마우스를 따라다니는 미리보기
	// DragVisualClass 지정 시 퀵슬롯 칸과 같은 모양 (인벤토리 ↔ 퀵슬롯 이미지 통일)
	if (DragVisualClass)
	{
		if (ULCOQuickSlotEntryWidget* DragVisual = CreateWidget<ULCOQuickSlotEntryWidget>(GetOwningPlayer(), DragVisualClass))
		{
			DragVisual->SetSlotData(INDEX_NONE, ItemID, Icon, DisplayName, Quantity);
			DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
			Operation->DefaultDragVisual = DragVisual;
		}
	}
	else if (ULCOInventoryItemWidget* DragVisual = CreateWidget<ULCOInventoryItemWidget>(GetOwningPlayer(), GetClass()))
	{
		DragVisual->SetItem(ItemID, Quantity, DisplayName, Icon);
		DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
		Operation->DefaultDragVisual = DragVisual;
	}

	OutOperation = Operation;
}
