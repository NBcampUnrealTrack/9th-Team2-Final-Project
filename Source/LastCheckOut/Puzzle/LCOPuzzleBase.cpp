#include "LCOPuzzleBase.h"
#include "Interaction/LCOInteractComponent.h"

ALCOPuzzleBase::ALCOPuzzleBase()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALCOPuzzleBase::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	// 기본 동작은 로그만 출력. 실제 로직은 자식 클래스에서 구현
	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s HandleInteraction: Source=%s, Interactor=%s"),
		*GetName(), *GetNameSafe(SourceComponent), *GetNameSafe(Interactor));
}

bool ALCOPuzzleBase::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 이미 해결된 퍼즐은 상호작용을 받지 않음
	return bIsSolved;
}

void ALCOPuzzleBase::SolvePuzzle()
{
	if (bIsSolved)
	{
		return;
	}

	bIsSolved = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleBase] %s Solved (PuzzleID: %s)"), *GetName(), *PuzzleID.ToString());

	OnPuzzleSolved();
	OnSolved.Broadcast(this);
}
