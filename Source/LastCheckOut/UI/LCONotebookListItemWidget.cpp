#include "UI/LCONotebookListItemWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void ULCONotebookListItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Btn_Item)
	{
		Btn_Item->OnClicked.RemoveDynamic(this, &ThisClass::HandleItemClicked);
		Btn_Item->OnClicked.AddDynamic(this, &ThisClass::HandleItemClicked);
	}
}

void ULCONotebookListItemWidget::SetItemData(int32 InItemIndex, const FText& InTitle, bool bInIsNew)
{
	ItemIndex = InItemIndex;

	if (Txt_Title)
	{
		Txt_Title->SetText(InTitle);
	}

	SetIsNew(bInIsNew);
	SetSelected(false);
}

void ULCONotebookListItemWidget::SetSelected(bool bInSelected)
{
	if (Img_Selected)
	{
		Img_Selected->SetVisibility(bInSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	OnSelectedChanged(bInSelected);
}

void ULCONotebookListItemWidget::SetIsNew(bool bInIsNew)
{
	if (Img_NewMark)
	{
		Img_NewMark->SetVisibility(bInIsNew ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void ULCONotebookListItemWidget::HandleItemClicked()
{
	OnItemClicked.Broadcast(ItemIndex);
}
