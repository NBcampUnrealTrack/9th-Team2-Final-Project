#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOCameraOcclusionComponent.generated.h"

class UPrimitiveComponent;

// 3인칭 카메라 가림 처리
// - 카메라가 벽에 밀려 캐릭터에 가까워지면 내 몸을 디더 반투명 처리 (머티리얼의 CameraFade 파라미터)
// - 카메라가 몸 안까지 들어오면 내 몸 숨김
// - 카메라와 캐릭터 사이(또는 카메라 위치)에 NPC 등 다른 Pawn이 있으면 그 Pawn을 숨김
// 숨김은 메인 / 깊이 패스만 끄는 방식이라 그림자는 그대로 남는다
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOCameraOcclusionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOCameraOcclusionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// 내 몸 가림 처리 사용 여부 (1인칭에서는 끔: 몸은 이미 숨겨져 있고, 그림자가 디더로 깨지지 않게)
	UFUNCTION(BlueprintCallable, Category = "Occlusion")
	void SetSelfOcclusionEnabled(bool bEnabled);

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

	// 카메라가 이 거리보다 가까워지면 내 몸이 반투명해지기 시작 (SelfHideDistance까지 점점 투명해짐)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion|Fade", meta = (ClampMin = "0"))
	float SelfFadeStartDistance = 150.f;

	// SelfHideDistance 직전, 가장 투명할 때의 불투명도 (0 = 완전 투명, 1 = 불투명)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion|Fade", meta = (ClampMin = "0", ClampMax = "1"))
	float SelfMinOpacity = 0.3f;

	// 불투명도가 바뀌는 속도 (벽에 부딪혀 카메라가 확 당겨져도 부드럽게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion|Fade", meta = (ClampMin = "0"))
	float SelfFadeSpeed = 10.f;

	// 캐릭터 머티리얼의 디더 불투명도 스칼라 파라미터 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Occlusion|Fade")
	FName FadeParamName = TEXT("CameraFade");

private:
	void UpdateSelf(const FVector& CameraLocation, const FVector& PawnEyeLocation, float DeltaTime);
	void UpdateOccluders(const FVector& CameraLocation, const FVector& PawnEyeLocation);

	void SetSelfHidden(bool bHide);
	void SetSelfOpacity(float Opacity);
	void RestoreAllOccluders();

	bool bSelfHidden = false;
	bool bSelfOcclusionEnabled = true;

	// 현재 내 몸 불투명도 (1 = 불투명)
	float SelfOpacity = 1.f;

	// 지금 숨겨 둔 다른 액터의 메시들
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenOccluders;
};