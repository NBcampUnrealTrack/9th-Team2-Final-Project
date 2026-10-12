#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "LCOQuickSlotDragOperation.generated.h"

class ULCOQuickSlotComponent;

/**
 * 퀵슬롯 드래그 앤 드롭 데이터 (인벤토리 창 안에서 사용)
 * - 인벤토리 아이템 → 퀵슬롯 칸 : 등록 (SourceSlotIndex = INDEX_NONE)
 * - 퀵슬롯 칸 → 다른 퀵슬롯 칸 : 순서 교체
 * - 퀵슬롯 칸 → 칸이 아닌 곳   : 해제
 */
UCLASS()
class LASTCHECKOUT_API ULCOQuickSlotDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// 끌고 있는 아이템
	UPROPERTY(BlueprintReadOnly, Category = "LCO|QuickSlot")
	FName ItemID;

	// 퀵슬롯 칸에서 끌기 시작했으면 그 칸 인덱스, 인벤토리에서 끌었으면 INDEX_NONE
	UPROPERTY(BlueprintReadOnly, Category = "LCO|QuickSlot")
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<ULCOQuickSlotComponent> QuickSlot;

	// 퀵슬롯 칸이 아닌 곳에 놓았을 때
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
};
