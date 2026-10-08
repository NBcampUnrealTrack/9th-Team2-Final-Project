#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCONotebookListItemWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCONotebookListItemClicked, int32, ItemIndex);

/**
 * 수첩 목록 한 줄 (WBP_NotebookListItem의 부모)
 * 왼쪽 페이지 항목 목록, 지도 탭 구역 버튼에 공용으로 사용
 *   Btn_Item     (필수) : 클릭 영역
 *   Txt_Title    (선택) : 항목 이름
 *   Img_NewMark  (선택) : 새 항목 표시 (빨간 점 등)
 *   Img_Selected (선택) : 선택된 줄 표시 (밑줄, 동그라미 표시 등)
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCONotebookListItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void SetItemData(int32 InItemIndex, const FText& InTitle, bool bInIsNew);

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void SetIsNew(bool bInIsNew);

	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	int32 GetItemIndex() const { return ItemIndex; }

	UPROPERTY(BlueprintAssignable, Category = "LCO|Notebook")
	FOnLCONotebookListItemClicked OnItemClicked;

protected:
	virtual void NativeOnInitialized() override;

	// BP 연출용 (선택 시 글씨 색 변경 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Notebook")
	void OnSelectedChanged(bool bInSelected);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Item;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Title;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_NewMark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Selected;

private:
	UFUNCTION()
	void HandleItemClicked();

	int32 ItemIndex = INDEX_NONE;
};
