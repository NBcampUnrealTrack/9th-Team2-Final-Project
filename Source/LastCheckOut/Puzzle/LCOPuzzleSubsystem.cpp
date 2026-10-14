#include "Puzzle/LCOPuzzleSubsystem.h"

void ULCOPuzzleSubsystem::MarkPuzzleSolved(FName PuzzleID)
{
	if (PuzzleID.IsNone())
	{
		return;
	}

	SolvedPuzzleIDs.Add(PuzzleID);
}

void ULCOPuzzleSubsystem::RestoreSolvedPuzzles(const TArray<FName>& PuzzleIDs)
{
	SolvedPuzzleIDs = TSet<FName>(PuzzleIDs);

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleSubsystem] RestoreSolvedPuzzles: %d"), SolvedPuzzleIDs.Num());

	OnSolvedPuzzlesRestored.Broadcast();
}

void ULCOPuzzleSubsystem::ClearAll()
{
	SolvedPuzzleIDs.Empty();

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzleSubsystem] ClearAll"));
}
