#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/LCOTimeOfDayTypes.h"
#include "LCOTimeOfDayManager.generated.h"

class ADirectionalLight;
class ASkyLight;
class APostProcessVolume;
class ULCOTimeOfDayPresetAsset;

// 맵의 시간대 조명 담당 (맵마다 1개 배치)
// - 시간대는 챕터가 끝날 때만 진행 (1막: 정오 이후 / 2막: 해질녘 / 3막: 저녁). 맵 이동으로는 바뀌지 않음
// - 맵이 시작되면 LCOGameMode가 현재 시간대로 ApplyTimeOfDay 호출 (어느 맵이든 같은 시간대)
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

	// 모든 맵 공용 조명 값 지정 (LCOGameMode가 호출). 비어 있으면 이 맵의 값 사용
	void SetSharedPresets(ULCOTimeOfDayPresetAsset* InSharedPresets) { SharedPresets = InSharedPresets; }

protected:
	// 시간대가 적용된 뒤 호출 (BP에서 가로등 켜기, 안개 변경 등 추가 연출용)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|TimeOfDay")
	void OnTimeOfDayApplied(ELCOTimeOfDay TimeOfDay);

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

	// 체크하면 공용 값(DA_TimeOfDayPresets) 대신 아래 이 맵의 값 사용
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "LCO|TimeOfDay|Preset")
	bool bUseMapPreset = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay|Preset")
	FLCOTimeOfDayPreset NoonPreset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay|Preset")
	FLCOTimeOfDayPreset SunsetPreset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|TimeOfDay|Preset")
	FLCOTimeOfDayPreset EveningPreset;

private:
	const FLCOTimeOfDayPreset& GetPreset(ELCOTimeOfDay TimeOfDay) const;

	UPROPERTY(Transient)
	TObjectPtr<ULCOTimeOfDayPresetAsset> SharedPresets;
};
