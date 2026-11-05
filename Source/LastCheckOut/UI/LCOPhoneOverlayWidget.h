#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOPhoneOverlayWidget.generated.h"

class UBorder;
class UTexture2D;

// Asset-free viewport layer: below dialogue for fades, above it for tutorial cards.
UCLASS()
class LASTCHECKOUT_API ULCOPhoneOverlayWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(const FText& Title, UTexture2D* Texture, bool bTutorial, float HoldSeconds = 0.7f);
	void SetFadeOpacity(float Opacity);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;
};
