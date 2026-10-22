#include "Puzzle/LCOPuzzle_Clock.h"
#include "Interaction/LCOInteractComponent.h"
#include "Player/LCOPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace
{
	constexpr float DegreesPerHour = 360.f / 12.f;
	constexpr float DegreesPerMinute = 360.f / 60.f;
}

ALCOPuzzle_Clock::ALCOPuzzle_Clock()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(RootComponent);

	// 오답이어도 맞춰 둔 시각은 유지 (조금만 고쳐서 다시 확정할 수 있도록)
	bResetOnFail = false;
}

void ALCOPuzzle_Clock::BeginPlay()
{
	// Super::BeginPlay 안에서 해결 상태 복원(진자 흔들기 시작)이 일어날 수 있으므로 먼저 진자를 찾아 둠
	CachePendulum();

	Super::BeginPlay();

	// 이미 해결 상태로 복원됐으면 HandleRestoredAsSolved에서 정답 시각으로 맞춰져 있음
	if (!IsSolved())
	{
		SetTime(StartHour, StartMinute, true);
	}

	if (AnswerMinute % MinuteStep != 0 || StartMinute % MinuteStep != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_Clock] %s: AnswerMinute %d / StartMinute %d should be multiples of MinuteStep %d"),
			*GetName(), AnswerMinute, StartMinute, MinuteStep);
	}
}

void ALCOPuzzle_Clock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsViewing())
	{
		ExitClockView();
	}

	Super::EndPlay(EndPlayReason);
}

void ALCOPuzzle_Clock::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 바늘 회전 애니메이션
	if (IsHandsAnimating())
	{
		UpdateHandAnimation(DeltaTime);
	}

	// 보기 중: 커서 아래 바늘 / 버튼 외곽선 표시
	if (IsViewing())
	{
		SetHoveredComponent(GetComponentUnderCursor());
	}

	// 진자 흔들림 (오답 떨림 / 정답 후 계속 흔들림)
	if (IsPendulumMoving())
	{
		UpdatePendulum(DeltaTime);
	}

	UpdateTickEnabled();
}

bool ALCOPuzzle_Clock::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return Super::IsInteractionLocked_Implementation(SourceComponent, Interactor) || IsViewing();
}

bool ALCOPuzzle_Clock::IsHandsAnimating() const
{
	return !FMath::IsNearlyEqual(DisplayHourAngle, TargetHourAngle) || !FMath::IsNearlyEqual(DisplayMinuteAngle, TargetMinuteAngle);
}

void ALCOPuzzle_Clock::HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor)
{
	if (IsViewing())
	{
		return;
	}

	// 바늘 / 확정 외의 부품(뚜껑 등)은 F키로 시계 보기를 시작하지 않음
	// (F키 동작은 BP의 OnPuzzleInteracted에서 처리. OnClockPartClicked는 시계 보기 중 클릭에만 호출)
	const bool bIsClockControl = InputID.IsNone() || InputID == HourHandID || InputID == MinuteHandID || InputID == ConfirmID;
	if (!bIsClockControl)
	{
		return;
	}

	// 몸통 / 바늘 / 확정 부품: 시계 보기 시작
	EnterClockView(Interactor);
}

void ALCOPuzzle_Clock::OnPuzzleReset_Implementation()
{
	Super::OnPuzzleReset_Implementation();

	SetTime(StartHour, StartMinute, false);
}

void ALCOPuzzle_Clock::HandleRestoredAsSolved()
{
	SetTime(AnswerHour, AnswerMinute, true);

	// 이어하기: 시계가 다시 움직이는 상태로
	StartPendulumSwing();
}

void ALCOPuzzle_Clock::EnterClockView(AActor* Interactor)
{
	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	ViewingController = PC;

	// 이동 / 시점 / F키 상호작용 차단 + 마우스 커서 표시
	// 퍼즐 보기 상태: 인벤토리 / 수첩 키 무시, ESC(옵션 키)는 일시정지 대신 OnPuzzleExitRequested로 전달
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
	{
		LCOPC->SetUIMode(true);
		LCOPC->BeginPuzzleView(this);
		LCOPC->OnPuzzleExitRequested.AddUniqueDynamic(this, &ALCOPuzzle_Clock::HandlePuzzleExitRequested);
	}

	PC->SetViewTargetWithBlend(this, ViewBlendTime);

	// 클릭 / 나가기 입력
	EnableInput(PC);
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALCOPuzzle_Clock::HandleLeftClick);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ALCOPuzzle_Clock::HandleExitInput);
	}

	UpdateTickEnabled();

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Clock] %s EnterClockView"), *GetName());
	OnClockViewEntered();
}

