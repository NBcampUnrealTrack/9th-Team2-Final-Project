#include "Player/LCOFootstepSurfaceVolume.h"
#include "Components/BoxComponent.h"

ALCOFootstepSurfaceVolume::ALCOFootstepSurfaceVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
	Box->ShapeColor = FColor::Cyan;

	// 게임 화면에는 안 보이게 (에디터에서만 박스 선 표시)
	SetActorHiddenInGame(true);
}