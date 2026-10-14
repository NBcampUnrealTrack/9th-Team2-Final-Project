#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOSenseWidget.generated.h"

class ALCOPlayerCharacter;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UWidget;

/**
 * Sense HUD base widget.
 * BP supplies the visual layout; this class only pushes charge/hold state.
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOSenseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULCOSenseWidget(const FObjectInitializer& ObjectInitializer);

	void BindToCharacter(ALCOPlayerCharacter* Character);
	void UnbindFromCharacter();

	void NotifySenseInputStarted();
	void NotifySenseInputReleased();
	void RefreshHoldState();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Sense")
	void BP_OnChargesChanged(int32 CurrentCharges, int32 MaxCharges);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Sense")
	void BP_OnChargesVisibilityChanged(bool bVisible);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Sense")
	void BP_OnHoldProgressChanged(float Progress);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Sense")
	void BP_OnHoldVisibilityChanged(bool bVisible);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Sense", meta = (ClampMin = "0.0"))
	float ChargesVisibleDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Sense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChargeActiveOpacity = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Sense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChargeInactiveOpacity = 0.25f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SenseChargesPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ChargeIconContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SenseCharges;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SenseHoldPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Progress_SenseHold;

private:
	UFUNCTION()
	void HandleSenseChargesChanged(int32 CurrentCharges, int32 MaxCharges);

	UFUNCTION()
	void HandleSenseUsed(bool bSuccess);

	void SetChargesVisible(bool bVisible);
	void SetHoldVisible(bool bVisible);
	void HideCharges();
	void SyncChargesFromCharacter();
	void UpdateCharges(int32 CurrentCharges, int32 MaxCharges);
	void UpdateHoldProgress(float Progress);

	FTimerHandle ChargesVisibilityTimer;

	TWeakObjectPtr<ALCOPlayerCharacter> BoundCharacter;
	bool bChargesVisible = false;
	bool bHoldVisible = false;
};
