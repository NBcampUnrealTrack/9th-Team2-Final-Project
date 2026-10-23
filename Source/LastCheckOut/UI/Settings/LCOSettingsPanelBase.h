#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOSettingsPanelBase.generated.h"

class UTextBlock;
class ULCOLocalizationSubsystem;
class ULCOGameSettingsSubsystem;

// 설정창 탭 패널 공통 부모 (그래픽 / 게임플레이 / 오디오 / 조작)
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOSettingsPanelBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	virtual void RefreshLocalizedTexts() {}

	ULCOLocalizationSubsystem* GetLocalizationSubsystem() const;
	ULCOGameSettingsSubsystem* GetGameSettingsSubsystem() const;
	
	FText GetUIText(FName Key) const;

	void SetUIText(UTextBlock* TextBlock, FName Key) const;

private:
	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);
};
