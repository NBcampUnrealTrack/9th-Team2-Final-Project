#include "Game/LCOScareEventVolume.h"
#include "LastCheckOut.h"
#include "Game/LCOScareDoor.h"
#include "Player/LCOPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ALCOScareEventVolume::ALCOScareEventVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));   // 막지 않고 겹침만 감지
	TriggerBox->ShapeColor = FColor::Red;                    // 에디터에서 보이는 색 (맵 이동 트리거와 구분)
	TriggerBox->SetHiddenInGame(true);                       // 게임 중에는 안 보임
}

void ALCOScareEventVolume::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);

	// 대상 미설정 경고 (배치 실수 확인용)
	if (TargetDoors.IsEmpty())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOScareEventVolume] %s: TargetDoors is empty"), *GetName());
	}

	for (ALCOScareDoor* Door : TargetDoors)
	{
		if (!IsValid(Door))
		{
			continue;
		}

		// 문이 닫히면 알림 받기
		Door->OnDoorSlammed.AddUObject(this, &ThisClass::HandleDoorSlammed);

		// 열린 문 연출인데 문이 닫힌 채로 시작하면 경고
		if (ScareType == ELCOScareType::SlamFromAjar && !Door->IsStartAjar())
		{
			UE_LOG(LogLCOCore, Warning, TEXT("[LCOScareEventVolume] %s: %s is not StartAjar"), *GetName(), *Door->GetName());
		}
	}
}

void ALCOScareEventVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어만 반응 (NPC, 물체 무시)
	if (!Cast<ALCOPlayerCharacter>(OtherActor) || bHasTriggered)
	{
		return;
	}

	bHasTriggered = true;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOScareEventVolume] %s: Triggered (Type = %s, Doors = %d, Delay = %.2f)"),
		*GetName(), ScareType == ELCOScareType::SlamFromAjar ? TEXT("SlamFromAjar") : TEXT("PeekAndSlam"),
		TargetDoors.Num(), StartDelay);

	if (StartDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(StartDelayTimer, this, &ThisClass::PlayScareOnDoors, StartDelay, false);
	}
	else
	{
		PlayScareOnDoors();
	}
}

void ALCOScareEventVolume::PlayScareOnDoors()
{
	for (ALCOScareDoor* Door : TargetDoors)
	{
		if (!IsValid(Door))
		{
			continue;
		}

		if (ScareType == ELCOScareType::SlamFromAjar)
		{
			Door->PlaySlam();
		}
		else
		{
			Door->PlayScare();
		}
	}

	OnScareTriggered();
}

void ALCOScareEventVolume::HandleDoorSlammed(ALCOScareDoor* Door)
{
	HideActorsAfter();
}

void ALCOScareEventVolume::HideActorsAfter()
{
	for (AActor* Actor : ActorsToHideAfter)
	{
		if (IsValid(Actor) && !Actor->IsHidden())
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);

			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOScareEventVolume] %s: Hide %s"), *GetName(), *Actor->GetName());
		}
	}
}

FLCOActorSaveState ALCOScareEventVolume::SaveActorState_Implementation() const
{
	FLCOActorSaveState State;
	State.bIsEnabled = !bHasTriggered;
	return State;
}

void ALCOScareEventVolume::LoadActorState_Implementation(const FLCOActorSaveState& State)
{
	bHasTriggered = !State.bIsEnabled;

	// 이미 발생한 연출이면 연출 없이 끝난 상태로 (문 닫힘 + 숨길 액터 숨김)
	if (bHasTriggered)
	{
		for (ALCOScareDoor* Door : TargetDoors)
		{
			if (IsValid(Door))
			{
				Door->CloseImmediately();
			}
		}
		HideActorsAfter();
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOScareEventVolume] %s LoadActorState (Triggered = %s)"),
		*GetName(), bHasTriggered ? TEXT("true") : TEXT("false"));
}
