#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Reveal/LCORevealTypes.h"
#include "LCORevealLightComponent.generated.h"

class USpotLightComponent;

// 이 액터의 Spot Light를 "흔적을 드러내는 빛"으로 등록하는 컴포넌트 (UV 라이트 아이템, 바닥에 둔 UV 램프 등)
// - 플레이어 손전등은 이 컴포넌트 없이도 서브시스템이 자동으로 찾음 (기본 광원)
// - 여러 광원이 켜져 있으면 Priority가 높은 것 하나만 사용 (MPC가 광원 하나만 담을 수 있어서)
// - Spot Light가 켜져 있고(Visible, 밝기 > 0) bRevealEnabled일 때만 광원으로 인정
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCORevealLightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCORevealLightComponent();

	// 광원 사용 켜기 / 끄기 (예: UV 라이트 아이템을 들었을 때만 켜기)
	UFUNCTION(BlueprintCallable, Category = "Reveal")
	void SetRevealEnabled(bool bEnabled);

	// 지금 흔적을 드러내는 광원으로 쓸 수 있는지
	UFUNCTION(BlueprintPure, Category = "Reveal")
	bool IsRevealActive() const;

	// 대상 Spot Light (SpotLightTag가 있으면 그 태그, 없으면 이 액터의 첫 Spot Light)
	UFUNCTION(BlueprintPure, Category = "Reveal")
	USpotLightComponent* GetSpotLight() const;

	ELCORevealLightType GetLightType() const { return LightType; }
	int32 GetPriority() const { return Priority; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 이 빛의 종류. 흔적 머티리얼은 자기가 반응할 종류만 드러남 (예: 지문은 UV에만)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reveal")
	ELCORevealLightType LightType = ELCORevealLightType::UV;

	// 여러 광원이 동시에 켜져 있을 때 우선순위 (높을수록 우선). 플레이어 손전등은 서브시스템 설정값(기본 0)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reveal")
	int32 Priority = 10;

	// 시작 시 광원 사용 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reveal")
	bool bRevealEnabled = true;

	// Spot Light가 여러 개일 때 사용할 컴포넌트의 태그. 비우면 첫 Spot Light
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reveal")
	FName SpotLightTag;

private:
	UPROPERTY()
	mutable TObjectPtr<USpotLightComponent> CachedSpotLight;
};
