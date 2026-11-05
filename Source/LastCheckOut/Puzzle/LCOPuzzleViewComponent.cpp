#include "Puzzle/LCOPuzzleViewComponent.h"
#include "Puzzle/LCOPuzzleBase.h"
#include "Interaction/LCOInteractComponent.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "Player/LCOPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ULCOPuzzleViewComponent::ULCOPuzzleViewComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ULCOPuzzleViewComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (Owner && !Owner->FindComponentByClass<UCameraComponent>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzleView] %s has no CameraComponent (view will use the actor origin)"), *GetNameSafe(Owner));
	}

	if (Owner && !Owner->IsA<ALCOPuzzleBase>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzleView] %s is not a puzzle (F key will not enter the view)"), *GetNameSafe(Owner));
	}
}

void ULCOPuzzleViewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsViewing())
	{
		ExitView();
	}

	Super::EndPlay(EndPlayReason);
}

void ULCOPuzzleViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 보기 중: 커서 아래 부품 외곽선 표시
	SetHoveredComponent(GetComponentUnderCursor());
}

bool ULCOPuzzleViewComponent::EnterView(AActor* Interactor)
{
	if (IsViewing())
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(Interactor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	AActor* Owner = GetOwner();
	if (!PC || !Owner)
	{
		return false;
	}

	ViewingController = PC;

	// 이동 / 시점 / F키 상호작용 차단 + 마우스 커서 표시
	// 퍼즐 보기 상태: 인벤토리 / 수첩 키 무시, ESC(옵션 키)는 일시정지 대신 OnPuzzleExitRequested로 전달
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
	{
		LCOPC->SetUIMode(true);
		LCOPC->BeginPuzzleView(Owner);
		LCOPC->OnPuzzleExitRequested.AddUniqueDynamic(this, &ULCOPuzzleViewComponent::HandlePuzzleExitRequested);
	}

	PC->SetViewTargetWithBlend(Owner, ViewBlendTime);

	// 클릭 / 나가기 입력
	if (!ViewInputComponent)
	{
		ViewInputComponent = NewObject<UInputComponent>(this, TEXT("PuzzleViewInput"));
		ViewInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ULCOPuzzleViewComponent::HandleLeftClick);
		ViewInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ULCOPuzzleViewComponent::HandleExitInput);
	}
	PC->PushInputComponent(ViewInputComponent);

	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleView] %s EnterView"), *GetNameSafe(Owner));
	OnViewEntered.Broadcast();
	return true;
}

void ULCOPuzzleViewComponent::ExitView()
{
	if (!IsViewing())
	{
		return;
	}

	APlayerController* PC = ViewingController;
	ViewingController = nullptr;

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExitTimerHandle);
	}

	SetHoveredComponent(nullptr);
	SetComponentTickEnabled(false);

	if (IsValid(PC))
	{
		PC->PopInputComponent(ViewInputComponent);
		PC->SetViewTargetWithBlend(PC->GetPawn(), ViewBlendTime);

		if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
		{
			LCOPC->OnPuzzleExitRequested.RemoveDynamic(this, &ULCOPuzzleViewComponent::HandlePuzzleExitRequested);
			LCOPC->EndPuzzleView();
			LCOPC->SetUIMode(false);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleView] %s ExitView"), *GetNameSafe(GetOwner()));
	OnViewExited.Broadcast();
}

void ULCOPuzzleViewComponent::HandlePuzzleSolved()
{
	if (!IsViewing())
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(ExitTimerHandle, this, &ULCOPuzzleViewComponent::ExitView, FMath::Max(ExitDelayAfterSolve, KINDA_SMALL_NUMBER), false);
}

void ULCOPuzzleViewComponent::HandleLeftClick()
{
	AActor* Owner = GetOwner();
	if (!ViewingController || !Owner || !Owner->Implements<ULCOInteractionReceiver>())
	{
		return;
	}

	// 해결 후 자동 종료를 기다리는 동안에는 조작 불가
	const ALCOPuzzleBase* Puzzle = Cast<ALCOPuzzleBase>(Owner);
	if (Puzzle && Puzzle->IsSolved())
	{
		return;
	}

	ULCOInteractComponent* Clicked = GetComponentUnderCursor();
	if (!Clicked)
	{
		return;
	}

	// F키 상호작용과 같은 경로로 전달 (보기 중이므로 퍼즐 입력으로 처리됨)
	ILCOInteractionReceiver::Execute_HandleInteraction(Owner, Clicked, ViewingController->GetPawn());
}

void ULCOPuzzleViewComponent::HandleExitInput()
{
	ExitView();
}

void ULCOPuzzleViewComponent::HandlePuzzleExitRequested(AActor* PuzzleActor)
{
	// 다른 퍼즐의 보기 종료 요청은 무시
	if (PuzzleActor == GetOwner())
	{
		ExitView();
	}
}

ULCOInteractComponent* ULCOPuzzleViewComponent::GetComponentUnderCursor() const
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

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOPuzzleViewCursorTrace), false);
	if (APawn* Pawn = ViewingController->GetPawn())
	{
		Params.AddIgnoredActor(Pawn);
	}

	// 몸체 / 장식 메시의 충돌이 부품을 가릴 수 있음
	// → 이 액터의 부품이 아닌 컴포넌트에 맞으면 무시하고 그 뒤를 다시 확인
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility, Params))
		{
			return nullptr;
		}

		UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (!HitComponent || HitComponent->GetOwner() != GetOwner())
		{
			// 다른 액터가 앞을 막음
			return nullptr;
		}

		// InputID가 있는 부품만 조작 대상 (숫자 키, 버튼 등)
		ULCOInteractComponent* Component = Cast<ULCOInteractComponent>(HitComponent);
		if (Component && !Component->GetInputID().IsNone())
		{
			return Component;
		}

		Params.AddIgnoredComponent(HitComponent);
	}

	return nullptr;
}

void ULCOPuzzleViewComponent::SetHoveredComponent(ULCOInteractComponent* NewHovered)
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
