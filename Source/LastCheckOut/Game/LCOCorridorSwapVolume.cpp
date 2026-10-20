#include "Game/LCOCorridorSwapVolume.h"
#include "LastCheckOut.h"
#include "Player/LCOPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

ALCOCorridorSwapVolume::ALCOCorridorSwapVolume()
{
	// 플레이어가 볼륨 안에 있을 때만 Tick
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(200.f, 150.f, 150.f));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));   // 막지 않고 겹침만 감지
	TriggerBox->ShapeColor = FColor::Purple;                 // 에디터에서 보이는 색
	TriggerBox->SetHiddenInGame(true);                       // 게임 중에는 안 보임

	DeadEndArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DeadEndArrow"));
	DeadEndArrow->SetupAttachment(TriggerBox);
	DeadEndArrow->ArrowSize = 3.f;
	DeadEndArrow->SetHiddenInGame(true);
}

void ALCOCorridorSwapVolume::BeginPlay()
{
	Super::BeginPlay();

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);

	// 걸어온 복도를 막을 벽은 처음엔 숨김
	SetActorsVisible(ActorsToShow, false);

	// 배치 실수 확인용
	if (ActorsToHide.IsEmpty() || ActorsToShow.IsEmpty())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOCorridorSwap] %s: ActorsToHide = %d, ActorsToShow = %d (empty list)"),
			*GetName(), ActorsToHide.Num(), ActorsToShow.Num());
	}
}

void ALCOCorridorSwapVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHasSwapped || !Cast<ALCOPlayerCharacter>(OtherActor))
	{
		return;
	}

	bPlayerInside = true;
	SetActorTickEnabled(true);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOCorridorSwap] %s: Player entered"), *GetName());
}

void ALCOCorridorSwapVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!Cast<ALCOPlayerCharacter>(OtherActor))
	{
		return;
	}

	bPlayerInside = false;
	SetActorTickEnabled(false);
}

void ALCOCorridorSwapVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPlayerInside || bHasSwapped)
	{
		SetActorTickEnabled(false);
		return;
	}

	float Angle = 0.f;
	float HalfFOV = 0.f;
	if (!GetCameraYawAngle(Angle, HalfFOV))
	{
		return;
	}

	// 1) 먼저 막다른 길을 정면으로 봐야 함 (볼륨에 들어오자마자 바뀌는 것 방지)
	if (!bHasSeenDeadEnd)
	{
		if (Angle <= DeadEndLookAngle)
		{
			bHasSeenDeadEnd = true;

			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOCorridorSwap] %s: Dead end seen"), *GetName());
		}
		return;
	}

	// 2) 앞(막다른 길)도 뒤(걸어온 복도)도 화면에 안 보이는 순간 바꿈
	const float ViewLimit = HalfFOV + ViewMargin;
	const bool bFrontInView = Angle < ViewLimit;
	const bool bBackInView = Angle > 180.f - ViewLimit;

	if (!bFrontInView && !bBackInView)
	{
		ApplySwappedState();
		SetActorTickEnabled(false);

		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOCorridorSwap] %s: Swapped (CameraAngle = %.1f, ViewLimit = %.1f)"),
			*GetName(), Angle, ViewLimit);

		OnCorridorSwapped();
	}
}

bool ALCOCorridorSwapVolume::GetCameraYawAngle(float& OutAngle, float& OutHalfFOV) const
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return false;
	}

	// 위아래(Pitch)는 무시하고 좌우 방향만 비교
	FVector CameraDir = CameraManager->GetCameraRotation().Vector();
	CameraDir.Z = 0.f;
	FVector DeadEndDir = GetActorForwardVector();
	DeadEndDir.Z = 0.f;

	if (!CameraDir.Normalize() || !DeadEndDir.Normalize())
	{
		return false;
	}

	const float Dot = FMath::Clamp(FVector::DotProduct(CameraDir, DeadEndDir), -1.f, 1.f);
	OutAngle = FMath::RadiansToDegrees(FMath::Acos(Dot));
	OutHalfFOV = CameraManager->GetFOVAngle() * 0.5f;
	return true;
}

void ALCOCorridorSwapVolume::ApplySwappedState()
{
	bHasSwapped = true;
	SetActorsVisible(ActorsToHide, false);
	SetActorsVisible(ActorsToShow, true);
}

void ALCOCorridorSwapVolume::SetActorsVisible(const TArray<TObjectPtr<AActor>>& Actors, bool bVisible)
{
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(!bVisible);
			Actor->SetActorEnableCollision(bVisible);
		}
	}
}

FLCOActorSaveState ALCOCorridorSwapVolume::SaveActorState_Implementation() const
{
	FLCOActorSaveState State;
	State.bIsEnabled = !bHasSwapped;
	return State;
}

void ALCOCorridorSwapVolume::LoadActorState_Implementation(const FLCOActorSaveState& State)
{
	// 이미 바뀐 기록이면 연출 없이 바뀐 상태로 맞춤
	if (!State.bIsEnabled && !bHasSwapped)
	{
		ApplySwappedState();

		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOCorridorSwap] %s: Restored as swapped"), *GetName());
	}
}
