#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Interaction/Reveal/LCORevealTypes.h"
#include "LCORevealLightSubsystem.generated.h"

class ULCORevealLightComponent;
class USpotLightComponent;
class UMaterialParameterCollection;

// 숨겨진 흔적(글씨, 지문, 발자국 등)을 드러내는 빛 정보를 매 프레임 MPC(MPC_RevealLight)에 기록
// - 광원 후보: ULCORevealLightComponent로 등록된 광원 + 플레이어 손전등(플레이어 폰의 Spot Light, 자동 탐색)
// - 켜져 있는 광원 중 우선순위가 가장 높은 하나만 기록 (MPC는 광원 하나 분량)
// - 흔적 머티리얼(MF_RevealMask)은 MPC 값으로 픽셀마다 빛을 받는지 계산, C++ 판정은 IsPointRevealed()로 같은 계산
//
// MPC 파라미터 이름 (MPC_RevealLight에 같은 이름으로 만들 것)
//   Vector: RevealLightPosition, RevealLightDirection
//   Scalar: RevealOuterConeCos, RevealInnerConeCos, RevealRange, RevealIntensity(0 / 1), RevealLightType(0 손전등, 1 UV)
UCLASS(Config = Game)
class LASTCHECKOUT_API ULCORevealLightSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ULCORevealLightSubsystem();

	// USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// 광원 등록 / 해제 (ULCORevealLightComponent가 BeginPlay / EndPlay에서 호출)
	void RegisterRevealLight(ULCORevealLightComponent* Light);
	void UnregisterRevealLight(ULCORevealLightComponent* Light);

	// 현재 선택된 광원 정보 (MPC에 기록한 값과 같음)
	UFUNCTION(BlueprintPure, Category = "Reveal")
	const FLCORevealLightState& GetActiveRevealLight() const { return ActiveLight; }

	// 이 위치가 지금 빛을 받는 정도 (0 ~ 1). 머티리얼 MF_RevealMask와 같은 계산 (벽 가림은 확인하지 않음)
	// RequiredLightType과 현재 광원 종류가 다르면 0
	UFUNCTION(BlueprintPure, Category = "Reveal")
	float GetRevealStrengthAt(const FVector& WorldLocation, ELCORevealLightType RequiredLightType) const;

	// MPC 파라미터 이름 (머티리얼과 맞춰야 함)
	static const FName PN_Position;
	static const FName PN_Direction;
	static const FName PN_OuterConeCos;
	static const FName PN_InnerConeCos;
	static const FName PN_Range;
	static const FName PN_Intensity;
	static const FName PN_LightType;

protected:
	// MPC 에셋 경로 (Config/DefaultGame.ini [/Script/LastCheckOut.LCORevealLightSubsystem]에서 변경 가능)
	UPROPERTY(Config)
	FSoftObjectPath RevealMPCPath;

	// 플레이어 손전등을 기본 광원으로 쓸지
	UPROPERTY(Config)
	bool bUsePlayerFlashlight = true;

	// 플레이어 손전등의 우선순위 (등록된 광원의 Priority와 비교. 기본 0 → UV 램프 등이 켜지면 그쪽 우선)
	UPROPERTY(Config)
	int32 PlayerFlashlightPriority = 0;

	// Spot Light의 안쪽 각도가 바깥 각도의 이 비율보다 작으면 이 비율로 보정 (가장자리를 부드럽게 흐려지도록)
	UPROPERTY(Config)
	float MinInnerConeRatio = 0.7f;

private:
	// 켜져 있는 광원 중 하나 선택
	FLCORevealLightState SelectActiveLight();

	// 플레이어 폰의 Spot Light (폰이 바뀌면 다시 찾음)
	USpotLightComponent* FindPlayerFlashlight();

	FLCORevealLightState MakeState(const USpotLightComponent* SpotLight, ELCORevealLightType LightType) const;

	void WriteToMPC();

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> RevealMPC;

	UPROPERTY()
	TArray<TWeakObjectPtr<ULCORevealLightComponent>> RegisteredLights;

	TWeakObjectPtr<APawn> CachedPlayerPawn;
	TWeakObjectPtr<USpotLightComponent> CachedPlayerFlashlight;

	FLCORevealLightState ActiveLight;
	bool bWarnedMissingMPC = false;
};
