#include "Interaction/LCOElevator.h"
#include "Interaction/LCOInteractComponent.h"
#include "Inventory/InventoryComponent.h"
#include "TimerManager.h"

ALCOElevator::ALCOElevator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Car = CreateDefaultSubobject<USceneComponent>(TEXT("Car"));
	Car->SetupAttachment(RootComponent);

	UnpoweredPrompt = NSLOCTEXT("LCOInteraction", "ElevatorUnpoweredPrompt", "작동하지 않는다");
	LockedFloorPrompt = NSLOCTEXT("LCOInteraction", "ElevatorLockedFloorPrompt", "카드키가 필요하다");
}

void ALCOElevator::BeginPlay()
{
	Super::BeginPlay();

	if (Floors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOElevator] %s has no Floors"), *GetName());
		return;
	}

	CurrentFloorIndex = FMath::Clamp(StartFloorIndex, 0, Floors.Num() - 1);
	TargetFloorIndex = CurrentFloorIndex;
	State = ELCOElevatorState::Idle;

	SnapCarToFloor(CurrentFloorIndex);
	OnElevatorInitialized(CurrentFloorIndex);
	OnInitializedDelegate.Broadcast(this, CurrentFloorIndex);
}

void ALCOElevator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State != ELCOElevatorState::Moving || !Floors.IsValidIndex(TargetFloorIndex))
	{
		SetActorTickEnabled(false);
		return;
	}

	// 일정한 속도로 목표 높이까지 이동 (X / Y는 그대로 유지)
	const FVector Location = Car->GetRelativeLocation();
	const float TargetZ = Floors[TargetFloorIndex].Height;
	const float NewZ = FMath::FInterpConstantTo(Location.Z, TargetZ, DeltaTime, MoveSpeed);
	Car->SetRelativeLocation(FVector(Location.X, Location.Y, NewZ));

	if (FMath::IsNearlyEqual(NewZ, TargetZ, KINDA_SMALL_NUMBER))
	{
		Arrive();
	}
}

void ALCOElevator::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	const FName FloorID = SourceComponent ? SourceComponent->GetInputID() : NAME_None;
	if (FloorID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOElevator] %s: %s has no InputID (set it to a FloorID)"), *GetName(), *GetNameSafe(SourceComponent));
		return;
	}

	RequestFloor(FloorID, Interactor);
}

bool ALCOElevator::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return State != ELCOElevatorState::Idle;
}

FText ALCOElevator::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return GetPromptForFloor(SourceComponent ? SourceComponent->GetInputID() : NAME_None, Interactor);
}

FText ALCOElevator::GetPromptForFloor(FName FloorID, const AActor* Interactor) const
{
	if (!bIsPowered)
	{
		return UnpoweredPrompt;
	}

	const int32 FloorIndex = FindFloorIndex(FloorID);
	if (Floors.IsValidIndex(FloorIndex) && !HasRequiredItem(Floors[FloorIndex], Interactor))
	{
		return LockedFloorPrompt;
	}

	// 그 외에는 버튼 컴포넌트의 기본 문구 (예: "2층", "엘리베이터 호출")
	return FText::GetEmpty();
}

FLCOActorSaveState ALCOElevator::SaveActorState_Implementation() const
{
	// 현재 층은 저장하지 않음 (불러오면 항상 시작 층에서 대기)
	FLCOActorSaveState SaveState;
	SaveState.bIsEnabled = bIsPowered;
	return SaveState;
}

void ALCOElevator::LoadActorState_Implementation(const FLCOActorSaveState& SaveState)
{
	// 전원만 복원. 층은 BeginPlay에서 이미 시작 층으로 맞춰져 있음
	bIsPowered = SaveState.bIsEnabled;

	UE_LOG(LogTemp, Log, TEXT("[LCOElevator] %s LoadActorState (Powered: %d)"), *GetName(), bIsPowered);
}

