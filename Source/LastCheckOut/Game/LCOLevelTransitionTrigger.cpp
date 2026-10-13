#include "LCOLevelTransitionTrigger.h"
#include "LastCheckOut.h"
#include "Game/LCOGameMode.h"
#include "Player/LCOPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

ALCOLevelTransitionTrigger::ALCOLevelTransitionTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));   // 막지 않고 겹침만 감지
	TriggerBox->ShapeColor = FColor::Cyan;                   // 에디터에서 보이는 색
	TriggerBox->SetHiddenInGame(true);                       // 게임 중에는 안 보임
}

void ALCOLevelTransitionTrigger::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);

	// 목적지 미설정 경고 (배치 실수 확인용)
	if (TargetLevel.IsNull())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOLevelTransitionTrigger] %s: TargetLevel is not set"), *GetName());
	}
}

void ALCOLevelTransitionTrigger::SetTriggerEnabled(bool bEnabled)
{
	bTriggerEnabled = bEnabled;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOLevelTransitionTrigger] %s: Enabled = %s"),
		*GetName(), bTriggerEnabled ? TEXT("true") : TEXT("false"));
}

void ALCOLevelTransitionTrigger::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 플레이어만 반응 (NPC, 물체 무시)
	if (!Cast<ALCOPlayerCharacter>(OtherActor))
	{
		return;
	}

	if (!bTriggerEnabled || bHasTriggered || TargetLevel.IsNull())
	{
		return;
	}

	ALCOGameMode* LCOGameMode = GetWorld()->GetAuthGameMode<ALCOGameMode>();
	if (!LCOGameMode)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOLevelTransitionTrigger] %s: LCOGameMode not found"), *GetName());
		return;
	}

	bHasTriggered = true;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOLevelTransitionTrigger] %s: Travel to %s (Entry = %s)"),
		*GetName(), *TargetLevel.ToString(), *TargetPlayerStartTag.ToString());

	// 인벤토리, 퀵슬롯, 시점 보관 + 맵 이동은 GameMode가 처리
	LCOGameMode->TravelToLevel(TargetLevel, TargetPlayerStartTag);
}