void ALCOPuzzle_Clock::ExitClockView()
{
	if (!IsViewing())
	{
		return;
	}

	APlayerController* PC = ViewingController;
	ViewingController = nullptr;

	GetWorldTimerManager().ClearTimer(ExitTimerHandle);
	SetHoveredComponent(nullptr);

	DisableInput(PC);
	if (InputComponent)
	{
		InputComponent->KeyBindings.Empty();
	}

	if (IsValid(PC))
	{
		PC->SetViewTargetWithBlend(PC->GetPawn(), ViewBlendTime);

		if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
		{
			LCOPC->OnPuzzleExitRequested.RemoveDynamic(this, &ALCOPuzzle_Clock::HandlePuzzleExitRequested);
			LCOPC->EndPuzzleView();
			LCOPC->SetUIMode(false);
		}
	}

	// 회전 중이던 바늘은 끝까지 돌도록 Tick 유지
	UpdateTickEnabled();

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Clock] %s ExitClockView"), *GetName());
	OnClockViewExited();
}

void ALCOPuzzle_Clock::HandleLeftClick()
{
	// 해결 후 자동 종료를 기다리는 동안에는 조작 불가
	if (IsSolved())
	{
		return;
	}

	ULCOInteractComponent* Clicked = GetComponentUnderCursor();
	if (!Clicked)
	{
		return;
	}

	const FName InputID = Clicked->GetInputID();
	const bool bReverse = ViewingController && ViewingController->IsInputKeyDown(EKeys::LeftShift);
	const int32 Direction = bReverse ? -1 : 1;

	if (InputID == HourHandID)
	{
		TurnHourHand(Direction);
	}
	else if (InputID == MinuteHandID)
	{
		TurnMinuteHand(Direction);
	}
	else if (InputID == ConfirmID)
	{
		// 바늘이 아직 돌고 있으면 다 돈 뒤에 확정 (보이는 시각과 판정 시각이 어긋나지 않도록)
		if (!IsHandsAnimating())
		{
			Confirm();
		}
	}
	else
	{
		// 그 밖의 부품 (뚜껑 등): BP에서 처리
		OnClockPartClicked(InputID, Clicked);
	}
}

void ALCOPuzzle_Clock::HandleExitInput()
{
	ExitClockView();
}

void ALCOPuzzle_Clock::HandlePuzzleExitRequested(AActor* PuzzleActor)
{
	// 다른 퍼즐의 보기 종료 요청은 무시
	if (PuzzleActor == this)
	{
		ExitClockView();
	}
}

ULCOInteractComponent* ALCOPuzzle_Clock::GetComponentUnderCursor() const
{
	if (!ViewingController)
	{
		return nullptr;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!ViewingController->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return nullptr;
	}

	const FVector TraceEnd = WorldOrigin + WorldDirection * 10000.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOClockCursorTrace), false);
	if (APawn* Pawn = ViewingController->GetPawn())
	{
		Params.AddIgnoredActor(Pawn);
	}

	// 시계의 몸통 / 장식 메시는 충돌(Convex)이 실제 모양보다 커서 안쪽 부품(진자 등)을 가릴 수 있음
	// → 이 시계의 조작 부품이 아닌 컴포넌트에 맞으면 무시하고 그 뒤를 다시 확인
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility, Params))
		{
			return nullptr;
		}

		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!HitComponent || HitComponent->GetOwner() != this)
		{
			// 다른 액터가 앞을 막음
			return nullptr;
		}

		// InputID가 있는 부품만 조작 대상 (바늘, 확정, 뚜껑 등)
		ULCOInteractComponent* Component = Cast<ULCOInteractComponent>(HitComponent);
		if (Component && !Component->GetInputID().IsNone())
		{
			return Component;
		}

		Params.AddIgnoredComponent(HitComponent);
	}

	return nullptr;
}

void ALCOPuzzle_Clock::TurnHourHand(int32 Direction)
{
	CurrentHour = (CurrentHour + Direction + 12) % 12;
	TargetHourAngle += Direction * DegreesPerHour;

	OnHandTurned(true);

	if (HandRotateSpeed <= 0.f)
	{
		DisplayHourAngle = TargetHourAngle;
		BroadcastHandAngles(true);
	}

	UpdateTickEnabled();
}

void ALCOPuzzle_Clock::TurnMinuteHand(int32 Direction)
{
	// 분침만 이동 (시침은 그대로)
	CurrentMinute = (CurrentMinute + Direction * MinuteStep + 60) % 60;
	TargetMinuteAngle += Direction * MinuteStep * DegreesPerMinute;

	OnHandTurned(false);

	if (HandRotateSpeed <= 0.f)
	{
		DisplayMinuteAngle = TargetMinuteAngle;
		BroadcastHandAngles(true);
	}

	UpdateTickEnabled();
}

void ALCOPuzzle_Clock::SetTime(int32 Hour, int32 Minute, bool bInstant)
{
	CurrentHour = Hour % 12;
	CurrentMinute = FMath::Clamp(Minute, 0, 59);

	TargetHourAngle = CurrentHour * DegreesPerHour;
	TargetMinuteAngle = CurrentMinute * DegreesPerMinute;

	if (bInstant || HandRotateSpeed <= 0.f)
	{
		DisplayHourAngle = TargetHourAngle;
		DisplayMinuteAngle = TargetMinuteAngle;
		BroadcastHandAngles(true);
	}

	UpdateTickEnabled();
}

