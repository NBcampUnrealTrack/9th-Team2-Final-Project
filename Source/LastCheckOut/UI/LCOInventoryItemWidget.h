#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOInventoryItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class ULCOQuickSlotComponent;

// 인벤토리 아이템 한 칸
// 소비 아이템은 끌어서 인벤토리 창의 퀵슬롯 칸(QuickSlotPanel)에 놓으면 등록됨
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Inventory")
	void SetItem(FName InItemID, int32 InQuantity, const FText& InDisplayName, UTexture2D* InIcon);

	UFUNCTION(BlueprintPure, Category = "LCO|Inventory")
	FName GetItemID() const { return ItemID; }

	UFUNCTION(BlueprintPure, Category = "LCO|Inventory")
	int32 GetQuantity() const { return Quantity; }

protected:
	// 버튼이 클릭을 먼저 가져가므로 Preview 단계에서 드래그 감지 (소비 아이템만)
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

private:
	ULCOQuickSlotComponent* GetQuickSlotComponent() const;

	// 퀵슬롯에 넣을 수 있는 아이템인지 (소비 아이템)
	bool CanDragToQuickSlot() const;

	UPROPERTY(Transient)
	FText DisplayName;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(Transient)
	FName ItemID;

	UPROPERTY(Transient)
	int32 Quantity = 0;
};
