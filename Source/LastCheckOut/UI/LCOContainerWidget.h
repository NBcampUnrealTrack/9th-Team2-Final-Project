#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOContainerWidget.generated.h"

class ALCOContainer;
class APlayerController;
class UButton;
class UPanelWidget;
class UTextBlock;
class UTexture2D;
class ULCOContainerItemWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOContainerWidgetClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLCOContainerWidgetItemTaken, ALCOContainer*, Container, FName, ItemID, int32, Quantity);

UCLASS(Abstract)
class LASTCHECKOUT_API ULCOContainerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Container")
	void OpenContainer(ALCOContainer* InContainer, AActor* InInteractor);

	UFUNCTION(BlueprintCallable, Category = "LCO|Container")
	void CloseContainer();

	UFUNCTION(BlueprintCallable, Category = "LCO|Container")
	void RefreshItems();

	UFUNCTION(BlueprintCallable, Category = "LCO|Container")
	bool TakeContainerItem(FName ItemID, int32 Quantity = 1);

	UFUNCTION(BlueprintPure, Category = "LCO|Container")
	ALCOContainer* GetContainer() const { return Container.Get(); }

	UFUNCTION(BlueprintPure, Category = "LCO|Container")
	AActor* GetInteractor() const { return Interactor.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "LCO|Container")
	FOnLCOContainerWidgetClosed OnContainerWidgetClosed;

	UPROPERTY(BlueprintAssignable, Category = "LCO|Container")
	FOnLCOContainerWidgetItemTaken OnContainerWidgetItemTaken;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Container")
	TSubclassOf<ULCOContainerItemWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Container", meta = (ClampMin = "1"))
	int32 QuantityToTakeOnClick = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Container")
	int32 ViewportZOrder = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Container")
	bool bRestoreGameInputOnClose = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Container")
	bool bBlockLookInputWhileOpen = true;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemsPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

private:
	UFUNCTION()
	void HandleCloseButtonClicked();

	UFUNCTION()
	void HandleItemClicked(FName ItemID, int32 ListedQuantity);

	void ApplyContainerInputMode();
	void RestoreGameInputMode();
	bool ShouldRestoreGameOnlyInputMode(APlayerController* PC) const;
	FText ResolveItemName(FName ItemID) const;
	UTexture2D* ResolveItemIcon(FName ItemID) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALCOContainer> Container;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Interactor;

	UPROPERTY(Transient)
	bool bContainerInputModeApplied = false;

	UPROPERTY(Transient)
	bool bPreviousShowMouseCursor = false;

	UPROPERTY(Transient)
	bool bLookInputBlockedByThisWidget = false;
};
