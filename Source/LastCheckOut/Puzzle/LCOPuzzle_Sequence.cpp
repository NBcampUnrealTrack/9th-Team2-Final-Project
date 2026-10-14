#include "Puzzle/LCOPuzzle_Sequence.h"
#include "Interaction/LCOInteractComponent.h"

void ALCOPuzzle_Sequence::BeginPlay()
{
	Super::BeginPlay();

	if (CorrectSequence.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_Sequence] %s has no CorrectSequence"), *GetName());
	}
}

void ALCOPuzzle_Sequence::HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor)
{
	if (InputID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_Sequence] %s: %s has no InputID"), *GetName(), *GetNameSafe(SourceComponent));
		return;
	}

	if (CorrectSequence.IsEmpty())
	{
		return;
	}

	const int32 InputIndex = CurrentInput.Num();

	// 즉시 판정: 이번 순번의 정답과 다르면 바로 실패 (틀린 입력은 기록하지 않음)
	if (bFailImmediately && CorrectSequence[InputIndex] != InputID)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Sequence] %s Wrong input: %s (Expected index %d)"), *GetName(), *InputID.ToString(), InputIndex);
		FailPuzzle();
		return;
	}

	CurrentInput.Add(InputID);

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Sequence] %s Input: %s (%d / %d)"), *GetName(), *InputID.ToString(), CurrentInput.Num(), CorrectSequence.Num());
	OnSequenceInput(InputID, InputIndex);

	if (CurrentInput.Num() < CorrectSequence.Num())
	{
		return;
	}

	if (CurrentInput == CorrectSequence)
	{
		SolvePuzzle();
	}
	else
	{
		// 다 누른 뒤 판정에서 틀림. bResetOnFail이 꺼져 있어도 다음 시도를 위해 입력은 비움
		CurrentInput.Reset();
		FailPuzzle();
	}
}

void ALCOPuzzle_Sequence::OnPuzzleReset_Implementation()
{
	Super::OnPuzzleReset_Implementation();

	CurrentInput.Reset();
}
