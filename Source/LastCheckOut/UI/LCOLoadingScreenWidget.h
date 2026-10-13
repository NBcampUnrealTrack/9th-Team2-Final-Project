#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOLoadingScreenWidget.generated.h"

class UTextBlock;

UCLASS(Abstract)
class LASTCHECKOUT_API ULCOLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULCOLoadingScreenWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Tip;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Loading")
	TArray<FName> LoadingTipKeys;

private:
	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	void SelectRandomTip();
	void RefreshTipText();
	void BindLocalizationEvents();
	void UnbindLocalizationEvents();

	UPROPERTY(Transient)
	FName CurrentTipKey;
};
