#include "Puzzle/LCOPuzzle_AnswerInput.h"

void ALCOPuzzle_AnswerInput::BeginPlay()
{
	Super::BeginPlay();

	if (!bUIJudgesAnswer && CorrectAnswer.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_AnswerInput] %s has no CorrectAnswer"), *GetName());
	}
}

bool ALCOPuzzle_AnswerInput::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return Super::IsInteractionLocked_Implementation(SourceComponent, Interactor) || IsAnswerInputOpen();
}

void ALCOPuzzle_AnswerInput::HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor)
{
	if (IsAnswerInputOpen())
	{
		return;
	}

	InputUser = Interactor;

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_AnswerInput] %s AnswerInputRequested by %s"), *GetName(), *GetNameSafe(Interactor));

	OnAnswerInputRequested(Interactor);
	OnAnswerInputRequestedDelegate.Broadcast(this, Interactor);
}

bool ALCOPuzzle_AnswerInput::SubmitAnswer(const FString& Answer)
{
	if (!IsAnswerInputOpen() || IsSolved())
	{
		return false;
	}

	if (bUIJudgesAnswer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_AnswerInput] %s: SubmitAnswer is not used when UI judges the answer (use ReportSolved / ReportFailed)"), *GetName());
		return false;
	}

	if (!CheckAnswer(Answer))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_AnswerInput] %s Wrong answer"), *GetName());
		FailPuzzle();
		return false;
	}

	SolveFromInput();
	return true;
}

bool ALCOPuzzle_AnswerInput::ReportSolved()
{
	if (!IsAnswerInputOpen() || IsSolved())
	{
		return false;
	}

	if (!bUIJudgesAnswer)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_AnswerInput] %s: ReportSolved is only used when UI judges the answer (use SubmitAnswer)"), *GetName());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_AnswerInput] %s Solved by UI"), *GetName());
	SolveFromInput();
	return true;
}

void ALCOPuzzle_AnswerInput::ReportFailed()
{
	if (!IsAnswerInputOpen() || !bUIJudgesAnswer)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_AnswerInput] %s Failed by UI"), *GetName());
	FailPuzzle();
}

void ALCOPuzzle_AnswerInput::SolveFromInput()
{
	// 해결 연출이 입력창 뒤에 가려지지 않도록 먼저 닫음
	CloseAnswerInput(true);
	SolvePuzzle();
}

void ALCOPuzzle_AnswerInput::CancelAnswerInput()
{
	CloseAnswerInput(false);
}

bool ALCOPuzzle_AnswerInput::CheckAnswer_Implementation(const FString& Answer) const
{
	if (CorrectAnswer.IsEmpty())
	{
		return false;
	}

	const ESearchCase::Type SearchCase = bIgnoreCase ? ESearchCase::IgnoreCase : ESearchCase::CaseSensitive;
	return Answer.TrimStartAndEnd().Equals(CorrectAnswer.TrimStartAndEnd(), SearchCase);
}

void ALCOPuzzle_AnswerInput::CloseAnswerInput(bool bSolved)
{
	if (!IsAnswerInputOpen())
	{
		return;
	}

	InputUser.Reset();

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_AnswerInput] %s AnswerInputClosed (Solved: %d)"), *GetName(), bSolved);

	OnAnswerInputClosed(bSolved);
	OnAnswerInputClosedDelegate.Broadcast(this, bSolved);
}
