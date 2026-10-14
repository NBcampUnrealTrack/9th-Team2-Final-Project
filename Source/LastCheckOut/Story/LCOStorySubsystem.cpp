#include "LCOStorySubsystem.h"
#include "LastCheckOut.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Engine/DataTable.h"

namespace
{
	// ELCOStoryFlag::T02 → "T02"
	FString FlagToString(ELCOStoryFlag Flag)
	{
		return StaticEnum<ELCOStoryFlag>()->GetNameStringByValue(static_cast<int64>(Flag));
	}
}

void ULCOStorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 증언, 키워드 해금 조건표 준비
	BuildUnlockRules();

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] Initialize"));
}

void ULCOStorySubsystem::StartNewStory()
{
	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] StartNewStory: Reset progress (Prev Chapter: %s, Prev Objective: %s)"),
		*CurrentChapterID.ToString(), *CurrentObjectiveID.ToString());

	// 기존 진행 상태 초기화
	CurrentChapterID = NAME_None;
	CurrentObjectiveID = NAME_None;
	CompletedObjectives.Empty();
	StoryFlags.Empty();

	// 첫 챕터 시작
	// 첫 챕터 ID는 DataTable(DataManager)이 생기기 전까지 임시로 여기에서 지정
	const FName FirstChapterID(TEXT("Chapter1"));
	StartChapter(FirstChapterID);
}

void ULCOStorySubsystem::StartChapter(FName ChapterID)
{
	if (ChapterID.IsNone())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] StartChapter ignored: ChapterID is None"));
		return;
	}

	CurrentChapterID = ChapterID;
	CurrentObjectiveID = NAME_None;   // 새 챕터는 목표 없이 시작

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] StartChapter: %s"), *CurrentChapterID.ToString());
}

bool ULCOStorySubsystem::CompleteChapter(FName ChapterID)
{
	// 현재 챕터가 아니면 무시
	if (ChapterID.IsNone() || ChapterID != CurrentChapterID)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] CompleteChapter ignored: %s (Current: %s)"),
			*ChapterID.ToString(), *CurrentChapterID.ToString());
		return false;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] CompleteChapter: %s"), *ChapterID.ToString());

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
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] StartObjective ignored: %s (Chapter: %s)"),
			*ObjectiveID.ToString(), *CurrentChapterID.ToString());
		return;
	}

	CurrentObjectiveID = ObjectiveID;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] StartObjective: %s"), *CurrentObjectiveID.ToString());
}

bool ULCOStorySubsystem::CompleteObjective(FName ObjectiveID)
{
	// 현재 목표가 아니면 무시 (이미 지난 목표의 트리거 재호출 방지)
	if (ObjectiveID.IsNone() || ObjectiveID != CurrentObjectiveID)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] CompleteObjective ignored: %s (Current: %s)"),
			*ObjectiveID.ToString(), *CurrentObjectiveID.ToString());
		return false;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] CompleteObjective: %s"), *ObjectiveID.ToString());

	// 완료 목록에 기록 (중복 없이)
	CompletedObjectives.AddUnique(ObjectiveID);

	// 다음 목표는 호출하는 쪽에서 StartObjective로 지정
	CurrentObjectiveID = NAME_None;
	return true;
}

void ULCOStorySubsystem::RestoreStoryState(FName ChapterID, FName ObjectiveID, const TArray<FName>& InCompletedObjectives)
{
	CurrentChapterID = ChapterID;
	CurrentObjectiveID = ObjectiveID;
	CompletedObjectives = InCompletedObjectives;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] RestoreStoryState: Chapter = %s, Objective = %s, Completed = %d"),
		*CurrentChapterID.ToString(), *CurrentObjectiveID.ToString(), CompletedObjectives.Num());
}

// 증언, 키워드 해금 조건표 (기획 문서와 1:1)
// 증거물 ID는 수첩 기록 ID(= DT_ItemData / DT_NotebookData 행 이름)와 같아야 함
void ULCOStorySubsystem::BuildUnlockRules()
{
	using F = ELCOStoryFlag;
	UnlockRules.Empty();

	// 증언
	AddUnlockRule(F::T01, {}, {});                       // 기본 대화
	AddUnlockRule(F::T02, {}, { F::K01 });
	AddUnlockRule(F::T03, {}, {});                       // 기본 대화
	AddUnlockRule(F::T04, { TEXT("E01") }, {});
	AddUnlockRule(F::T05, { TEXT("E02") }, {});
	AddUnlockRule(F::T06, { TEXT("E01") }, {});
	AddUnlockRule(F::T07, { TEXT("E06") }, {});
	AddUnlockRule(F::T08, {}, {});                       // 기본 대화
	AddUnlockRule(F::T09, {}, { F::K02 });
	AddUnlockRule(F::T10, { TEXT("E03") }, {});
	AddUnlockRule(F::T11, {}, { F::K04 });
	AddUnlockRule(F::T12, {}, { F::K04 });
	AddUnlockRule(F::T13, {}, { F::K04 });

	// 조합 키워드
	AddUnlockRule(F::K01, { TEXT("E04"), TEXT("E08") }, {});
	AddUnlockRule(F::K02, { TEXT("E04"), TEXT("E05"), TEXT("E06") }, {});
	AddUnlockRule(F::K03, { TEXT("E03"), TEXT("E07") }, {});
	AddUnlockRule(F::K04, { TEXT("E01"), TEXT("E02") }, {});
	AddUnlockRule(F::K05, {}, { F::K01, F::T02 });
}

