#pragma once

#include "CoreMinimal.h"
#include "AudioDeviceHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOAudioSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;
class FAudioDevice;

UENUM(BlueprintType)
enum class ELCOAudioVolumeChannel : uint8
{
	Master,
	BGM,
	SFX,
	Voice
};

UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	ULCOAudioSettingsSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "LCO|Audio")
	void SetVolume(ELCOAudioVolumeChannel Channel, float Volume);

	UFUNCTION(BlueprintPure, Category = "LCO|Audio")
	float GetVolume(ELCOAudioVolumeChannel Channel) const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Audio")
	void ApplyAudioSettings();

	UFUNCTION(BlueprintCallable, Category = "LCO|Audio")
	void SaveAudioSettings();

private:
	void LoadAudioSettings();
	void HandleWorldRegisteredToAudioDevice(const UWorld* World, Audio::FDeviceId AudioDeviceId);
	bool ShouldHandleWorld(const UWorld* World) const;
	void ApplyAudioSettingsForWorld(UWorld* World);
	bool EnsureSoundMixActive(UWorld* World, USoundMix* SoundMix);
	void ApplyClassVolume(UWorld* World, USoundMix* SoundMix, USoundClass* SoundClass, float Volume, const TCHAR* DebugName, bool bApplyToChildren);
	USoundMix* LoadSoundMix() const;
	USoundClass* LoadSoundClass(const TSoftObjectPtr<USoundClass>& SoundClass, const TCHAR* DebugName) const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Audio")
	TSoftObjectPtr<USoundMix> GameAudioSoundMix;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Audio")
	TSoftObjectPtr<USoundClass> BGMSoundClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Audio")
	TSoftObjectPtr<USoundClass> SFXSoundClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Audio")
	TSoftObjectPtr<USoundClass> VoiceSoundClass;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> SoundMixPushedWorld;

	Audio::FDeviceId SoundMixPushedAudioDeviceId = Audio::FDeviceId(INDEX_NONE);
	FDelegateHandle AudioDeviceRegistrationHandle;

	float MasterVolume = 1.0f;
	float BGMVolume = 1.0f;
	float SFXVolume = 1.0f;
	float VoiceVolume = 1.0f;

	static const FString SaveSlotName;
	static constexpr int32 SaveUserIndex = 0;
};
