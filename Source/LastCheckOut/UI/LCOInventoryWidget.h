#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOInventoryWidget.generated.h"

class APlayerController;
class UButton;
class UInventoryComponent;
class UPanelWidget;
class UTextBlock;
class UTexture2D;
class ULCOInventoryItemWidget;
class ULCOQuickSlotComponent;
class ULCOQuickSlotWidget;

UCLASS(Abstract)
class LASTCHECKOUT_API ULCOInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Inventory")
	void OpenInventory();

	UFUNCTION(BlueprintCallable, Category = "LCO|Inventory")
	void CloseInventory();

	UFUNCTION(BlueprintCallable, Category = "LCO|Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "LCO|Inventory")
	void RefreshItems();

	void UnbindFromInventoryInput();

	UFUNCTION(BlueprintPure, Category = "LCO|Inventory")
	bool IsInventoryOpen() const { return IsInViewport(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Inventory")
	TSubclassOf<ULCOInventoryItemWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Inventory")
	int32 ViewportZOrder = 7;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Inventory")
	bool bRestoreGameInputOnClose = true;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemsPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InventoryTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	// 인벤토리 창 안의 퀵슬롯 4칸 (LCOQuickSlotWidget 자식 WBP를 이 이름으로 배치)
	// 소비 아이템을 끌어다 놓으면 등록, 칸끼리 끌면 교체, 바깥에 놓거나 우클릭하면 해제
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULCOQuickSlotWidget> QuickSlotPanel;

private:
	UFUNCTION()
	void HandleInventoryRequested();

	UFUNCTION()
	void HandleCloseButtonClicked();

	void ApplyInventoryInputMode();
	void RestoreGameInputMode();
	UInventoryComponent* GetPlayerInventory() const;
	FText ResolveItemName(FName ItemID, const UInventoryComponent* Inventory) const;
	UTexture2D* ResolveItemIcon(FName ItemID, const UInventoryComponent* Inventory) const;
	void RefreshLocalizedTexts();

	// 퀵슬롯 패널 갱신 / 슬롯 변경 이벤트 연결
	UFUNCTION()
	void HandleQuickSlotsChanged();

	void RefreshQuickSlotPanel();
	void BindQuickSlotEvents();
	void UnbindQuickSlotEvents();
	ULCOQuickSlotComponent* GetPlayerQuickSlot() const;

	TWeakObjectPtr<ULCOQuickSlotComponent> BoundQuickSlot;

	UPROPERTY(Transient)
	bool bInventoryInputModeApplied = false;
};
