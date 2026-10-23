#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/LCOSettingsPanelBase.h"
#include "InputCoreTypes.h"
#include "LCOControlsSettingsPanel.generated.h"

class UButton;
class UPanelWidget;
class UInputMappingContext;
class ULCOKeyBindingRowWidget;

// 설정창 조작 탭
UCLASS()
class LASTCHECKOUT_API ULCOControlsSettingsPanel : public ULCOSettingsPanelBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshLocalizedTexts() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ControlsTitleText;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> KeyBindingList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyBindingMessageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetKeysButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResetKeysButtonText;
	
	UPROPERTY(EditAnywhere, Category = "LCO|Settings")
	TSubclassOf<ULCOKeyBindingRowWidget> KeyBindingRowClass;
	
	UPROPERTY(EditAnywhere, Category = "LCO|Settings")
	TArray<TObjectPtr<UInputMappingContext>> KeyBindingMappingContexts;

private:
	UFUNCTION()
	void HandleKeyBindingRowKeySelected(FName MappingName, FKey NewKey);

	UFUNCTION()
	void HandleResetKeysClicked();

	void RegisterKeyBindingMappingContexts();
	void RefreshKeyBindings();
	void UnbindKeyBindingRows();
	FText GetKeyBindingDisplayName(FName MappingName, const FText& Fallback) const;
	void SetKeyBindingMessage(const FText& Message);

	UPROPERTY()
	TArray<TObjectPtr<ULCOKeyBindingRowWidget>> KeyBindingRows;
};
