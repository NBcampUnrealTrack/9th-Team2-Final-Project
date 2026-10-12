#include "UI/LCOQuickSlotDragOperation.h"

#include "QuickSlot/LCOQuickSlotComponent.h"

void ULCOQuickSlotDragOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	// 퀵슬롯 칸을 끌어서 바깥에 놓으면 해제 (인벤토리 아이템을 바깥에 놓은 건 무시)
	if (SourceSlotIndex == INDEX_NONE)
	{
		return;
	}

	if (ULCOQuickSlotComponent* QuickSlotComponent = QuickSlot.Get())
	{
		QuickSlotComponent->ClearSlot(SourceSlotIndex);
	}
}
