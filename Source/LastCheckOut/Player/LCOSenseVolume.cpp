#include "Player/LCOSenseVolume.h"
#include "Components/BoxComponent.h"

ALCOSenseVolume::ALCOSenseVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	SetRootComponent(Zone);
	Zone->SetBoxExtent(FVector(300.f, 300.f, 200.f));

	// 판정은 위치 계산으로 하므로 충돌 불필요 (숨기 중처럼 캐릭터 충돌이 꺼져 있어도 판정 가능)
	Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Zone->SetGenerateOverlapEvents(false);
}

bool ALCOSenseVolume::IsLocationInside(const FVector& Location) const
{
	// 박스 로컬 공간으로 옮기면 회전 / 스케일이 풀리므로 스케일 전 Extent와 비교
	const FVector Local = Zone->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = Zone->GetUnscaledBoxExtent();
	return FBox(-Extent, Extent).IsInsideOrOn(Local);
}

void ALCOSenseVolume::NotifySensed(AActor* Sensor)
{
	UE_LOG(LogTemp, Log, TEXT("[LCOSenseVolume] %s Sensed by %s"), *GetName(), *GetNameSafe(Sensor));

	OnSensed(Sensor);
}