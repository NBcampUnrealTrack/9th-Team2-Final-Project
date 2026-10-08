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

	UPROPERTY(Transient)
	bool bInventoryInputModeApplied = false;
};
