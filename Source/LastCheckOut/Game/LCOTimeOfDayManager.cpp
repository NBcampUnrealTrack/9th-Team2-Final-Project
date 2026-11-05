#include "Game/LCOTimeOfDayManager.h"
#include "LastCheckOut.h"
#include "Game/LCOTimeOfDayPresetAsset.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"

ALCOTimeOfDayManager::ALCOTimeOfDayManager()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 기본값 (LCOTimeOfDayTypes.h). 공용 값이 없을 때 / bUseMapPreset일 때 사용
	NoonPreset = LCOTimeOfDayDefaults::Noon();
	SunsetPreset = LCOTimeOfDayDefaults::Sunset();
	EveningPreset = LCOTimeOfDayDefaults::Evening();
}

void ALCOTimeOfDayManager::ApplyTimeOfDay(ELCOTimeOfDay TimeOfDay)
{
	const FLCOTimeOfDayPreset& Preset = GetPreset(TimeOfDay);

	// 태양: 높이(Pitch)만 바꾸고 방향(Yaw)은 맵에 배치한 그대로
	if (IsValid(Sun))
	{
		if (ULightComponent* SunComponent = Sun->GetLightComponent())
		{
			if (SunComponent->Mobility != EComponentMobility::Movable)
			{
				UE_LOG(LogLCOCore, Warning, TEXT("[LCOTimeOfDay] %s: Sun is not Movable -> set Mobility to Movable"), *GetName());
			}

			FRotator SunRotation = Sun->GetActorRotation();
			SunRotation.Pitch = -Preset.SunElevation;
			SunRotation.Roll = 0.f;
			Sun->SetActorRotation(SunRotation);

			SunComponent->SetIntensity(Preset.SunIntensity);
			SunComponent->SetLightColor(Preset.SunColor);
		}
	}
	else
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOTimeOfDay] %s: Sun is not set"), *GetName());
	}

	// 하늘 빛: 밝기 변경 후 하늘 다시 캡처 (Real Time Capture면 자동 갱신)
	if (IsValid(SkyLight))
	{
		if (USkyLightComponent* SkyComponent = SkyLight->GetLightComponent())
		{
			SkyComponent->SetIntensity(Preset.SkyLightIntensity);
			SkyComponent->RecaptureSky();
		}
	}

	// 노출 보정 (선택)
	if (IsValid(PostProcessVolume))
	{
		PostProcessVolume->Settings.bOverride_AutoExposureBias = true;
		PostProcessVolume->Settings.AutoExposureBias = Preset.ExposureBias;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOTimeOfDay] %s: Apply %s (Preset = %s, Elevation = %.1f, Sun = %.1f, Sky = %.2f)"),
		*GetName(), *UEnum::GetValueAsString(TimeOfDay),
		(SharedPresets && !bUseMapPreset) ? *SharedPresets->GetName() : TEXT("Map"),
		Preset.SunElevation, Preset.SunIntensity, Preset.SkyLightIntensity);

	OnTimeOfDayApplied(TimeOfDay);
}

const FLCOTimeOfDayPreset& ALCOTimeOfDayManager::GetPreset(ELCOTimeOfDay TimeOfDay) const
{
	// 공용 값 우선 (이 맵만 다르게 쓰려면 bUseMapPreset)
	if (SharedPresets && !bUseMapPreset)
	{
		return SharedPresets->GetPreset(TimeOfDay);
	}

	switch (TimeOfDay)
	{
	case ELCOTimeOfDay::Sunset:
		return SunsetPreset;

	case ELCOTimeOfDay::Evening:
		return EveningPreset;

	case ELCOTimeOfDay::Noon:
	default:
		return NoonPreset;
	}
}
