#include "Game/LCOTimeOfDayManager.h"
#include "LastCheckOut.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"

ALCOTimeOfDayManager::ALCOTimeOfDayManager()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 기본값 (맵에서 보면서 조절)
	// 정오 이후: 높이 뜬 밝은 태양
	NoonPreset.SunElevation = 50.f;
	NoonPreset.SunIntensity = 10.f;
	NoonPreset.SunColor = FLinearColor(1.f, 0.97f, 0.92f);
	NoonPreset.SkyLightIntensity = 1.f;
	NoonPreset.ExposureBias = 0.f;

	// 해질녘: 지평선 근처 주황빛
	SunsetPreset.SunElevation = 4.f;
	SunsetPreset.SunIntensity = 6.f;
	SunsetPreset.SunColor = FLinearColor(1.f, 0.55f, 0.3f);
	SunsetPreset.SkyLightIntensity = 0.6f;
	SunsetPreset.ExposureBias = -0.5f;

	// 저녁: 태양이 지평선 아래, 어둡고 푸른빛
	EveningPreset.SunElevation = -6.f;
	EveningPreset.SunIntensity = 0.5f;
	EveningPreset.SunColor = FLinearColor(0.45f, 0.55f, 0.85f);
	EveningPreset.SkyLightIntensity = 0.3f;
	EveningPreset.ExposureBias = -1.5f;
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
	UE_LOG(LogLCOCore, Log, TEXT("[LCOTimeOfDay] %s: Apply %s (Elevation = %.1f, Sun = %.1f, Sky = %.2f)"),
		*GetName(), *UEnum::GetValueAsString(TimeOfDay),
		Preset.SunElevation, Preset.SunIntensity, Preset.SkyLightIntensity);

	OnTimeOfDayApplied(TimeOfDay);
}

const FLCOTimeOfDayPreset& ALCOTimeOfDayManager::GetPreset(ELCOTimeOfDay TimeOfDay) const
{
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
