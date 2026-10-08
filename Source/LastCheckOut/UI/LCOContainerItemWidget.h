#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOContainerItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOContainerItemClicked, FName, ItemID, int32, Quantity);

UCLASS(Abstract)
class LASTCHECKOUT_API ULCOContainerItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Container")
	void SetItem(FName InItemID, int32 InQuantity, const FText& InDisplayName, UTexture2D* InIcon);

	UFUNCTION(BlueprintPure, Category = "LCO|Container")
	FName GetItemID() const { return ItemID; }

	UFUNCTION(BlueprintPure, Category = "LCO|Container")
	int32 GetQuantity() const { return Quantity; }

	UPROPERTY(BlueprintAssignable, Category = "LCO|Container")
	FOnLCOContainerItemClicked OnItemClicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

private:
	UFUNCTION()
	void HandleItemButtonClicked();

	UPROPERTY(Transient)
	FName ItemID;

	UPROPERTY(Transient)
	int32 Quantity = 0;
};
