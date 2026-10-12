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
	void SetVolume(ELCOAudioVolumeChannel Channel, float Volume, bool bSaveImmediately = true);

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
	void ApplyAudioSettingsForWorld(const UWorld* World);
	bool EnsureSoundMixActive(const UWorld* World, USoundMix* SoundMix);
	void ApplyClassVolume(const UWorld* World, USoundMix* SoundMix, USoundClass* SoundClass, float Volume, const TCHAR* DebugName, bool bApplyToChildren);
	USoundMix* LoadSoundMix();
	USoundClass* LoadSoundClass(TObjectPtr<USoundClass>& LoadedSoundClass, const TSoftObjectPtr<USoundClass>& SoundClass, const TCHAR* DebugName, bool& bMissingAssetWarningLogged);

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
	TObjectPtr<USoundMix> LoadedGameAudioSoundMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> LoadedMasterSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> LoadedBGMSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> LoadedSFXSoundClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> LoadedVoiceSoundClass;

	uint32 SoundMixPushedWorldId = 0;
	Audio::FDeviceId SoundMixPushedAudioDeviceId = Audio::FDeviceId(INDEX_NONE);
	FDelegateHandle AudioDeviceRegistrationHandle;

	bool bMissingSoundMixWarningLogged = false;
	bool bMissingMasterSoundClassWarningLogged = false;
	bool bMissingBGMSoundClassWarningLogged = false;
	bool bMissingSFXSoundClassWarningLogged = false;
	bool bMissingVoiceSoundClassWarningLogged = false;

	float MasterVolume = 1.0f;
	float BGMVolume = 1.0f;
	float SFXVolume = 1.0f;
	float VoiceVolume = 1.0f;

	static const FString SaveSlotName;
	static constexpr int32 SaveUserIndex = 0;
};
