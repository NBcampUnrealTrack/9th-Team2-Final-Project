#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOInventoryWidget.generated.h"

class APlayerController;
class UButton;
class UInventoryComponent;
class UPanelWidget;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;
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

	// ---------- 아이템 상세 설명 (아이템 좌클릭 시 표시, 전부 선택사항) ----------

	// 상세 패널 전체 (선택한 아이템이 없으면 숨김)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DetailPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_DetailIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DetailName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DetailDescription;

	// 상세 패널이 바뀔 때 (BP 연출용, ItemID가 None이면 닫힘)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Inventory")
	void OnItemDetailChanged(FName ItemID);

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

	// 설명: Items.csv의 "아이템ID_DESC" 키 → 없으면 DT_ItemData의 Description
	FText ResolveItemDescription(FName ItemID, const UInventoryComponent* Inventory) const;

	// 아이템 선택 → 상세 설명
	UFUNCTION()
	void HandleItemSelected(FName ItemID);

	void ShowItemDetail(FName ItemID);
	void HideItemDetail();
	void UpdateItemSelectionVisuals();

	// 현재 상세 설명을 보고 있는 아이템 (없으면 None)
	FName SelectedItemID = NAME_None;
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
