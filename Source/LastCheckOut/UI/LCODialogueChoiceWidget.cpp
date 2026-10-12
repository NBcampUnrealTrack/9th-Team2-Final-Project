#include "UI/LCODialogueChoiceWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void ULCODialogueChoiceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Choice)
	{
		Button_Choice->OnClicked.RemoveDynamic(this, &ThisClass::HandleChoiceClicked);
		Button_Choice->OnClicked.AddDynamic(this, &ThisClass::HandleChoiceClicked);
	}
}

void ULCODialogueChoiceWidget::SetChoice(FName InChoiceID, const FText& InChoiceText)
{
	ChoiceID = InChoiceID;

	if (Text_Choice)
	{
		Text_Choice->SetText(InChoiceText);
	}
}

void ULCODialogueChoiceWidget::HandleChoiceClicked()
{
	OnChoiceSelected.Broadcast(ChoiceID);
}
