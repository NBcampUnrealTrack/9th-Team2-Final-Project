#include "UI/LCOAudioSettingsSubsystem.h"

#include "AudioDevice.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UI/LCOAudioSettingsSaveGame.h"
#include "UObject/SoftObjectPath.h"

const FString ULCOAudioSettingsSubsystem::SaveSlotName = TEXT("LCO_AudioSettings");

ULCOAudioSettingsSubsystem::ULCOAudioSettingsSubsystem()
{
	GameAudioSoundMix = TSoftObjectPtr<USoundMix>(FSoftObjectPath(TEXT("/Game/HJ/Sound_Test/SM_GameAudio.SM_GameAudio")));
	MasterSoundClass = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/HJ/Sound_Test/MasterClass/SC_Master.SC_Master")));
	BGMSoundClass = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/HJ/Sound_Test/BGM/SC_BGM.SC_BGM")));
	SFXSoundClass = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/HJ/Sound_Test/SFX/SC_SFX.SC_SFX")));
	VoiceSoundClass = TSoftObjectPtr<USoundClass>(FSoftObjectPath(TEXT("/Game/HJ/Sound_Test/Voice/SC_Voice.SC_Voice")));
}

void ULCOAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Initialize: World=%s"), *GetNameSafe(GetWorld()));
	LoadAudioSettings();

	if (!AudioDeviceRegistrationHandle.IsValid())
	{
		AudioDeviceRegistrationHandle = FAudioDeviceWorldDelegates::OnWorldRegisteredToAudioDevice.AddUObject(this, &ThisClass::HandleWorldRegisteredToAudioDevice);
	}
	ApplyAudioSettings();
}

void ULCOAudioSettingsSubsystem::Deinitialize()
{
	SaveAudioSettings();

	if (AudioDeviceRegistrationHandle.IsValid())
	{
		FAudioDeviceWorldDelegates::OnWorldRegisteredToAudioDevice.Remove(AudioDeviceRegistrationHandle);
		AudioDeviceRegistrationHandle.Reset();
	}

	SoundMixPushedWorldId = 0;
	SoundMixPushedAudioDeviceId = Audio::FDeviceId(INDEX_NONE);

	Super::Deinitialize();
}

void ULCOAudioSettingsSubsystem::SetVolume(ELCOAudioVolumeChannel Channel, float Volume, bool bSaveImmediately)
{
	const float ClampedVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	const TCHAR* ChannelName = TEXT("Unknown");
	switch (Channel)
	{
	case ELCOAudioVolumeChannel::Master:
		MasterVolume = ClampedVolume;
		ChannelName = TEXT("Master");
		break;
	case ELCOAudioVolumeChannel::BGM:
		BGMVolume = ClampedVolume;
		ChannelName = TEXT("BGM");
		break;
	case ELCOAudioVolumeChannel::SFX:
		SFXVolume = ClampedVolume;
		ChannelName = TEXT("SFX");
		break;
	case ELCOAudioVolumeChannel::Voice:
		VoiceVolume = ClampedVolume;
		ChannelName = TEXT("Voice");
		break;
	default:
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] SetVolume: %s=%.3f"), ChannelName, ClampedVolume);
	ApplyAudioSettings();
	if (bSaveImmediately)
	{
		SaveAudioSettings();
	}
}

float ULCOAudioSettingsSubsystem::GetVolume(ELCOAudioVolumeChannel Channel) const
{
	switch (Channel)
	{
	case ELCOAudioVolumeChannel::Master:
		return MasterVolume;
	case ELCOAudioVolumeChannel::BGM:
		return BGMVolume;
	case ELCOAudioVolumeChannel::SFX:
		return SFXVolume;
	case ELCOAudioVolumeChannel::Voice:
		return VoiceVolume;
	default:
		return 1.0f;
	}
}

void ULCOAudioSettingsSubsystem::ApplyAudioSettings()
{
	ApplyAudioSettingsForWorld(GetWorld());
}

void ULCOAudioSettingsSubsystem::ApplyAudioSettingsForWorld(const UWorld* World)
{
	USoundMix* SoundMix = LoadSoundMix();
	if (!SoundMix)
	{
		return;
	}

	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Cannot apply audio settings: no world"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] ApplyAudioSettings: World=%s Mix=%s Master=%.3f BGM=%.3f SFX=%.3f Voice=%.3f"),
		*GetNameSafe(World),
		*GetNameSafe(SoundMix),
		MasterVolume,
		BGMVolume,
		SFXVolume,
		VoiceVolume);

	if (!EnsureSoundMixActive(World, SoundMix))
	{
		return;
	}

	ApplyClassVolume(World, SoundMix, LoadSoundClass(LoadedMasterSoundClass, MasterSoundClass, TEXT("SC_Master"), bMissingMasterSoundClassWarningLogged), MasterVolume, TEXT("SC_Master"), true);
	ApplyClassVolume(World, SoundMix, LoadSoundClass(LoadedBGMSoundClass, BGMSoundClass, TEXT("SC_BGM"), bMissingBGMSoundClassWarningLogged), BGMVolume, TEXT("SC_BGM"), false);
	ApplyClassVolume(World, SoundMix, LoadSoundClass(LoadedSFXSoundClass, SFXSoundClass, TEXT("SC_SFX"), bMissingSFXSoundClassWarningLogged), SFXVolume, TEXT("SC_SFX"), false);
	ApplyClassVolume(World, SoundMix, LoadSoundClass(LoadedVoiceSoundClass, VoiceSoundClass, TEXT("SC_Voice"), bMissingVoiceSoundClassWarningLogged), VoiceVolume, TEXT("SC_Voice"), false);
}

