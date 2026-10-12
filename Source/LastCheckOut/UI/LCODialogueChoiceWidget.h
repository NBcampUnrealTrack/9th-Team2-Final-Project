#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCODialogueChoiceWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCODialogueChoiceSelected, FName, ChoiceID);

UCLASS(Abstract)
class LASTCHECKOUT_API ULCODialogueChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	void SetChoice(FName InChoiceID, const FText& InChoiceText);

	UPROPERTY(BlueprintAssignable, Category = "LCO|Dialogue")
	FOnLCODialogueChoiceSelected OnChoiceSelected;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Choice;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Choice;

private:
	UFUNCTION()
	void HandleChoiceClicked();

	UPROPERTY(Transient)
	FName ChoiceID = NAME_None;
};
