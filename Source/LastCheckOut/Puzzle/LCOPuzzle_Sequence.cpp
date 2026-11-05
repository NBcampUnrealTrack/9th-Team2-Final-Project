#include "Puzzle/LCOPuzzle_Sequence.h"
#include "Interaction/LCOInteractComponent.h"

void ALCOPuzzle_Sequence::BeginPlay()
{
	Super::BeginPlay();

	if (CorrectSequence.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_Sequence] %s has no CorrectSequence"), *GetName());
	}

	if ((!ClearInputID.IsNone() && CorrectSequence.Contains(ClearInputID))
		|| (bRequireConfirm && !ConfirmInputID.IsNone() && CorrectSequence.Contains(ConfirmInputID)))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_Sequence] %s: CorrectSequence contains the Clear / Confirm InputID (that button never counts as input)"), *GetName());
	}

	if (bRequireConfirm && ConfirmInputID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_Sequence] %s: bRequireConfirm is set but ConfirmInputID is None (puzzle can never be checked)"), *GetName());
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

	// 지우기 버튼
	if (!ClearInputID.IsNone() && InputID == ClearInputID)
	{
		CurrentInput.Reset();

		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Sequence] %s Cleared"), *GetName());
		OnSequenceCleared();
		return;
	}

	// 확인 버튼 (확인 방식에서만)
	if (bRequireConfirm && !ConfirmInputID.IsNone() && InputID == ConfirmInputID)
	{
		if (CurrentInput.IsEmpty())
		{
			OnSequenceInputRejected(InputID);
			return;
		}

		CheckAnswer();
		return;
	}

	const int32 InputIndex = CurrentInput.Num();

	// 확인 방식: 정답 길이까지만 입력 가능
	if (bRequireConfirm && InputIndex >= CorrectSequence.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Sequence] %s Input ignored (full): %s"), *GetName(), *InputID.ToString());
		OnSequenceInputRejected(InputID);
		return;
	}

	// 즉시 판정: 이번 순번의 정답과 다르면 바로 실패 (틀린 입력은 기록하지 않음)
	if (!bRequireConfirm && bFailImmediately && CorrectSequence[InputIndex] != InputID)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Sequence] %s Wrong input: %s (Expected index %d)"), *GetName(), *InputID.ToString(), InputIndex);
		FailPuzzle();
		return;
	}

	CurrentInput.Add(InputID);

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_Sequence] %s Input: %s (%d / %d)"), *GetName(), *InputID.ToString(), CurrentInput.Num(), CorrectSequence.Num());
	OnSequenceInput(InputID, InputIndex);

	// 확인 방식은 확인 버튼을 기다림
	if (bRequireConfirm || CurrentInput.Num() < CorrectSequence.Num())
	{
		return;
	}

	CheckAnswer();
}

void ALCOPuzzle_Sequence::CheckAnswer()
{
	if (CurrentInput == CorrectSequence)
	{
		SolvePuzzle();
	}
	else
	{
		// 틀림. bResetOnFail이 꺼져 있어도 다음 시도를 위해 입력은 비움
		CurrentInput.Reset();
		FailPuzzle();
	}
}

void ALCOPuzzle_Sequence::OnPuzzleReset_Implementation()
{
	Super::OnPuzzleReset_Implementation();

	CurrentInput.Reset();
}
