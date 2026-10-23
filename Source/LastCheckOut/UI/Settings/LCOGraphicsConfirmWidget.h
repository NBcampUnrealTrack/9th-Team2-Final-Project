#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOGraphicsConfirmWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOGraphicsConfirmFinished, bool, bKept);

// 그래픽 설정 확인 창
UCLASS()
class LASTCHECKOUT_API ULCOGraphicsConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "LCO|Settings")
	FOnLCOGraphicsConfirmFinished OnFinished;
	
	void RevertAndClose();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GraphicsConfirmText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GraphicsConfirmCountdownText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> GraphicsKeepButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GraphicsKeepButtonText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> GraphicsRevertButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GraphicsRevertButtonText;

	UPROPERTY(EditAnywhere, Category = "LCO|Settings", meta = (ClampMin = "1.0", Units = "s"))
	float GraphicsConfirmDuration = 5.f;

private:
	UFUNCTION()
	void HandleKeepClicked();

	UFUNCTION()
	void HandleRevertClicked();

	void Finish(bool bKeep);
	void RefreshCountdown();
	FText GetUIText(FName Key) const;

	float RemainingTime = 0.f;
	int32 ShownSeconds = INDEX_NONE;
	bool bFinished = false;
};
