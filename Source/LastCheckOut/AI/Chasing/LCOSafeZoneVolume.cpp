#include "AI/Chasing/LCOSafeZoneVolume.h"

#include "AI/Chasing/LCOChasingManager.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ALCOSafeZoneVolume::ALCOSafeZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("Zone"));
	SetRootComponent(Zone);
	Zone->SetBoxExtent(FVector(500.f, 500.f, 200.f));
	Zone->SetCollisionProfileName(TEXT("OverlapOnlyPawn"));
}

void ALCOSafeZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	Zone->OnComponentBeginOverlap.AddDynamic(this, &ALCOSafeZoneVolume::OnZoneBeginOverlap);
	Zone->OnComponentEndOverlap.AddDynamic(this, &ALCOSafeZoneVolume::OnZoneEndOverlap);
}

void ALCOSafeZoneVolume::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어만 대상으로 한다.
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	if (ALCOChasingManager* Manager = Cast<ALCOChasingManager>(UGameplayStatics::GetActorOfClass(this, ALCOChasingManager::StaticClass())))
	{
		Manager->NotifyPlayerEnteredSafeZone();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSafeZoneVolume] No ChasingManager in the level."));
	}
}

void ALCOSafeZoneVolume::OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	if (ALCOChasingManager* Manager = Cast<ALCOChasingManager>(UGameplayStatics::GetActorOfClass(this, ALCOChasingManager::StaticClass())))
	{
		Manager->NotifyPlayerExitedSafeZone();
	}
}
