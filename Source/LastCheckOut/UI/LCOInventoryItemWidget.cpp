#include "UI/LCOInventoryItemWidget.h"

#include "UI/LCOQuickSlotDragOperation.h"
#include "UI/LCOQuickSlotEntryWidget.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventoryComponent.h"
#include "Item/ItemData.h"
#include "Localization/LCOLocalizationSubsystem.h"

void ULCOInventoryItemWidget::SetItem(FName InItemID, int32 InQuantity, const FText& InDisplayName, UTexture2D* InIcon)
{
	ItemID = InItemID;
	Quantity = InQuantity;
	DisplayName = InDisplayName;
	Icon = InIcon;

	if (ItemNameText)
	{
		ItemNameText->SetText(InDisplayName);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Quantity));
	}

	if (ItemIconImage)
	{
		if (InIcon)
		{
			ItemIconImage->SetBrushFromTexture(InIcon);
			ItemIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ItemIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	RefreshTooltip();
}

void ULCOInventoryItemWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;

	if (Img_Selected)
	{
		Img_Selected->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	OnSelectedChanged(bSelected);
}

// ======================================================================
// 퀵슬롯 드래그
// ======================================================================

ULCOQuickSlotComponent* ULCOInventoryItemWidget::GetQuickSlotComponent() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr;
}

bool ULCOInventoryItemWidget::CanDragToQuickSlot() const
{
	const ULCOQuickSlotComponent* QuickSlot = GetQuickSlotComponent();
	return QuickSlot && QuickSlot->CanAssignItem(ItemID);
}

FReply ULCOInventoryItemWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 열쇠 / 증거품은 기존처럼 버튼 클릭으로 처리
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && CanDragToQuickSlot())
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void ULCOInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	if (!CanDragToQuickSlot())
	{
		return;
	}

	ULCOQuickSlotDragOperation* Operation = NewObject<ULCOQuickSlotDragOperation>(this);
	Operation->ItemID = ItemID;
	Operation->SourceSlotIndex = INDEX_NONE;
	Operation->QuickSlot = GetQuickSlotComponent();
	Operation->Pivot = EDragPivot::CenterCenter;

	// 마우스를 따라다니는 미리보기
	// DragVisualClass 지정 시 퀵슬롯 칸과 같은 모양 (인벤토리 ↔ 퀵슬롯 이미지 통일)
	if (DragVisualClass)
	{
		if (ULCOQuickSlotEntryWidget* DragVisual = CreateWidget<ULCOQuickSlotEntryWidget>(GetOwningPlayer(), DragVisualClass))
		{
			DragVisual->SetSlotData(INDEX_NONE, ItemID, Icon, DisplayName, Quantity);
			DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
			Operation->DefaultDragVisual = DragVisual;
		}
	}
	else if (ULCOInventoryItemWidget* DragVisual = CreateWidget<ULCOInventoryItemWidget>(GetOwningPlayer(), GetClass()))
	{
		DragVisual->SetItem(ItemID, Quantity, DisplayName, Icon);
		DragVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
		Operation->DefaultDragVisual = DragVisual;
	}

	OutOperation = Operation;
}

// ======================================================================
// 마우스 올리기 → 테두리
// ======================================================================

void ULCOInventoryItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	SetSelected(true);
}

void ULCOInventoryItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	SetSelected(false);
}

// ======================================================================
// 툴팁 (마우스를 올리면 설명 창)
// ======================================================================

void ULCOInventoryItemWidget::RefreshTooltip()
{
	if (!TooltipWidgetClass || ItemID.IsNone())
	{
		SetToolTip(nullptr);
		return;
	}

	// 툴팁 위젯은 한 번만 만들고 내용만 바꿔서 재사용
	if (!ItemTooltipWidget)
	{
		ItemTooltipWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), TooltipWidgetClass);
	}

	if (!ItemTooltipWidget)
	{
		return;
	}

	if (UImage* TooltipIcon = Cast<UImage>(ItemTooltipWidget->GetWidgetFromName(TEXT("Img_TooltipIcon"))))
	{
		if (Icon)
		{
			TooltipIcon->SetBrushFromTexture(Icon);
			TooltipIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			TooltipIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (UTextBlock* TooltipName = Cast<UTextBlock>(ItemTooltipWidget->GetWidgetFromName(TEXT("Txt_TooltipName"))))
	{
		TooltipName->SetText(DisplayName);
	}

	if (UTextBlock* TooltipDescription = Cast<UTextBlock>(ItemTooltipWidget->GetWidgetFromName(TEXT("Txt_TooltipDescription"))))
	{
		TooltipDescription->SetText(ResolveDescription());
	}

	// 엔진 내장 툴팁: 마우스를 올리면 커서 옆에 표시, 드래그 중에는 자동으로 숨김
	SetToolTip(ItemTooltipWidget);
}

FText ULCOInventoryItemWidget::ResolveDescription() const
{
	// Items.csv에 "아이템ID_DESC" 키가 있으면 번역 문구 우선
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const ULCOLocalizationSubsystem* Localization = GameInstance->GetSubsystem<ULCOLocalizationSubsystem>())
		{
			const FName DescKey(*FString::Printf(TEXT("%s_DESC"), *ItemID.ToString()));
			const FText LocalizedDesc = Localization->GetItemText(DescKey);
			if (!LocalizedDesc.IsEmpty())
			{
				return LocalizedDesc;
			}
		}
	}

	// 없으면 DT_ItemData의 Description
	const APawn* Pawn = GetOwningPlayerPawn();
	const UInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	return ItemData ? ItemData->Description : FText::GetEmpty();
}
