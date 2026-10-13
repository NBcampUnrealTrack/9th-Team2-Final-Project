#include "UI/LCOContainerItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryComponent.h"
#include "Item/ItemData.h"
#include "Localization/LCOLocalizationSubsystem.h"

void ULCOContainerItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ItemButton)
	{
		ItemButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleItemButtonClicked);
		ItemButton->OnClicked.AddDynamic(this, &ThisClass::HandleItemButtonClicked);
	}

	SetHovered(false);
}

void ULCOContainerItemWidget::SetItem(FName InItemID, int32 InQuantity, const FText& InDisplayName, UTexture2D* InIcon)
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

void ULCOContainerItemWidget::HandleItemButtonClicked()
{
	if (!ItemID.IsNone() && Quantity > 0)
	{
		OnItemClicked.Broadcast(ItemID, Quantity);
	}
}

// ======================================================================
// 마우스 올리기 → 테두리
// ======================================================================

void ULCOContainerItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	SetHovered(true);
}

void ULCOContainerItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	SetHovered(false);
}

void ULCOContainerItemWidget::SetHovered(bool bHovered)
{
	if (Img_Selected)
	{
		Img_Selected->SetVisibility(bHovered ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

// ======================================================================
// 툴팁 (마우스를 올리면 설명 창)
// ======================================================================

void ULCOContainerItemWidget::RefreshTooltip()
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

	// 엔진 내장 툴팁: 마우스를 올리면 커서 옆에 표시
	SetToolTip(ItemTooltipWidget);
}

FText ULCOContainerItemWidget::ResolveDescription() const
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

	// 없으면 DT_ItemData의 Description (플레이어 인벤토리의 아이템 테이블 사용)
	const APawn* Pawn = GetOwningPlayerPawn();
	const UInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	return ItemData ? ItemData->Description : FText::GetEmpty();
}
