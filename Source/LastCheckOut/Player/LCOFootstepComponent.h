#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "LCOFootstepComponent.generated.h"

class USoundBase;
class ALCOPlayerCharacter;
class ALCOFootstepSurfaceVolume;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOFootstep, FVector, Location, float, Loudness);

// 플레이어 발소리
// 이동한 거리를 누적해서 일정 거리마다 발소리 재생 + AI 청각 소음 발생 (걷기 / 달리기 / 웅크리기별 간격과 크기 다름)
// 애니메이션과 무관하게 동작 (나중에 애님 노티파이로 바꿔도 PlayFootstep()만 호출하면 됨)
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 발소리 1회 재생 + AI 소음 발생 (애님 노티파이 등에서 직접 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "LCO|Footstep")
	void PlayFootstep();

	// 발소리가 날 때마다 (발자국 이펙트, 사운드 추가 연출 등)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Footstep")
	FOnLCOFootstep OnFootstep;

protected:
	virtual void BeginPlay() override;

	// ---------- 사운드 ----------

	// 재생할 발소리 (비워두면 소리 없이 AI 소음만 발생)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound")
	TObjectPtr<USoundBase> FootstepSound;
	
	// 바닥 종류(Physical Surface)별 발소리. 등록 안 된 바닥은 FootstepSound 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound")
	TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<USoundBase>> SurfaceSounds;

	// 발밑 바닥을 찾는 트레이스 길이 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound", meta = (ClampMin = "1"))
	float SurfaceTraceDistance = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound", meta = (ClampMin = "0"))
	float WalkVolume = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound", meta = (ClampMin = "0"))
	float SprintVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound", meta = (ClampMin = "0"))
	float CrouchVolume = 0.25f;

	// ---------- 발걸음 간격 (cm) ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Step", meta = (ClampMin = "1"))
	float WalkStepDistance = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Step", meta = (ClampMin = "1"))
	float SprintStepDistance = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Step", meta = (ClampMin = "1"))
	float CrouchStepDistance = 90.f;

	// 이 속도 이상일 때만 발걸음 누적 (미세한 미끄러짐 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Step", meta = (ClampMin = "0"))
	float MinStepSpeed = 10.f;

	// ---------- AI 소음 ----------

	// 소음 크기 0 ~ 1
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float WalkLoudness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float SprintLoudness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchLoudness = 0.1f;

	// 소음이 들리는 최대 거리 (cm, 0이면 AI 청각 설정 거리 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (ClampMin = "0"))
	float WalkNoiseRange = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (ClampMin = "0"))
	float SprintNoiseRange = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (ClampMin = "0"))
	float CrouchNoiseRange = 200.f;

	// AI가 소음 종류를 구분할 때 쓰는 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise")
	FName NoiseTag = TEXT("Footstep");

	// 소음 범위를 화면에 표시 (테스트용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Debug")
	bool bDrawDebugNoise = false;

private:
	// 현재 상태(걷기 / 달리기 / 웅크리기)에 맞는 설정값
	void GetCurrentSettings(float& OutStepDistance, float& OutVolume, float& OutLoudness, float& OutRange) const;
	// 발밑 바닥 종류에 맞는 발소리 (없으면 FootstepSound)
	USoundBase* FindSurfaceSound(const FVector& FootLocation) const;
	// 캐릭터가 들어가 있는 발소리 볼륨 중 우선순위가 가장 높은 것 (없으면 nullptr)
	const ALCOFootstepSurfaceVolume* FindSurfaceVolume() const;
	UPROPERTY(Transient)
	TObjectPtr<ALCOPlayerCharacter> Character;

	float DistanceSinceLastStep = 0.f;
};