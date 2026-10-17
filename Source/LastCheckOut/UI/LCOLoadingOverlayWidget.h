#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOLoadingOverlayWidget.generated.h"

// A native UMG host for exactly the same Slate design as the MoviePlayer screen.
// Mounted directly in the viewport overlay to avoid a second UMG DPI scale.
UCLASS()
class LASTCHECKOUT_API ULCOLoadingOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeLoadingScreen(const FText& InTipText, double InStartTime);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	FText TipText;

	double StartTime = 0.0;
};
