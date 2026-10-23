#include "Interaction/LCODoorBendZone.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "NPC/LCOCaretaker.h"

ALCODoorBendZone::ALCODoorBendZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	SetRootComponent(Zone);
	Zone->SetBoxExtent(FVector(150.f, 150.f, 100.f));
	Zone->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
}

void ALCODoorBendZone::BeginPlay()
{
	Super::BeginPlay();

	Zone->OnComponentBeginOverlap.AddDynamic(this, &ALCODoorBendZone::OnZoneBeginOverlap);
	Zone->OnComponentEndOverlap.AddDynamic(this, &ALCODoorBendZone::OnZoneEndOverlap);
}

void ALCODoorBendZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 관리인만 대상으로 한다. 캡슐과 메시가 각각 겹침을 보내므로 캡슐만 센다.
	ALCOCaretaker* Caretaker = Cast<ALCOCaretaker>(OtherActor);
	if (!Caretaker || OtherComp != Caretaker->GetCapsuleComponent())
	{
		return;
	}

	Caretaker->EnterDoorBendZone();
}

void ALCODoorBendZone::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ALCOCaretaker* Caretaker = Cast<ALCOCaretaker>(OtherActor);
	if (!Caretaker || OtherComp != Caretaker->GetCapsuleComponent())
	{
		return;
	}

	Caretaker->ExitDoorBendZone();
}
