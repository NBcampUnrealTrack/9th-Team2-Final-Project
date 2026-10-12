#include "UI/LCOQuickSlotEntryWidget.h"

#include "UI/LCOQuickSlotDragOperation.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void ULCOQuickSlotEntryWidget::SetSlotData(int32 InSlotIndex, FName InItemID, UTexture2D* InIcon, const FText& InItemName, int32 InCount)
{
	SlotIndex = InSlotIndex;
	ItemID = InItemID;
	ItemName = InItemName;
	Icon = InIcon;
	Count = InCount;

	const bool bHasItem = !ItemID.IsNone();
	const bool bUsable = bHasItem && InCount > 0;

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

// ======================================================================
// 인벤토리 창 드래그 앤 드롭
// ======================================================================

void ULCOQuickSlotEntryWidget::SetInteractive(bool bInInteractive)
{
	bInteractive = bInInteractive;

	// 마우스 / 드롭 이벤트를 받으려면 Visible이어야 함
	if (bInteractive)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

ULCOQuickSlotComponent* ULCOQuickSlotEntryWidget::GetQuickSlotComponent() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr;
}

FReply ULCOQuickSlotEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bInteractive || ItemID.IsNone())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// 우클릭: 해제
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (ULCOQuickSlotComponent* QuickSlot = GetQuickSlotComponent())
		{
			QuickSlot->ClearSlot(SlotIndex);
		}
		return FReply::Handled();
	}

	// 좌클릭 후 끌면 드래그 시작
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void ULCOQuickSlotEntryWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!bInteractive || ItemID.IsNone())
	{
		return;
	}

	ULCOQuickSlotDragOperation* Operation = NewObject<ULCOQuickSlotDragOperation>(this);
	Operation->ItemID = ItemID;
	Operation->SourceSlotIndex = SlotIndex;
	Operation->QuickSlot = GetQuickSlotComponent();
	Operation->Pivot = EDragPivot::CenterCenter;

	// 마우스를 따라다니는 미리보기 (같은 WBP로 복사본 생성)
	if (ULCOQuickSlotEntryWidget* DragVisual = CreateWidget<ULCOQuickSlotEntryWidget>(GetOwningPlayer(), GetClass()))
	{
		DragVisual->SetSlotData(SlotIndex, ItemID, Icon, ItemName, Count);
		DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
		Operation->DefaultDragVisual = DragVisual;
	}

	OutOperation = Operation;
}

void ULCOQuickSlotEntryWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (bInteractive && Cast<ULCOQuickSlotDragOperation>(InOperation))
	{
		SetHighlighted(true);
	}
}

void ULCOQuickSlotEntryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	if (bInteractive)
	{
		SetHighlighted(false);
	}
}

bool ULCOQuickSlotEntryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ULCOQuickSlotDragOperation* Operation = Cast<ULCOQuickSlotDragOperation>(InOperation);
	if (!bInteractive || !Operation)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	SetHighlighted(false);

	ULCOQuickSlotComponent* QuickSlot = GetQuickSlotComponent();
	if (!QuickSlot || SlotIndex == INDEX_NONE)
	{
		return true;
	}

	if (Operation->SourceSlotIndex == INDEX_NONE)
	{
		// 인벤토리 → 칸: 등록 (소비 아이템이 아니면 AssignSlot에서 거부)
		QuickSlot->AssignSlot(SlotIndex, Operation->ItemID);
	}
	else
	{
		// 칸 → 칸: 순서 교체 (같은 칸이면 SwapSlots에서 무시)
		QuickSlot->SwapSlots(Operation->SourceSlotIndex, SlotIndex);
	}

	return true;
}
