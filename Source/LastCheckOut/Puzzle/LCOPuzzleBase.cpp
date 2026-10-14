#include "Puzzle/LCOPuzzleBase.h"
#include "Puzzle/LCOPuzzleSubsystem.h"
#include "Interaction/LCOInteractComponent.h"
#include "Interaction/LCOOpenableBase.h"
#include "Engine/GameInstance.h"

ALCOPuzzleBase::ALCOPuzzleBase()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	InactivePrompt = NSLOCTEXT("LCOInteraction", "PuzzleInactivePrompt", "작동하지 않는다");
}

void ALCOPuzzleBase::BeginPlay()
{
	Super::BeginPlay();

	if (PuzzleID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzleBase] %s has no PuzzleID (solved state will not be saved)"), *GetName());
	}

	// 선행 퍼즐이 있으면 모두 해결될 때까지 비활성
	bIsActive = RequiredPuzzles.IsEmpty() ? bStartActive : AreRequiredPuzzlesSolved();

	for (ALCOPuzzleBase* RequiredPuzzle : RequiredPuzzles)
	{
		if (IsValid(RequiredPuzzle))
		{
			RequiredPuzzle->OnSolved.AddUniqueDynamic(this, &ALCOPuzzleBase::HandleRequiredPuzzleSolved);
		}
	}

	// 세이브 적용이 이 BeginPlay보다 늦을 수 있으므로 복원 알림도 받는다
	if (ULCOPuzzleSubsystem* PuzzleSubsystem = GetGameInstance()->GetSubsystem<ULCOPuzzleSubsystem>())
	{
		RestoredHandle = PuzzleSubsystem->OnSolvedPuzzlesRestored.AddUObject(this, &ALCOPuzzleBase::TryRestoreSolvedState);
	}

	TryRestoreSolvedState();
}

void ALCOPuzzleBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (ULCOPuzzleSubsystem* PuzzleSubsystem = GameInstance->GetSubsystem<ULCOPuzzleSubsystem>())
		{
			PuzzleSubsystem->OnSolvedPuzzlesRestored.Remove(RestoredHandle);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ALCOPuzzleBase::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	// IsInteractionLocked에서 이미 막지만, BP / 코드에서 직접 호출되는 경우 대비
	if (bIsSolved || !bIsActive)
	{
		return;
	}

	const FName InputID = SourceComponent ? SourceComponent->GetInputID() : NAME_None;
	HandlePuzzleInput(SourceComponent, InputID, Interactor);
}

bool ALCOPuzzleBase::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return bIsSolved || !bIsActive;
}

FText ALCOPuzzleBase::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 잠긴 상태에서는 포커스되지 않지만, 문구를 미리 확인하는 경우 대비
	return bIsActive ? FText::GetEmpty() : InactivePrompt;
}

void ALCOPuzzleBase::HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor)
{
	// 기본 동작은 로그만 출력. 실제 로직은 자식 클래스에서 구현
	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s HandlePuzzleInput: Source=%s, InputID=%s, Interactor=%s"),
		*GetName(), *GetNameSafe(SourceComponent), *InputID.ToString(), *GetNameSafe(Interactor));
}

void ALCOPuzzleBase::OnPuzzleReset_Implementation()
{
}

void ALCOPuzzleBase::SolvePuzzle()
{
	if (bIsSolved)
	{
		return;
	}

	bIsSolved = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s Solved (PuzzleID: %s)"), *GetName(), *PuzzleID.ToString());

	if (ULCOPuzzleSubsystem* PuzzleSubsystem = GetGameInstance()->GetSubsystem<ULCOPuzzleSubsystem>())
	{
		PuzzleSubsystem->MarkPuzzleSolved(PuzzleID);
	}

	ApplyWorldResult();
	StoryResult.Apply(GetGameInstance());

	OnPuzzleSolved();
	OnSolved.Broadcast(this);
}

void ALCOPuzzleBase::FailPuzzle()
{
	if (bIsSolved || !bIsActive)
	{
		return;
	}

	++FailCount;

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s Failed (Count: %d)"), *GetName(), FailCount);

	OnPuzzleFailed();
	OnFailed.Broadcast(this);

	if (bResetOnFail)
	{
		ResetPuzzle();
	}
}

void ALCOPuzzleBase::ResetPuzzle()
{
	if (bIsSolved)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s Reset"), *GetName());

	OnPuzzleReset();
}

void ALCOPuzzleBase::Activate()
{
	if (bIsActive)
	{
		return;
	}

	bIsActive = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s Activated"), *GetName());

	OnPuzzleActivated();
	OnActivated.Broadcast(this);
}

void ALCOPuzzleBase::Deactivate()
{
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s Deactivated"), *GetName());
}

bool ALCOPuzzleBase::AreRequiredPuzzlesSolved() const
{
	const ULCOPuzzleSubsystem* PuzzleSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOPuzzleSubsystem>() : nullptr;

	for (const ALCOPuzzleBase* RequiredPuzzle : RequiredPuzzles)
	{
		if (!IsValid(RequiredPuzzle))
		{
			continue;
		}

		// BeginPlay 순서와 상관없이 판단하도록 저장된 해결 기록도 확인
		const bool bSolved = RequiredPuzzle->IsSolved()
			|| (PuzzleSubsystem && PuzzleSubsystem->IsPuzzleSolved(RequiredPuzzle->GetPuzzleID()));
		if (!bSolved)
		{
			return false;
		}
	}

	return true;
}

void ALCOPuzzleBase::TryRestoreSolvedState()
{
	if (bIsSolved)
	{
		return;
	}

	const ULCOPuzzleSubsystem* PuzzleSubsystem = GetGameInstance()->GetSubsystem<ULCOPuzzleSubsystem>();
	if (!PuzzleSubsystem || !PuzzleSubsystem->IsPuzzleSolved(PuzzleID))
	{
		// 선행 퍼즐이 복원으로 해결 처리됐을 수 있으므로 활성화 조건만 다시 확인
		if (!bIsActive && !RequiredPuzzles.IsEmpty() && AreRequiredPuzzlesSolved())
		{
			Activate();
		}
		return;
	}

	bIsSolved = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s Restored as solved (PuzzleID: %s)"), *GetName(), *PuzzleID.ToString());

	// 스토리 결과는 세이브에 이미 반영되어 있으므로 월드 결과만 다시 적용
	ApplyWorldResult();
	OnPuzzleRestored();
}

void ALCOPuzzleBase::ApplyWorldResult()
{
	for (ALCOOpenableBase* Target : UnlockTargets)
	{
		if (!IsValid(Target))
		{
			continue;
		}

		Target->Unlock();

		if (bOpenTargetsOnSolve)
		{
			Target->Open();
		}
	}
}

void ALCOPuzzleBase::HandleRequiredPuzzleSolved(ALCOPuzzleBase* RequiredPuzzle)
{
	if (!bIsActive && AreRequiredPuzzlesSolved())
	{
		Activate();
	}
}
