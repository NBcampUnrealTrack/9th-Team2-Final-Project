#include "Game/LCOScareDoor.h"
#include "LastCheckOut.h"
#include "Components/StaticMeshComponent.h"

ALCOScareDoor::ALCOScareDoor()
{
	// 연출 중에만 Tick (평소에는 꺼둠)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(RootComponent);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
}

void ALCOScareDoor::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 배치한 회전을 닫힌 상태로 사용
	ClosedHingeRotation = Hinge->GetRelativeRotation();

	// 틈으로 엿보는 연출: 처음부터 살짝 열어 둠
	if (bStartAjar)
	{
		ApplyOpenAlpha(1.f);
	}
}

void ALCOScareDoor::PlayScare()
{
	if (Phase != EScarePhase::Idle)
	{
		return;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOScareDoor] %s PlayScare (Angle = %.1f)"), *GetName(), PeekAngle);

	SetPhase(EScarePhase::Opening);
	SetActorTickEnabled(true);
	OnPeekStarted();
}

void ALCOScareDoor::PlaySlam()
{
	if (Phase != EScarePhase::Idle)
	{
		return;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOScareDoor] %s PlaySlam"), *GetName());

	SetPhase(EScarePhase::Closing);
	SetActorTickEnabled(true);
}

void ALCOScareDoor::CloseImmediately()
{
	SetPhase(EScarePhase::Idle);
	SetActorTickEnabled(false);
	ApplyOpenAlpha(0.f);
}

void ALCOScareDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PhaseElapsed += DeltaSeconds;

	switch (Phase)
	{
	case EScarePhase::Opening:
	{
		// 천천히 열리기 시작해서 천천히 멈춤
		const float T = FMath::Clamp(PhaseElapsed / OpenTime, 0.f, 1.f);
		ApplyOpenAlpha(FMath::InterpEaseInOut(0.f, 1.f, T, 2.f));
		if (T >= 1.f)
		{
			SetPhase(EScarePhase::Holding);
		}
		break;
	}

	case EScarePhase::Holding:
		if (PhaseElapsed >= HoldTime)
		{
			SetPhase(EScarePhase::Closing);
		}
		break;

	case EScarePhase::Closing:
	{
		// 점점 빨라지며 닫힘
		const float T = FMath::Clamp(PhaseElapsed / CloseTime, 0.f, 1.f);
		ApplyOpenAlpha(CloseStartAlpha * (1.f - FMath::InterpEaseIn(0.f, 1.f, T, 2.f)));
		if (T >= 1.f)
		{
			ApplyOpenAlpha(0.f);
			SetPhase(EScarePhase::Idle);
			SetActorTickEnabled(false);

			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOScareDoor] %s Slammed"), *GetName());
			OnSlammed();
			OnDoorSlammed.Broadcast(this);
		}
		break;
	}

	case EScarePhase::Idle:
	default:
		SetActorTickEnabled(false);
		break;
	}
}

void ALCOScareDoor::SetPhase(EScarePhase NewPhase)
{
	Phase = NewPhase;
	PhaseElapsed = 0.f;

	// 닫히기 시작하는 순간의 열린 정도 기억
	if (NewPhase == EScarePhase::Closing)
	{
		CloseStartAlpha = CurrentAlpha;
	}
}

void ALCOScareDoor::ApplyOpenAlpha(float Alpha)
{
	CurrentAlpha = Alpha;

	const float Direction = bReverseDirection ? -1.f : 1.f;
	FRotator NewRotation = ClosedHingeRotation;
	NewRotation.Yaw += PeekAngle * Alpha * Direction;
	Hinge->SetRelativeRotation(NewRotation);
}
