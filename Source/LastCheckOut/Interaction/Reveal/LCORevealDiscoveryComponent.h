#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Reveal/LCORevealTypes.h"
#include "LCORevealDiscoveryComponent.generated.h"

class UDataTable;
class ULCORevealDiscoveryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCORevealDiscovered, ULCORevealDiscoveryComponent*, Discovery);

// 흔적 데칼(M_RevealDecal을 쓰는 액터)에 붙여서 "플레이어가 흔적을 발견했다"를 판정하는 컴포넌트
// - 화면에 보이는 건 머티리얼이 처리하고, 이 컴포넌트는 퍼즐 진행·수첩 기록만 담당
// - 판정은 ULCORevealLightSubsystem::GetRevealStrengthAt()을 그대로 사용 (머티리얼과 같은 계산)
// - 머티리얼은 벽 가림을 확인하지 않으므로, 발견 판정에서만 선택적으로 벽 가림 확인
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCORevealDiscoveryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULCORevealDiscoveryComponent();

    UFUNCTION(BlueprintPure, Category = "Reveal|Discovery")
    bool IsDiscovered() const { return bDiscovered; }

    // 지금 이 흔적이 빛을 받는 정도 (0 ~ 1, 디버그·연출용)
    UFUNCTION(BlueprintPure, Category = "Reveal|Discovery")
    float GetCurrentRevealStrength() const { return CurrentStrength; }

    // 처음 발견했을 때
    UPROPERTY(BlueprintAssignable, Category = "Reveal|Discovery")
    FOnLCORevealDiscovered OnDiscovered;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // 데칼 머티리얼의 RequiredLightType 값을 그대로 사용 (끄면 아래 RequiredLightType 사용)
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery")
    bool bReadLightTypeFromMaterial = true;

    // 이 흔적이 반응하는 빛 종류
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery", meta = (EditCondition = "!bReadLightTypeFromMaterial"))
    ELCORevealLightType RequiredLightType = ELCORevealLightType::UV;

    // 이 정도 이상 빛을 받아야 발견으로 침 (0 ~ 1)
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DiscoverThreshold = 0.35f;

    // 이 시간(초) 동안 계속 비춰야 발견
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery", meta = (ClampMin = "0.0"))
    float DiscoverTime = 0.8f;

    // 빛과 흔적 사이에 벽이 있으면 발견 안 됨
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery")
    bool bCheckOcclusion = true;

    // 판정 지점을 데칼 표면에서 앞으로 띄우는 거리 (벽 자체에 가려지지 않도록)
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery", meta = (ClampMin = "0.0"))
    float SurfaceOffset = 5.f;

    // 발견 시 수첩에 기록 (선택, 비우면 기록 안 함)
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery|Notebook")
    TObjectPtr<UDataTable> NotebookTable;

    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery|Notebook")
    FName NotebookRowName;

    // 디버그: 판정 지점과 현재 세기 표시
    UPROPERTY(EditAnywhere, Category = "Reveal|Discovery|Debug")
    bool bDrawDebug = false;

private:
    FVector GetSamplePoint() const;
    bool IsBlockedFromLight(const FVector& SamplePoint) const;
    void ReadLightTypeFromMaterial();
    void Discover();

    float CurrentStrength = 0.f;
    float LitTime = 0.f;
    bool bDiscovered = false;
};
