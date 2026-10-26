#pragma once

#include "CoreMinimal.h"
#include "LCORevealTypes.generated.h"

// 숨겨진 흔적(글씨, 지문, 발자국 등)을 드러내는 빛의 종류
// MPC에는 숫자(0, 1, ...)로 기록되므로 값 순서를 바꾸지 말 것 (머티리얼이 이 숫자로 비교)
UENUM(BlueprintType)
enum class ELCORevealLightType : uint8
{
	Flashlight = 0 UMETA(DisplayName = "손전등"),
	UV         = 1 UMETA(DisplayName = "UV 라이트")
};

// 현재 흔적을 드러내는 빛 하나의 정보 (서브시스템이 매 프레임 갱신, MPC와 같은 값)
USTRUCT(BlueprintType)
struct FLCORevealLightState
{
	GENERATED_BODY()

	// 켜져 있는 광원이 있는지 (false면 아래 값은 의미 없음)
	UPROPERTY(BlueprintReadOnly, Category = "Reveal")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Reveal")
	ELCORevealLightType LightType = ELCORevealLightType::Flashlight;

	UPROPERTY(BlueprintReadOnly, Category = "Reveal")
	FVector Position = FVector::ZeroVector;

	// 빛이 향하는 방향 (단위 벡터)
	UPROPERTY(BlueprintReadOnly, Category = "Reveal")
	FVector Direction = FVector::ForwardVector;

	// 원뿔 바깥 / 안쪽 각도의 cos. 바깥 밖이면 0, 안쪽 안이면 1, 사이는 부드럽게
	UPROPERTY(BlueprintReadOnly, Category = "Reveal")
	float OuterConeCos = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Reveal")
	float InnerConeCos = 1.f;

	// 유효 거리 (cm). 멀수록 약해짐
	UPROPERTY(BlueprintReadOnly, Category = "Reveal")
	float Range = 0.f;
};
