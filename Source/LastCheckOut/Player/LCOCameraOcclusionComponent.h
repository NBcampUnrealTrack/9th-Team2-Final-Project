#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOCameraOcclusionComponent.generated.h"

class UPrimitiveComponent;

// 3인칭 카메라 가림 처리
// - 카메라가 벽에 밀려 캐릭터에 너무 가까워지면 내 몸을 숨김
// - 카메라와 캐릭터 사이(또는 카메라 위치)에 NPC 등 다른 Pawn이 있으면 그 Pawn을 숨김
// 숨김은 SetRenderInMainPass(false) 방식이라 그림자는 그대로 남는다
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOCameraOcclusionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOCameraOcclusionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 카메라와 캐릭터 눈 위치 사이 거리가 이 값보다 가까우면 내 몸 숨김
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion", meta = (ClampMin = "0"))
	float SelfHideDistance = 70.f;

	// 카메라 ~ 캐릭터 사이 가림 검사용 구체 반경
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion", meta = (ClampMin = "0"))
	float ProbeRadius = 20.f;

	// false면 NPC 숨김 기능 끔
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion")
	bool bHideOccludingPawns = true;

private:
	void UpdateSelf(const FVector& CameraLocation, const FVector& PawnEyeLocation);
	void UpdateOccluders(const FVector& CameraLocation, const FVector& PawnEyeLocation);

	void SetSelfHidden(bool bHide);
	void RestoreAllOccluders();

	bool bSelfHidden = false;

	// 지금 숨겨 둔 다른 액터의 메시들
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenOccluders;
};