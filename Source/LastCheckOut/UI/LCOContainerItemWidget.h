#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOContainerItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOContainerItemClicked, FName, ItemID, int32, Quantity);

// 상자 아이템 한 칸
// 마우스를 올리면 테두리(Img_Selected) + 설명 툴팁(TooltipWidgetClass, 인벤토리와 같은 WBP_ItemTooltip 사용)
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

	// 마우스를 올리면 테두리 표시 (툴팁과 같이 동작)
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ItemIconImage;

	// 마우스를 올렸을 때 테두리 - 없어도 동작 (평소 Hidden)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Selected;

	// 마우스를 올렸을 때 뜨는 설명 창 (WBP_ItemTooltip). 비워두면 툴팁 없음
	// WBP 안에 Img_TooltipIcon / Txt_TooltipName / Txt_TooltipDescription 이름으로 배치 (전부 선택)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Container")
	TSubclassOf<UUserWidget> TooltipWidgetClass;

private:
	UFUNCTION()
	void HandleItemButtonClicked();

	// 테두리 보이기 / 숨기기
	void SetHovered(bool bHovered);

	// 툴팁 생성 / 내용 채우기 (SetItem에서 호출)
	void RefreshTooltip();

	// 설명: Items.csv의 "아이템ID_DESC" → 없으면 DT_ItemData의 Description
	FText ResolveDescription() const;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ItemTooltipWidget;

	UPROPERTY(Transient)
	FText DisplayName;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(Transient)
	FName ItemID;

	UPROPERTY(Transient)
	int32 Quantity = 0;
};
