#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/LCOTimeOfDayTypes.h"
#include "LCOTimeOfDayPresetAsset.generated.h"

// 모든 맵이 함께 쓰는 시간대 조명 값 (DA_TimeOfDayPresets)
// - LCOGameMode의 TimeOfDayPresets에 지정하면 모든 맵의 LCOTimeOfDayManager가 이 값을 사용
// - 특정 맵만 다르게 하려면 그 맵 매니저의 bUseMapPreset 체크
UCLASS(BlueprintType)
class LASTCHECKOUT_API ULCOTimeOfDayPresetAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay")
	FLCOTimeOfDayPreset NoonPreset = LCOTimeOfDayDefaults::Noon();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay")
	FLCOTimeOfDayPreset SunsetPreset = LCOTimeOfDayDefaults::Sunset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay")
	FLCOTimeOfDayPreset EveningPreset = LCOTimeOfDayDefaults::Evening();

	const FLCOTimeOfDayPreset& GetPreset(ELCOTimeOfDay TimeOfDay) const
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
};
