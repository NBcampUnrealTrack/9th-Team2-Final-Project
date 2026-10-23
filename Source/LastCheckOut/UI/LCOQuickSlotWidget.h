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
 *                          선택 없음(가운데)일 때는 NoSelectionTextKey 문구 ("맨손") 표시
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
	void RefreshSlots(ULCOQuickSlotComponent* QuickSlot);
	
	
	// 위젯의 용도를 설정한다.
	// false: 일반 HUD 슬롯 표시
	// true : Q 소비 아이템 휠 표시
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void SetWheelMode(bool bInWheelMode);

	// 휠 선택 표시 (-1 = 선택 없음)
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void SetSelectedIndex(int32 NewIndex);

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	// 인벤토리 창 안에 배치한 경우: 4칸 모두 드래그 앤 드롭 / 우클릭 해제 가능하게
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void SetEntriesInteractive(bool bInteractive);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|QuickSlot")
	void ShowQuickSlot();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|QuickSlot")
	void HideQuickSlot();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

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

	// 선택 없음(휠 가운데)일 때 Txt_SelectedName에 표시할 UI.csv 키 (가운데에서 떼면 맨손). None이면 빈칸
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|QuickSlot")
	FName NoSelectionTextKey = TEXT("QUICKSLOT_BARE_HAND");

private:
	ULCOQuickSlotEntryWidget* GetEntry(int32 Index) const;
	FText ResolveItemName(FName ItemID, const FText& FallbackName) const;
	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);
	void BindLocalizationEvents();
	void UnbindLocalizationEvents();
	void UpdateSelectedName();

	TWeakObjectPtr<ULCOQuickSlotComponent> CachedQuickSlot;
	int32 SelectedIndex = INDEX_NONE;
	
	// 현재 위젯이 Q 휠인지 여부
	// HUD와 Q 휠은 같은 위젯 클래스를 사용하므로 용도를 구분한다.
	UPROPERTY(Transient)
	bool bIsWheelMode = false;
};