bool ALCOElevator::RequestFloor(FName FloorID, AActor* Requester)
{
	if (State != ELCOElevatorState::Idle)
	{
		return false;
	}

	int32 FloorIndex = INDEX_NONE;
	if (!CanRequestFloor(FloorID, Requester, FloorIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOElevator] %s Request denied: %s"), *GetName(), *FloorID.ToString());
		OnRequestDenied(Requester, FloorIndex);
		return false;
	}

	// 이미 그 층에 있으면 문이 열려 있으므로 할 일 없음
	if (FloorIndex == CurrentFloorIndex)
	{
		return true;
	}

	TargetFloorIndex = FloorIndex;

	UE_LOG(LogTemp, Log, TEXT("[LCOElevator] %s Request: %s -> %s"), *GetName(), *GetCurrentFloorID().ToString(), *FloorID.ToString());

	OnFloorRequested(CurrentFloorIndex, TargetFloorIndex, Requester);
	StartClosingDoors();
	return true;
}

void ALCOElevator::SetPowered(bool bNewPowered)
{
	if (bIsPowered == bNewPowered)
	{
		return;
	}

	bIsPowered = bNewPowered;

	UE_LOG(LogTemp, Log, TEXT("[LCOElevator] %s Powered: %d"), *GetName(), bIsPowered);
}

FName ALCOElevator::GetCurrentFloorID() const
{
	return Floors.IsValidIndex(CurrentFloorIndex) ? Floors[CurrentFloorIndex].FloorID : NAME_None;
}

int32 ALCOElevator::FindFloorIndex(FName FloorID) const
{
	return Floors.IndexOfByPredicate([FloorID](const FLCOElevatorFloor& Floor)
	{
		return Floor.FloorID == FloorID;
	});
}

bool ALCOElevator::CanRequestFloor(FName FloorID, const AActor* Requester, int32& OutFloorIndex) const
{
	OutFloorIndex = FindFloorIndex(FloorID);

	if (OutFloorIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOElevator] %s: unknown FloorID %s"), *GetName(), *FloorID.ToString());
		return false;
	}

	return bIsPowered && HasRequiredItem(Floors[OutFloorIndex], Requester);
}

bool ALCOElevator::HasRequiredItem(const FLCOElevatorFloor& Floor, const AActor* Requester) const
{
	if (Floor.RequiredItemID.IsNone())
	{
		return true;
	}

	const UInventoryComponent* Inventory = IsValid(Requester) ? Requester->FindComponentByClass<UInventoryComponent>() : nullptr;
	return Inventory && Inventory->HasItem(Floor.RequiredItemID);
}

void ALCOElevator::StartClosingDoors()
{
	State = ELCOElevatorState::DoorsClosing;
	OnDoorsClosing(CurrentFloorIndex);
	OnDoorsClosingDelegate.Broadcast(this, CurrentFloorIndex);

	GetWorldTimerManager().SetTimer(DoorTimerHandle, this, &ALCOElevator::StartMoving, FMath::Max(DoorDuration, KINDA_SMALL_NUMBER), false);
}

void ALCOElevator::StartMoving()
{
	State = ELCOElevatorState::Moving;
	OnMoveStarted(CurrentFloorIndex, TargetFloorIndex);

	SetActorTickEnabled(true);
}

void ALCOElevator::Arrive()
{
	SetActorTickEnabled(false);

	CurrentFloorIndex = TargetFloorIndex;

	UE_LOG(LogTemp, Log, TEXT("[LCOElevator] %s Arrived: %s"), *GetName(), *GetCurrentFloorID().ToString());

	OnArrived(CurrentFloorIndex);
	OnArrivedDelegate.Broadcast(this, CurrentFloorIndex);

	State = ELCOElevatorState::DoorsOpening;
	OnDoorsOpening(CurrentFloorIndex);
	OnDoorsOpeningDelegate.Broadcast(this, CurrentFloorIndex);

	GetWorldTimerManager().SetTimer(DoorTimerHandle, this, &ALCOElevator::FinishOpeningDoors, FMath::Max(DoorDuration, KINDA_SMALL_NUMBER), false);
}

void ALCOElevator::FinishOpeningDoors()
{
	State = ELCOElevatorState::Idle;
}

void ALCOElevator::SnapCarToFloor(int32 FloorIndex)
{
	if (!Floors.IsValidIndex(FloorIndex))
	{
		return;
	}

	const FVector Location = Car->GetRelativeLocation();
	Car->SetRelativeLocation(FVector(Location.X, Location.Y, Floors[FloorIndex].Height));
}
