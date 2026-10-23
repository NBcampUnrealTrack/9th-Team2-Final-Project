#pragma once

#include "CoreMinimal.h"
#include "UI/Settings/LCOSettingsPanelBase.h"
#include "LCOAudioSettingsPanel.generated.h"

class USlider;
class ULCOAudioSettingsSubsystem;

// 설정창 오디오 탭
UCLASS()
class LASTCHECKOUT_API ULCOAudioSettingsPanel : public ULCOSettingsPanelBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void RefreshLocalizedTexts() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AudioTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MasterVolumeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BGMVolumeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SFXVolumeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VoiceVolumeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> MasterVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> BGMVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> SFXVolumeSlider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USlider> VoiceVolumeSlider;

private:
	UFUNCTION()
	void HandleMasterVolumeChanged(float Value);

	UFUNCTION()
	void HandleBGMVolumeChanged(float Value);

	UFUNCTION()
	void HandleSFXVolumeChanged(float Value);

	UFUNCTION()
	void HandleVoiceVolumeChanged(float Value);

	UFUNCTION()
	void HandleAudioSliderCaptureEnd();

	void ResolveAudioSliders();
	void BindAudioSliders();
	void UnbindAudioSliders();
	void SaveAudioSettings();
	ULCOAudioSettingsSubsystem* GetAudioSettingsSubsystem() const;
	void InitializeAudioSliders();

	bool bUpdatingAudioSliders = false;
	bool bAudioSettingsDirty = false;
};
