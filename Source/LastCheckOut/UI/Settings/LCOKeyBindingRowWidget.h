#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Commands/InputChord.h"
#include "LCOKeyBindingRowWidget.generated.h"

class UTextBlock;
class UInputKeySelector;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOKeyBindingRowKeySelected, FName, MappingName, FKey, NewKey);

UCLASS()
class LASTCHECKOUT_API ULCOKeyBindingRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetKeyBinding(FName InMappingName, const FText& InDisplayName, const FKey& InKey, const FText& KeySelectionText);

	FName GetMappingName() const { return MappingName; }
	
	UPROPERTY(BlueprintAssignable, Category = "LCO|Settings")
	FOnLCOKeyBindingRowKeySelected OnKeySelected;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ActionNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInputKeySelector> KeySelector;

private:
	UFUNCTION()
	void HandleKeySelected(FInputChord SelectedKey);

	FName MappingName;
	bool bUpdatingKey = false;
};
