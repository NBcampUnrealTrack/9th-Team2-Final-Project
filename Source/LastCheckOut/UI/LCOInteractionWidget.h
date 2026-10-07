#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOInteractionWidget.generated.h"

class UTextBlock;

UCLASS()
class LASTCHECKOUT_API ULCOInteractionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Interaction UI")
	void ShowInteraction(const FText& Prompt);

	UFUNCTION(BlueprintCallable, Category = "LCO|Interaction UI")
	void HideInteraction();

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptTextBlock;
};
