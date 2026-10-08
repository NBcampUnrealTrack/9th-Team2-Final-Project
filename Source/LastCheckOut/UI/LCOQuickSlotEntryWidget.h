#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOQuickSlotEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/**
 * 퀵슬롯 한 칸 (WBP_QuickSlotEntry의 부모)
 * HUD와 휠 둘 다 이 위젯을 4개씩 배치해서 사용
 * 위젯 이름을 아래와 똑같이 맞추면 자동 연결 (전부 선택사항)
 *   Img_Icon      : 아이템 아이콘
 *   Img_Highlight : 선택 테두리/배경 (휠에서 선택 시 표시)
 *   Txt_Key       : 슬롯 번호 "1"~"4"
 *   Txt_Count     : 보유 수량 (2개 이상일 때만 표시)
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOQuickSlotEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 슬롯 데이터 반영. ItemID가 None이면 빈 칸, Count가 0이면 흐리게 표시
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void SetSlotData(int32 InSlotIndex, FName InItemID, UTexture2D* InIcon, const FText& InItemName, int32 InCount);

	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void SetHighlighted(bool bInHighlighted);

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	FName GetItemID() const { return ItemID; }

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	FText GetItemName() const { return ItemName; }

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	bool IsHighlighted() const { return bHighlighted; }

protected:
	// BP에서 연출 추가용 (스케일 애니메이션, 사운드 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|QuickSlot")
	void OnSlotDataChanged(bool bHasItem, bool bUsable);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|QuickSlot")
	void OnHighlightChanged(bool bInHighlighted);

	// 보유 수량 0 / 빈 칸일 때 아이콘 투명도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|QuickSlot")
	float UnusableOpacity = 0.35f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Icon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Highlight;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Key;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Count;

private:
	int32 SlotIndex = INDEX_NONE;
	FName ItemID = NAME_None;
	FText ItemName;
	bool bHighlighted = false;
};
