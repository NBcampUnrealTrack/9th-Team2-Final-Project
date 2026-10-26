#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/ChaosEngineInterface.h"
#include "LCOFootstepSurfaceVolume.generated.h"

class UBoxComponent;

// 발소리 바닥 종류 지정 볼륨
// 이 박스 안에서는 바닥 재질(Physical Material)과 관계없이 지정한 바닥 종류의 발소리가 남
// 이미 만들어진 맵의 바닥 머티리얼을 건드리지 않고 구역별로 발소리를 바꿀 때 사용
UCLASS()
class LASTCHECKOUT_API ALCOFootstepSurfaceVolume : public AActor
{
	GENERATED_BODY()

public:
	ALCOFootstepSurfaceVolume();

	EPhysicalSurface GetSurface() const { return Surface; }
	int32 GetPriority() const { return Priority; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Footstep")
	TObjectPtr<UBoxComponent> Box;

	// 이 구역의 바닥 종류 (Project Settings → Physics → Physical Surface에 등록한 이름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType_Default;

	// 볼륨이 겹칠 때 높은 쪽이 우선 (예: 나무 바닥 방 0, 그 안의 카펫 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	int32 Priority = 0;
};