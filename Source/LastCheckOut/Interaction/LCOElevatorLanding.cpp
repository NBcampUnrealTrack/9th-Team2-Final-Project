#include "Interaction/LCOElevatorLanding.h"
#include "Interaction/LCOElevator.h"

ALCOElevatorLanding::ALCOElevatorLanding()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALCOElevatorLanding::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(Elevator))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOElevatorLanding] %s has no Elevator"), *GetName());
		return;
	}

	MyFloorIndex = Elevator->FindFloorIndex(FloorID);
	if (MyFloorIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOElevatorLanding] %s: FloorID %s is not in %s"), *GetName(), *FloorID.ToString(), *Elevator->GetName());
		return;
	}

	Elevator->OnInitializedDelegate.AddUniqueDynamic(this, &ALCOElevatorLanding::HandleElevatorInitialized);
	Elevator->OnDoorsOpeningDelegate.AddUniqueDynamic(this, &ALCOElevatorLanding::HandleDoorsOpening);
	Elevator->OnDoorsClosingDelegate.AddUniqueDynamic(this, &ALCOElevatorLanding::HandleDoorsClosing);

	// 엘리베이터가 먼저 BeginPlay를 마쳤으면 초기화 알림을 이미 놓쳤으므로 현재 상태로 맞춤
	// (아직이면 엘리베이터 BeginPlay의 OnInitializedDelegate로 맞춰짐)
	if (Elevator->HasActorBegunPlay())
	{
		const bool bDoorsOpen = Elevator->GetCurrentFloorIndex() == MyFloorIndex
			&& Elevator->GetElevatorState() == ELCOElevatorState::Idle;
		OnLandingInitialized(bDoorsOpen);
	}
	else
	{
		OnLandingInitialized(false);
	}
}

void ALCOElevatorLanding::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	if (!IsValid(Elevator))
	{
		return;
	}

	// 이미 이 층에 있으면 RequestFloor가 true여도 이동하지 않으므로 알림 없음
	const bool bAlreadyHere = Elevator->GetCurrentFloorIndex() == MyFloorIndex;
	if (Elevator->RequestFloor(FloorID, Interactor) && !bAlreadyHere)
	{
		OnLandingCalled(SourceComponent);
	}
}

bool ALCOElevatorLanding::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return !IsValid(Elevator) || MyFloorIndex == INDEX_NONE || Elevator->GetElevatorState() != ELCOElevatorState::Idle;
}

FText ALCOElevatorLanding::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return IsValid(Elevator) ? Elevator->GetPromptForFloor(FloorID, Interactor) : FText::GetEmpty();
}

TArray<FName> ALCOElevatorLanding::GetFloorIDOptions() const
{
	TArray<FName> Options;
	if (IsValid(Elevator))
	{
		for (const FLCOElevatorFloor& Floor : Elevator->GetFloors())
		{
			Options.Add(Floor.FloorID);
		}
	}
	return Options;
}

void ALCOElevatorLanding::HandleElevatorInitialized(ALCOElevator* InElevator, int32 FloorIndex)
{
	OnLandingInitialized(FloorIndex == MyFloorIndex);
}

void ALCOElevatorLanding::HandleDoorsOpening(ALCOElevator* InElevator, int32 FloorIndex)
{
	if (FloorIndex == MyFloorIndex)
	{
		OnLandingDoorsOpening();
	}
}

void ALCOElevatorLanding::HandleDoorsClosing(ALCOElevator* InElevator, int32 FloorIndex)
{
	if (FloorIndex == MyFloorIndex)
	{
		OnLandingDoorsClosing();
	}
}
