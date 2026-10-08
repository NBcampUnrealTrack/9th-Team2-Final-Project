#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOInventoryItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

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
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

private:
	UPROPERTY(Transient)
	FName ItemID;

	UPROPERTY(Transient)
	int32 Quantity = 0;
};