void ULCOStorySubsystem::AddUnlockRule(ELCOStoryFlag Flag, const TArray<FName>& RequiredEvidence, const TArray<ELCOStoryFlag>& RequiredFlags)
{
	FLCOUnlockRule Rule;
	Rule.RequiredEvidence = RequiredEvidence;
	Rule.RequiredFlags = RequiredFlags;
	UnlockRules.Add(Flag, Rule);
}

// 조건 검사, 획득
bool ULCOStorySubsystem::CanUnlock(ELCOStoryFlag Flag) const
{
	// 규칙이 없으면 조건 없음
	const FLCOUnlockRule* Rule = UnlockRules.Find(Flag);
	if (!Rule)
	{
		return true;
	}

	// 증거물: 수첩에 기록이 있는지 (증거물을 주우면 수첩에 자동 기록됨)
	if (Rule->RequiredEvidence.Num() > 0)
	{
		const ULCONotebookSubsystem* NotebookSubsystem = GetGameInstance()->GetSubsystem<ULCONotebookSubsystem>();
		if (!NotebookSubsystem)
		{
			return false;
		}

		for (const FName& EvidenceID : Rule->RequiredEvidence)
		{
			if (!NotebookSubsystem->HasEntry(EvidenceID))
			{
				return false;
			}
		}
	}

	// 플래그: 먼저 얻어야 하는 증언 / 키워드
	for (const ELCOStoryFlag RequiredFlag : Rule->RequiredFlags)
	{
		if (!HasFlag(RequiredFlag))
		{
			return false;
		}
	}

	return true;
}

bool ULCOStorySubsystem::TryUnlock(ELCOStoryFlag Flag)
{
	// 이미 획득 (대화 반복 등) → 조용히 무시
	if (HasFlag(Flag))
	{
		return false;
	}

	if (!CanUnlock(Flag))
	{
		LogMissingRequirements(Flag);
		return false;
	}

	return SetFlag(Flag);
}

bool ULCOStorySubsystem::SetFlag(ELCOStoryFlag Flag)
{
	if (StoryFlags.Contains(Flag))
	{
		return false;
	}

	StoryFlags.Add(Flag);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] Unlock: %s"), *FlagToString(Flag));

	RecordFlagToNotebook(Flag);
	OnStoryFlagUnlocked.Broadcast(Flag);
	return true;
}

bool ULCOStorySubsystem::TryUnlockByName(FName FlagName)
{
	const int64 Value = StaticEnum<ELCOStoryFlag>()->GetValueByNameString(FlagName.ToString());
	if (Value == INDEX_NONE)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] TryUnlockByName: unknown flag %s"), *FlagName.ToString());
		return false;
	}

	return TryUnlock(static_cast<ELCOStoryFlag>(Value));
}

FName ULCOStorySubsystem::GetFlagName(ELCOStoryFlag Flag)
{
	return FName(*FlagToString(Flag));
}

bool ULCOStorySubsystem::IsDeductionFlag(ELCOStoryFlag Flag)
{
	return FlagToString(Flag).StartsWith(TEXT("K"));
}

ELCODeductionResult ULCOStorySubsystem::TryDeduce(const TArray<FName>& SelectedIDs, ELCOStoryFlag& OutFlag)
{
	// 중복 / 빈 칸 제거
	TSet<FName> Selected;
	for (const FName& ID : SelectedIDs)
	{
		if (!ID.IsNone())
		{
			Selected.Add(ID);
		}
	}

	if (Selected.Num() < 2)
	{
		return ELCODeductionResult::NotEnough;
	}

	for (const TPair<ELCOStoryFlag, FLCOUnlockRule>& Pair : UnlockRules)
	{
		if (!IsDeductionFlag(Pair.Key))
		{
			continue;
		}

		// 조건 전체 (증거물 ID + 플래그 이름)
		TSet<FName> Required;
		for (const FName& EvidenceID : Pair.Value.RequiredEvidence)
		{
			Required.Add(EvidenceID);
		}
		for (const ELCOStoryFlag RequiredFlag : Pair.Value.RequiredFlags)
		{
			Required.Add(GetFlagName(RequiredFlag));
		}

		// 정확히 일치해야 함 (아무거나 많이 넣어서 맞히는 것 방지)
		if (Required.Num() != Selected.Num() || !Required.Includes(Selected))
		{
			continue;
		}

		OutFlag = Pair.Key;

		if (HasFlag(Pair.Key))
		{
			return ELCODeductionResult::AlreadyKnown;
		}

		// 선택한 항목은 모두 수첩에 있는 것이라 보통 성공. 실패하면 조건 데이터 불일치
		return TryUnlock(Pair.Key) ? ELCODeductionResult::Success : ELCODeductionResult::NoMatch;
	}

	return ELCODeductionResult::NoMatch;
}

