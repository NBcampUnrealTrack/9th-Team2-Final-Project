#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/LCOTimeOfDayTypes.h"
#include "LCOTimeOfDayManager.generated.h"

class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;

// 맵의 시간대 조명 담당 (맵마다 1개 배치)
// - ArrivalTimeOfDay: 이 맵에 도착하면 되는 시간대 (Main: 정오 이후 / Outdoor: 해질녘 / Annex: 저녁)
// - 맵이 시작되면 LCOGameMode가 "지금까지의 시간대"와 ArrivalTimeOfDay 중 더 늦은 쪽으로 갱신 후 ApplyTimeOfDay 호출
//   → 시간대는 앞으로만 진행 (저녁이 된 뒤 Main / Outdoor에 가도 저녁 유지)
// - 디테일 패널에서 Sun(Directional Light) / SkyLight / (선택) PostProcessVolume 지정
// - 실행 중에 태양을 움직이므로 Directional Light / Sky Light의 Mobility는 Movable
UCLASS()
class LASTCHECKOUT_API ALCOTimeOfDayManager : public AActor
{
	GENERATED_BODY()

public:
	ALCOTimeOfDayManager();

	// 시간대 조명 적용 (연출 없이 즉시)
	UFUNCTION(BlueprintCallable, Category = "LCO|TimeOfDay")
	void ApplyTimeOfDay(ELCOTimeOfDay TimeOfDay);

	// 이 맵에 도착하면 되는 시간대
	UFUNCTION(BlueprintPure, Category = "LCO|TimeOfDay")
	ELCOTimeOfDay GetArrivalTimeOfDay() const { return ArrivalTimeOfDay; }

protected:
	// 시간대가 적용된 뒤 호출 (BP에서 가로등 켜기, 안개 변경 등 추가 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|TimeOfDay")
	void OnTimeOfDayApplied(ELCOTimeOfDay TimeOfDay);

	// 이 맵에 도착하면 되는 시간대 (Main: 정오 이후 / Outdoor: 해질녘 / Annex: 저녁)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|TimeOfDay")
	ELCOTimeOfDay ArrivalTimeOfDay = ELCOTimeOfDay::Noon;

	// 태양 (맵의 Directional Light)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|TimeOfDay")
	TObjectPtr<ADirectionalLight> Sun;

	// 하늘 빛 (맵의 Sky Light)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|TimeOfDay")
	TObjectPtr<ASkyLight> SkyLight;

	// (선택) 노출 보정용 PostProcessVolume. 비워두면 노출은 건드리지 않음
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|TimeOfDay")
	TObjectPtr<APostProcessVolume> PostProcessVolume;

	// ---------- 시간대별 조명 값 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay|Preset")
	FLCOTimeOfDayPreset NoonPreset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay|Preset")
	FLCOTimeOfDayPreset SunsetPreset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay|Preset")
	FLCOTimeOfDayPreset EveningPreset;

private:
	const FLCOTimeOfDayPreset& GetPreset(ELCOTimeOfDay TimeOfDay) const;
};
