#include "Interaction/LCOSwitch.h"
#include "Interaction/LCOOpenableBase.h"
#include "Puzzle/LCOPuzzleBase.h"

ALCOSwitch::ALCOSwitch()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	TurnOffPrompt = NSLOCTEXT("LCOInteraction", "SwitchTurnOffPrompt", "끄기");
}

void ALCOSwitch::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 설정한 시작 상태를 BP 연출에 반영 (대상에는 적용하지 않음 → 대상의 시작 상태는 대상에서 설정)
	OnSwitchInitialized(bIsOn);
}

void ALCOSwitch::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	Toggle();
}

bool ALCOSwitch::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return bIsOn && !bCanTurnOff;
}

FText ALCOSwitch::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return bIsOn ? TurnOffPrompt : TurnOnPrompt;
}

FLCOActorSaveState ALCOSwitch::SaveActorState_Implementation() const
{
	FLCOActorSaveState State;
	State.bIsEnabled = bIsOn;
	return State;
}

void ALCOSwitch::LoadActorState_Implementation(const FLCOActorSaveState& State)
{
	bIsOn = State.bIsEnabled;

	UE_LOG(LogTemp, Log, TEXT("[LCOSwitch] %s LoadActorState (On: %d)"), *GetName(), bIsOn);

	// 대상(퍼즐, 문)은 각자 상태를 복원하므로 다시 적용하지 않음. 모습만 맞춤
	OnSwitchInitialized(bIsOn);
}

void ALCOSwitch::TurnOn()
{
	if (bIsOn)
	{
		return;
	}

	bIsOn = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOSwitch] %s TurnOn"), *GetName());

	ApplyTargets();
	OnTurnedOn();
	OnSwitchChanged.Broadcast(this, bIsOn);
}

void ALCOSwitch::TurnOff()
{
	if (!bIsOn || !bCanTurnOff)
	{
		return;
	}

	bIsOn = false;

	UE_LOG(LogTemp, Log, TEXT("[LCOSwitch] %s TurnOff"), *GetName());

	ApplyTargets();
	OnTurnedOff();
	OnSwitchChanged.Broadcast(this, bIsOn);
}

void ALCOSwitch::Toggle()
{
	if (bIsOn)
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}

void ALCOSwitch::ApplyTargets()
{
	for (ALCOPuzzleBase* Puzzle : PuzzleTargets)
	{
		if (!IsValid(Puzzle))
		{
			continue;
		}

		if (bIsOn)
		{
			Puzzle->Activate();
		}
		else
		{
			Puzzle->Deactivate();
		}
	}

	if (!bIsOn)
	{
		return;
	}

	for (ALCOOpenableBase* Target : UnlockTargets)
	{
		if (IsValid(Target))
		{
			Target->Unlock();
		}
	}
}
