#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOQuickSlotWidget.generated.h"

class ULCOQuickSlotComponent;
class ULCOQuickSlotEntryWidget;
class UTextBlock;

/**
 * 퀵슬롯 4칸 묶음 위젯 - HUD와 원형 휠이 같은 부모 클래스를 씀
 *   WBP_QuickSlotHUD   : 화면 구석에 1~4 가로 배치 (달릴 때만 표시)
 *   WBP_QuickSlotWheel : 화면 중앙에 위/오른쪽/아래/왼쪽 = 1/2/3/4 배치 (Q 꾹)
 *
 * 디자이너에서 WBP_QuickSlotEntry 4개를 Slot_1 ~ Slot_4 이름으로 배치 (필수)
 * Txt_SelectedName (선택) : 휠에서 현재 선택한 아이템 이름 표시
 *
 * 표시/숨김은 ShowQuickSlot / HideQuickSlot - BP에서 오버라이드하면 페이드 애니메이션 가능
 * (오버라이드 시 Parent 호출 또는 직접 Visibility 처리 필요)
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 컴포넌트의 슬롯 정보로 4칸 갱신
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void RefreshSlots(const ULCOQuickSlotComponent* QuickSlot);

	// 휠 선택 표시 (-1 = 선택 없음)
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void SetSelectedIndex(int32 NewIndex);

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|QuickSlot")
	void ShowQuickSlot();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|QuickSlot")
	void HideQuickSlot();

protected:
	// BP 연출용 (선택 바뀔 때 틱 사운드 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|QuickSlot")
	void OnSelectedIndexChanged(int32 NewIndex);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULCOQuickSlotEntryWidget> Slot_1;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULCOQuickSlotEntryWidget> Slot_2;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULCOQuickSlotEntryWidget> Slot_3;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULCOQuickSlotEntryWidget> Slot_4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SelectedName;

private:
	ULCOQuickSlotEntryWidget* GetEntry(int32 Index) const;
	void UpdateSelectedName();

	int32 SelectedIndex = INDEX_NONE;
};
