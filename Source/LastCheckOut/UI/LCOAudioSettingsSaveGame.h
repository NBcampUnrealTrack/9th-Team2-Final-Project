#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LCOAudioSettingsSaveGame.generated.h"

UCLASS()
class LASTCHECKOUT_API ULCOAudioSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float MasterVolume = 1.0f;

	UPROPERTY()
	float BGMVolume = 1.0f;

	UPROPERTY()
	float SFXVolume = 1.0f;

	UPROPERTY()
	float VoiceVolume = 1.0f;
};