void ULCOAudioSettingsSubsystem::SaveAudioSettings()
{
	ULCOAudioSettingsSaveGame* SaveData = Cast<ULCOAudioSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(ULCOAudioSettingsSaveGame::StaticClass()));
	if (!SaveData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Failed to create audio settings save object"));
		return;
	}

	SaveData->MasterVolume = MasterVolume;
	SaveData->BGMVolume = BGMVolume;
	SaveData->SFXVolume = SFXVolume;
	SaveData->VoiceVolume = VoiceVolume;

	if (!UGameplayStatics::SaveGameToSlot(SaveData, SaveSlotName, SaveUserIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Failed to save audio settings"));
	}
}

void ULCOAudioSettingsSubsystem::LoadAudioSettings()
{
	ULCOAudioSettingsSaveGame* SaveData = Cast<ULCOAudioSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!SaveData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] No saved audio settings; using defaults"));
		return;
	}

	MasterVolume = FMath::Clamp(SaveData->MasterVolume, 0.0f, 1.0f);
	BGMVolume = FMath::Clamp(SaveData->BGMVolume, 0.0f, 1.0f);
	SFXVolume = FMath::Clamp(SaveData->SFXVolume, 0.0f, 1.0f);
	VoiceVolume = FMath::Clamp(SaveData->VoiceVolume, 0.0f, 1.0f);
	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Loaded saved audio settings: Master=%.3f BGM=%.3f SFX=%.3f Voice=%.3f"),
		MasterVolume,
		BGMVolume,
		SFXVolume,
	VoiceVolume);
}

void ULCOAudioSettingsSubsystem::HandleWorldRegisteredToAudioDevice(const UWorld* World, Audio::FDeviceId AudioDeviceId)
{
	if (!ShouldHandleWorld(World))
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] WorldRegisteredToAudioDevice: World=%s AudioDeviceId=%u"),
		*GetNameSafe(World),
		AudioDeviceId);
	ApplyAudioSettingsForWorld(World);
}

bool ULCOAudioSettingsSubsystem::ShouldHandleWorld(const UWorld* World) const
{
	return World
		&& World->IsGameWorld()
		&& World->GetGameInstance() == GetGameInstance();
}

bool ULCOAudioSettingsSubsystem::EnsureSoundMixActive(const UWorld* World, USoundMix* SoundMix)
{
	if (!ShouldHandleWorld(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Sound mix not pushed: invalid game world World=%s"), *GetNameSafe(World));
		return false;
	}

	const FAudioDeviceHandle AudioDeviceHandle = World->GetAudioDevice();
	const Audio::FDeviceId AudioDeviceId = AudioDeviceHandle.GetDeviceID();
	FAudioDevice* AudioDevice = AudioDeviceHandle.GetAudioDevice();
	if (!AudioDevice)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Sound mix not pushed: no audio device World=%s Mix=%s"),
			*GetNameSafe(World),
			*GetNameSafe(SoundMix));
		return false;
	}

	const uint32 WorldId = World->GetUniqueID();
	if (SoundMixPushedWorldId != WorldId || SoundMixPushedAudioDeviceId != AudioDeviceId)
	{
		UGameplayStatics::PushSoundMixModifier(World, SoundMix);
		SoundMixPushedWorldId = WorldId;
		SoundMixPushedAudioDeviceId = AudioDeviceId;
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Pushed sound mix: %s World=%s AudioDeviceId=%u"),
			*GetNameSafe(SoundMix),
			*GetNameSafe(World),
			AudioDeviceId);
	}

	return true;
}

void ULCOAudioSettingsSubsystem::ApplyClassVolume(const UWorld* World, USoundMix* SoundMix, USoundClass* SoundClass, float Volume, const TCHAR* DebugName, bool bApplyToChildren)
{
	if (!World || !SoundMix || !SoundClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Skip override: %s World=%s Mix=%s Class=%s"),
			DebugName,
			*GetNameSafe(World),
			*GetNameSafe(SoundMix),
			*GetNameSafe(SoundClass));
		return;
	}

	const float ClampedVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Override: Mix=%s Class=%s Volume=%.3f Children=%s"),
		*GetNameSafe(SoundMix),
		*GetNameSafe(SoundClass),
		ClampedVolume,
		bApplyToChildren ? TEXT("true") : TEXT("false"));
	UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, SoundClass, ClampedVolume, 1.0f, 0.0f, bApplyToChildren);
}

USoundMix* ULCOAudioSettingsSubsystem::LoadSoundMix()
{
	if (!LoadedGameAudioSoundMix)
	{
		LoadedGameAudioSoundMix = GameAudioSoundMix.LoadSynchronous();
		if (!LoadedGameAudioSoundMix && !bMissingSoundMixWarningLogged)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Missing sound mix asset: %s"), *GameAudioSoundMix.ToString());
			bMissingSoundMixWarningLogged = true;
		}
	}

	return LoadedGameAudioSoundMix;
}

USoundClass* ULCOAudioSettingsSubsystem::LoadSoundClass(TObjectPtr<USoundClass>& LoadedSoundClass, const TSoftObjectPtr<USoundClass>& SoundClass, const TCHAR* DebugName, bool& bMissingAssetWarningLogged)
{
	if (SoundClass.IsNull())
	{
		if (!bMissingAssetWarningLogged)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Sound class is not configured: %s"), DebugName);
			bMissingAssetWarningLogged = true;
		}
		return nullptr;
	}

	if (!LoadedSoundClass)
	{
		LoadedSoundClass = SoundClass.LoadSynchronous();
		if (!LoadedSoundClass && !bMissingAssetWarningLogged)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOAudioSettings] Missing sound class asset for %s: %s"), DebugName, *SoundClass.ToString());
			bMissingAssetWarningLogged = true;
		}
	}

	return LoadedSoundClass;
}
