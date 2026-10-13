#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOInventoryItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class ULCOQuickSlotComponent;
class ULCOQuickSlotEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOInventoryItemSelected, FName, ItemID);

// 인벤토리 아이템 한 칸
// 소비 아이템은 끌어서 인벤토리 창의 퀵슬롯 칸(QuickSlotPanel)에 놓으면 등록됨
// 좌클릭하면 OnItemSelected → 인벤토리 창의 상세 설명 패널 표시
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

	// 선택 표시 (Img_Selected 보이기 + BP 이벤트)
	UFUNCTION(BlueprintCallable, Category = "LCO|Inventory")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "LCO|Inventory")
	bool IsSelected() const { return bSelected; }

	// 좌클릭으로 선택했을 때 (인벤토리 창이 바인딩해서 상세 설명 표시)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Inventory")
	FOnLCOInventoryItemSelected OnItemSelected;

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

	// 선택 테두리 / 배경 (선택된 아이템만 표시) - 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Selected;

	// 선택 상태가 바뀔 때 (BP 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Inventory")
	void OnSelectedChanged(bool bInSelected);

	// 드래그할 때 마우스를 따라다니는 이미지 (퀵슬롯 칸과 같은 WBP를 지정하면 모양 통일)
	// 비워두면 이 아이템 WBP를 그대로 복사해서 사용
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Inventory")
	TSubclassOf<ULCOQuickSlotEntryWidget> DragVisualClass;

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

	bool bSelected = false;
};
