#include "LCOStorySubsystem.h"

void ULCOStorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Log Test
	UE_LOG(LogTemp, Log, TEXT("[LCOStorySubsystem] Initialize"));
}

void ULCOStorySubsystem::StartNewStory()
{
	// Log Test
	UE_LOG(LogTemp, Log, TEXT("[LCOStorySubsystem] StartNewStory: Reset progress (Prev Chapter: %s, Prev Objective: %s)"),
		*CurrentChapterID.ToString(), *CurrentObjectiveID.ToString());

	// 기존 진행 상태 초기화
	CurrentChapterID = NAME_None;
	CurrentObjectiveID = NAME_None;

	// 첫 챕터 시작
	// 첫 챕터 ID는 DataTable(DataManager)이 생기기 전까지 임시로 여기에서 지정
	const FName FirstChapterID(TEXT("Chapter1"));
	StartChapter(FirstChapterID);
}

void ULCOStorySubsystem::StartChapter(FName ChapterID)
{
	if (ChapterID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOStorySubsystem] StartChapter ignored: ChapterID is None"));
		return;
	}

	CurrentChapterID = ChapterID;
	CurrentObjectiveID = NAME_None;   // 새 챕터는 목표 없이 시작

	// Log Test
	UE_LOG(LogTemp, Log, TEXT("[LCOStorySubsystem] StartChapter: %s"), *CurrentChapterID.ToString());
}

bool ULCOStorySubsystem::CompleteChapter(FName ChapterID)
{
	// 현재 챕터가 아니면 무시
	if (ChapterID.IsNone() || ChapterID != CurrentChapterID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOStorySubsystem] CompleteChapter ignored: %s (Current: %s)"),
			*ChapterID.ToString(), *CurrentChapterID.ToString());
		return false;
	}

	// Log Test
	UE_LOG(LogTemp, Log, TEXT("[LCOStorySubsystem] CompleteChapter: %s"), *ChapterID.ToString());

	// 다음 챕터는 호출하는 쪽에서 StartChapter로 지정
	CurrentChapterID = NAME_None;
	CurrentObjectiveID = NAME_None;
	return true;
}

void ULCOStorySubsystem::StartObjective(FName ObjectiveID)
{
	// 챕터 없이 목표를 시작하려는 경우 → 호출 순서 실수
	if (CurrentChapterID.IsNone() || ObjectiveID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOStorySubsystem] StartObjective ignored: %s (Chapter: %s)"),
			*ObjectiveID.ToString(), *CurrentChapterID.ToString());
		return;
	}

	CurrentObjectiveID = ObjectiveID;

	// Log Test
	UE_LOG(LogTemp, Log, TEXT("[LCOStorySubsystem] StartObjective: %s"), *CurrentObjectiveID.ToString());
}

bool ULCOStorySubsystem::CompleteObjective(FName ObjectiveID)
{
	// 현재 목표가 아니면 무시 (이미 지난 목표의 트리거 재호출 방지)
	if (ObjectiveID.IsNone() || ObjectiveID != CurrentObjectiveID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOStorySubsystem] CompleteObjective ignored: %s (Current: %s)"),
			*ObjectiveID.ToString(), *CurrentObjectiveID.ToString());
		return false;
	}

	// Log Test
	UE_LOG(LogTemp, Log, TEXT("[LCOStorySubsystem] CompleteObjective: %s"), *ObjectiveID.ToString());

	// 다음 목표는 호출하는 쪽에서 StartObjective로 지정
	CurrentObjectiveID = NAME_None;
	return true;
}