void ALCOPuzzle_Clock::Confirm()
{
	const bool bCorrect = CurrentHour == AnswerHour % 12 && CurrentMinute == AnswerMinute;
	if (bCorrect)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Clock] %s Correct time"), *GetName());
		StartPendulumSwing();
		SolvePuzzle();

		// 해결 연출을 잠깐 보여준 뒤 시계 보기 종료
		GetWorldTimerManager().SetTimer(ExitTimerHandle, this, &ALCOPuzzle_Clock::ExitClockView, FMath::Max(ExitDelayAfterSolve, KINDA_SMALL_NUMBER), false);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Clock] %s Wrong time (%d:%02d)"), *GetName(), GetCurrentHour(), CurrentMinute);
		StartPendulumShake();
		FailPuzzle();
	}
}

void ALCOPuzzle_Clock::CachePendulum()
{
	PendulumComponent = nullptr;

	// 1순위: "Pendulum" 태그가 붙은 컴포넌트 (진자 회전 중심용 Scene을 따로 둘 때)
	TArray<UActorComponent*> Tagged = GetComponentsByTag(USceneComponent::StaticClass(), PendulumTag);
	if (!Tagged.IsEmpty())
	{
		PendulumComponent = Cast<USceneComponent>(Tagged[0]);
	}

	// 2순위: InputID가 PendulumID인 LCOInteract (진자 자체가 확정 버튼인 경우)
	if (!PendulumComponent)
	{
		TArray<ULCOInteractComponent*> InteractComponents;
		GetComponents(InteractComponents);
		for (ULCOInteractComponent* Component : InteractComponents)
		{
			if (Component->GetInputID() == PendulumID)
			{
				PendulumComponent = Component;
				break;
			}
		}
	}

	if (PendulumComponent)
	{
		PendulumRestRotation = PendulumComponent->GetRelativeRotation();
	}
}

bool ALCOPuzzle_Clock::IsPendulumMoving() const
{
	return PendulumComponent && (bPendulumSwinging || PendulumShakeElapsed < FailShakeDuration);
}

void ALCOPuzzle_Clock::StartPendulumShake()
{
	// 이미 계속 흔들리고 있으면(해결 후) 떨림은 생략
	if (!PendulumComponent || bPendulumSwinging)
	{
		return;
	}

	PendulumShakeElapsed = 0.f;
	UpdateTickEnabled();
}

void ALCOPuzzle_Clock::StartPendulumSwing()
{
	if (!PendulumComponent)
	{
		return;
	}

	bPendulumSwinging = true;
	PendulumSwingElapsed = 0.f;
	PendulumShakeElapsed = FailShakeDuration;
	UpdateTickEnabled();
}

void ALCOPuzzle_Clock::UpdatePendulum(float DeltaTime)
{
	float Offset = 0.f;

	if (bPendulumSwinging)
	{
		// 정답 후: 일정한 주기로 계속 좌우로 흔들림
		PendulumSwingElapsed += DeltaTime;
		const float Period = FMath::Max(SolvedSwingPeriod, KINDA_SMALL_NUMBER);
		Offset = SolvedSwingAngle * FMath::Sin(2.f * PI * PendulumSwingElapsed / Period);
	}
	else
	{
		// 오답: 짧게 떨리다가 점점 약해지며 멈춤
		PendulumShakeElapsed = FMath::Min(PendulumShakeElapsed + DeltaTime, FailShakeDuration);
		const float Duration = FMath::Max(FailShakeDuration, KINDA_SMALL_NUMBER);
		const float Fade = 1.f - PendulumShakeElapsed / Duration;
		Offset = FailShakeAngle * Fade * FMath::Sin(2.f * PI * FailShakeFrequency * PendulumShakeElapsed);
	}

	// X축(Roll) 기준으로 흔들기
	FRotator Rotation = PendulumRestRotation;
	Rotation.Roll += Offset;
	PendulumComponent->SetRelativeRotation(Rotation);
}

bool ALCOPuzzle_Clock::UpdateHandAnimation(float DeltaTime)
{
	DisplayHourAngle = FMath::FInterpConstantTo(DisplayHourAngle, TargetHourAngle, DeltaTime, HandRotateSpeed);
	DisplayMinuteAngle = FMath::FInterpConstantTo(DisplayMinuteAngle, TargetMinuteAngle, DeltaTime, HandRotateSpeed);

	BroadcastHandAngles(false);
	return IsHandsAnimating();
}

void ALCOPuzzle_Clock::BroadcastHandAngles(bool bInstant)
{
	OnClockTimeChanged(DisplayHourAngle, DisplayMinuteAngle, bInstant);
}

void ALCOPuzzle_Clock::UpdateTickEnabled()
{
	SetActorTickEnabled(IsViewing() || IsHandsAnimating() || IsPendulumMoving());
}

void ALCOPuzzle_Clock::SetHoveredComponent(ULCOInteractComponent* NewHovered)
{
	if (HoveredComponent == NewHovered)
	{
		return;
	}

	if (IsValid(HoveredComponent))
	{
		HoveredComponent->SetFocused(false);
	}

	HoveredComponent = NewHovered;

	if (IsValid(HoveredComponent))
	{
		HoveredComponent->SetFocused(true);
	}
}