void ULCOStorySubsystem::RecordFlagToNotebook(ELCOStoryFlag Flag) const
{
	ULCONotebookSubsystem* NotebookSubsystem = GetGameInstance()->GetSubsystem<ULCONotebookSubsystem>();
	if (!NotebookSubsystem)
	{
		return;
	}

	const UDataTable* Table = StoryNotebookTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] StoryNotebookTable 미설정 - 수첩 기록 안 됨 (DefaultGame.ini 확인)"));
		return;
	}

	const FName FlagName = GetFlagName(Flag);
	const bool bIsDeduction = IsDeductionFlag(Flag);

	// 증언 / 추리 독백
	if (const FLCONotebookEntry* Row = Table->FindRow<FLCONotebookEntry>(FlagName, TEXT(""), false))
	{
		FLCONotebookEntry Entry = *Row;
		Entry.EntryID = FlagName;   // 추리 조합 시 이 ID로 찾으므로 행 이름과 반드시 같게
		Entry.Category = bIsDeduction ? ELCONotebookCategory::Deduction : ELCONotebookCategory::Testimony;
		Entry.bIsNew = true;
		NotebookSubsystem->AddEntry(Entry);
	}
	else
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] StoryNotebookTable에 %s 행 없음"), *FlagName.ToString());
	}

	// 추리 후 새 목표 (선택)
	if (bIsDeduction)
	{
		const FName ObjectiveRowName(*(FlagName.ToString() + TEXT("_OBJ")));
		if (const FLCONotebookEntry* ObjectiveRow = Table->FindRow<FLCONotebookEntry>(ObjectiveRowName, TEXT(""), false))
		{
			FLCONotebookEntry Objective = *ObjectiveRow;
			Objective.EntryID = ObjectiveRowName;
			Objective.Category = ELCONotebookCategory::Objective;
			Objective.bIsNew = true;
			NotebookSubsystem->AddEntry(Objective);
		}
	}
}

TArray<ELCOStoryFlag> ULCOStorySubsystem::GetUnlockableFlags() const
{
	TArray<ELCOStoryFlag> Result;
	for (const TPair<ELCOStoryFlag, FLCOUnlockRule>& Pair : UnlockRules)
	{
		if (!HasFlag(Pair.Key) && CanUnlock(Pair.Key))
		{
			Result.Add(Pair.Key);
		}
	}

	// enum 순서(T01 → K05)로 정렬
	Result.Sort([](ELCOStoryFlag A, ELCOStoryFlag B) { return static_cast<uint8>(A) < static_cast<uint8>(B); });
	return Result;
}

void ULCOStorySubsystem::LogMissingRequirements(ELCOStoryFlag Flag) const
{
	const FLCOUnlockRule* Rule = UnlockRules.Find(Flag);
	if (!Rule)
	{
		return;
	}

	TArray<FString> Missing;
	const ULCONotebookSubsystem* NotebookSubsystem = GetGameInstance()->GetSubsystem<ULCONotebookSubsystem>();

	for (const FName& EvidenceID : Rule->RequiredEvidence)
	{
		if (!NotebookSubsystem || !NotebookSubsystem->HasEntry(EvidenceID))
		{
			Missing.Add(EvidenceID.ToString());
		}
	}

	for (const ELCOStoryFlag RequiredFlag : Rule->RequiredFlags)
	{
		if (!HasFlag(RequiredFlag))
		{
			Missing.Add(FlagToString(RequiredFlag));
		}
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] TryUnlock %s failed: missing %s"),
		*FlagToString(Flag), *FString::Join(Missing, TEXT(", ")));
}

// 세이브
TArray<FName> ULCOStorySubsystem::GetStoryFlagNames() const
{
	TArray<FName> FlagNames;
	for (const ELCOStoryFlag Flag : StoryFlags)
	{
		FlagNames.Add(FName(*FlagToString(Flag)));
	}
	return FlagNames;
}

void ULCOStorySubsystem::RestoreStoryFlags(const TArray<FName>& FlagNames)
{
	StoryFlags.Empty();
	const UEnum* FlagEnum = StaticEnum<ELCOStoryFlag>();

	for (const FName& FlagName : FlagNames)
	{
		// "T02" → enum 값. 없는 이름이면 INDEX_NONE (삭제된 플래그 등)
		const int64 Value = FlagEnum->GetValueByNameString(FlagName.ToString());
		if (Value == INDEX_NONE)
		{
			UE_LOG(LogLCOCore, Warning, TEXT("[LCOStorySubsystem] RestoreStoryFlags: unknown flag %s"), *FlagName.ToString());
			continue;
		}
		StoryFlags.Add(static_cast<ELCOStoryFlag>(Value));
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOStorySubsystem] RestoreStoryFlags: %d"), StoryFlags.Num());
}
