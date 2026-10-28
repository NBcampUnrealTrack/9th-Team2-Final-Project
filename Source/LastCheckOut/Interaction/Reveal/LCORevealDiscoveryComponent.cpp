#include "Interaction/Reveal/LCORevealDiscoveryComponent.h"

#include "Interaction/Reveal/LCORevealLightSubsystem.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

namespace LCORevealDiscoveryPrivate
{
    // M_RevealDecal의 빛 종류 파라미터 이름
    const FName RequiredLightTypeParam(TEXT("RequiredLightType"));

    // ★ FLCORevealLightState의 광원 위치 변수 이름이 다르면 이 한 줄만 고치면 됨
    FVector GetLightPosition(const FLCORevealLightState& State)
    {
        return State.Position;
    }
}

ULCORevealDiscoveryComponent::ULCORevealDiscoveryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f;
}

void ULCORevealDiscoveryComponent::BeginPlay()
{
    Super::BeginPlay();

    if (bReadLightTypeFromMaterial)
    {
        ReadLightTypeFromMaterial();
    }
}

void ULCORevealDiscoveryComponent::ReadLightTypeFromMaterial()
{
    const AActor* Owner = GetOwner();
    const UDecalComponent* Decal = Owner ? Owner->FindComponentByClass<UDecalComponent>() : nullptr;
    const UMaterialInterface* Material = Decal ? Decal->GetDecalMaterial() : nullptr;

    float Value = 0.f;
    if (Material && Material->GetScalarParameterValue(FHashedMaterialParameterInfo(LCORevealDiscoveryPrivate::RequiredLightTypeParam), Value))
    {
        RequiredLightType = static_cast<ELCORevealLightType>(FMath::RoundToInt(Value));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[LCORevealDiscovery] %s: 데칼 머티리얼에서 RequiredLightType을 읽지 못해 설정값 사용"),
            Owner ? *Owner->GetName() : TEXT("?"));
    }
}

FVector ULCORevealDiscoveryComponent::GetSamplePoint() const
{
    // 데칼은 +X 방향으로 투사되므로 표면 앞쪽(-X)을 판정 지점으로 사용
    const AActor* Owner = GetOwner();
    return Owner ? Owner->GetActorLocation() - Owner->GetActorForwardVector() * SurfaceOffset : FVector::ZeroVector;
}

void ULCORevealDiscoveryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bDiscovered)
    {
        return;
    }

    const UWorld* World = GetWorld();
    const ULCORevealLightSubsystem* RevealLights = World ? World->GetSubsystem<ULCORevealLightSubsystem>() : nullptr;
    if (!RevealLights)
    {
        return;
    }

    const FVector SamplePoint = GetSamplePoint();

    // 머티리얼과 같은 계산 (빛 종류가 다르면 0)
    CurrentStrength = RevealLights->GetRevealStrengthAt(SamplePoint, RequiredLightType);

    if (CurrentStrength > 0.f && bCheckOcclusion && IsBlockedFromLight(SamplePoint))
    {
        CurrentStrength = 0.f;
    }

    if (bDrawDebug)
    {
        DrawDebugSphere(World, SamplePoint, 6.f, 8, CurrentStrength >= DiscoverThreshold ? FColor::Green : FColor::Red, false, 0.1f);
    }

    if (CurrentStrength >= DiscoverThreshold)
    {
        LitTime += DeltaTime;
        if (LitTime >= DiscoverTime)
        {
            Discover();
        }
    }
    else
    {
        LitTime = 0.f;
    }
}

bool ULCORevealDiscoveryComponent::IsBlockedFromLight(const FVector& SamplePoint) const
{
    const UWorld* World = GetWorld();
    const ULCORevealLightSubsystem* RevealLights = World ? World->GetSubsystem<ULCORevealLightSubsystem>() : nullptr;
    if (!RevealLights)
    {
        return false;
    }

    const FVector LightPosition = LCORevealDiscoveryPrivate::GetLightPosition(RevealLights->GetActiveRevealLight());

    FCollisionQueryParams Params(SCENE_QUERY_STAT(LCORevealDiscovery), false);
    Params.AddIgnoredActor(GetOwner());

    // 플레이어 몸과 손에 든 물건(손전등, UV 라이트)은 무시
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        Params.AddIgnoredActor(Player);

        TArray<AActor*> Attached;
        Player->GetAttachedActors(Attached, true, true);
        Params.AddIgnoredActors(Attached);
    }

    FHitResult Hit;
    return World->LineTraceSingleByChannel(Hit, LightPosition, SamplePoint, ECC_Visibility, Params);
}

void ULCORevealDiscoveryComponent::Discover()
{
    bDiscovered = true;
    SetComponentTickEnabled(false);

    UE_LOG(LogTemp, Log, TEXT("[LCORevealDiscovery] 흔적 발견: %s"), GetOwner() ? *GetOwner()->GetName() : TEXT("?"));

    if (NotebookTable && !NotebookRowName.IsNone())
    {
        if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (ULCONotebookSubsystem* Notebook = GameInstance->GetSubsystem<ULCONotebookSubsystem>())
            {
                Notebook->AddEntryFromTable(NotebookTable, NotebookRowName);
            }
        }
    }

    OnDiscovered.Broadcast(this);
}
