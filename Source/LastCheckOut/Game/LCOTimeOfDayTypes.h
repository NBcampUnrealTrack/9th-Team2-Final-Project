#pragma once

#include "CoreMinimal.h"
#include "LCOTimeOfDayTypes.generated.h"

// 시간대. 앞으로만 진행 (정오 이후 → 해질녘 → 저녁)
// 값 순서 = 진행 순서이므로 순서를 바꾸지 말 것 (더 늦은 시간대 비교에 사용)
UENUM(BlueprintType)
enum class ELCOTimeOfDay : uint8
{
	Noon		UMETA(DisplayName = "정오 이후"),	// 1막
	Sunset		UMETA(DisplayName = "해질녘"),		// 2막 (1막 클리어부터)
	Evening		UMETA(DisplayName = "저녁")			// 3막 (2막 클리어부터)
};

// 시간대 1개의 조명 값
USTRUCT(BlueprintType)
struct FLCOTimeOfDayPreset
{
	GENERATED_BODY()

	// 태양 높이 (지평선 기준 각도, 위 = +, 아래 = -). 하늘색 / 노을은 SkyAtmosphere가 태양 각도에 맞춰 자동 변경
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|TimeOfDay", meta = (ClampMin = "-30.0", ClampMax = "90.0", Units = "deg"))
	float SunElevation = 45.f;

	// 태양 밝기 (lux)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|TimeOfDay", meta = (ClampMin = "0.0"))
	float SunIntensity = 10.f;

	// 태양 색
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|TimeOfDay")
	FLinearColor SunColor = FLinearColor::White;

	// 하늘 빛(SkyLight) 밝기 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|TimeOfDay", meta = (ClampMin = "0.0"))
	float SkyLightIntensity = 1.f;

	// 노출 보정 (PostProcessVolume 지정 시). 자동 노출 때문에 저녁이 너무 밝아 보이면 낮춤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|TimeOfDay", meta = (ClampMin = "-5.0", ClampMax = "5.0"))
	float ExposureBias = 0.f;
};
