#include "UI/LCOInteractionWidget.h"

#include "Components/TextBlock.h"

void ULCOInteractionWidget::ShowInteraction(const FText& Prompt)
{
	if (PromptTextBlock)
	{
		PromptTextBlock->SetText(Prompt);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ULCOInteractionWidget::HideInteraction()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
