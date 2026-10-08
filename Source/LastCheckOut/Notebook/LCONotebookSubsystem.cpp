#include "Notebook/LCONotebookSubsystem.h"

#include "Engine/DataTable.h"

bool ULCONotebookSubsystem::AddEntry(const FLCONotebookEntry& Entry)
{
	if (Entry.EntryID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebook] EntryID가 비어 있어 추가하지 않음"));
		return false;
	}

	if (HasEntry(Entry.EntryID))
	{
		return false;
	}

	Entries.Add(Entry);
	UE_LOG(LogTemp, Log, TEXT("[LCONotebook] 항목 추가: %s"), *Entry.EntryID.ToString());

	OnNotebookEntryAdded.Broadcast(Entry);
	OnNotebookChanged.Broadcast();
	return true;
}

bool ULCONotebookSubsystem::AddEntryFromTable(UDataTable* Table, FName RowName)
{
	if (!Table || RowName.IsNone())
	{
		return false;
	}

	const FLCONotebookEntry* Row = Table->FindRow<FLCONotebookEntry>(RowName, TEXT("AddEntryFromTable"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebook] DataTable에 행이 없음: %s"), *RowName.ToString());
		return false;
	}

	FLCONotebookEntry Entry = *Row;
	if (Entry.EntryID.IsNone())
	{
		Entry.EntryID = RowName;
	}

	return AddEntry(Entry);
}

void ULCONotebookSubsystem::UpdateEntry(const FLCONotebookEntry& Entry)
{
	const int32 Index = FindEntryIndex(Entry.EntryID);
	if (Index == INDEX_NONE)
	{
		AddEntry(Entry);
		return;
	}

	Entries[Index] = Entry;
	OnNotebookChanged.Broadcast();
}

bool ULCONotebookSubsystem::RemoveEntry(FName EntryID)
{
	const int32 Index = FindEntryIndex(EntryID);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Entries.RemoveAt(Index);
	OnNotebookChanged.Broadcast();
	return true;
}

void ULCONotebookSubsystem::ClearAll()
{
	Entries.Empty();
	OnNotebookChanged.Broadcast();
}

bool ULCONotebookSubsystem::HasEntry(FName EntryID) const
{
	return FindEntryIndex(EntryID) != INDEX_NONE;
}

bool ULCONotebookSubsystem::FindEntry(FName EntryID, FLCONotebookEntry& OutEntry) const
{
	const int32 Index = FindEntryIndex(EntryID);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutEntry = Entries[Index];
	return true;
}

TArray<FLCONotebookEntry> ULCONotebookSubsystem::GetEntriesByCategory(ELCONotebookCategory Category) const
{
	TArray<FLCONotebookEntry> Result;
	for (const FLCONotebookEntry& Entry : Entries)
	{
		if (Entry.Category == Category)
		{
			Result.Add(Entry);
		}
	}
	return Result;
}

bool ULCONotebookSubsystem::HasNewEntries(ELCONotebookCategory Category) const
{
	for (const FLCONotebookEntry& Entry : Entries)
	{
		if (Entry.Category == Category && Entry.bIsNew)
		{
			return true;
		}
	}
	return false;
}

void ULCONotebookSubsystem::MarkEntryAsRead(FName EntryID)
{
	const int32 Index = FindEntryIndex(EntryID);
	if (Index == INDEX_NONE || !Entries[Index].bIsNew)
	{
		return;
	}

	Entries[Index].bIsNew = false;
	OnNotebookChanged.Broadcast();
}

int32 ULCONotebookSubsystem::FindEntryIndex(FName EntryID) const
{
	if (EntryID.IsNone())
	{
		return INDEX_NONE;
	}

	return Entries.IndexOfByPredicate([EntryID](const FLCONotebookEntry& Entry)
	{
		return Entry.EntryID == EntryID;
	});
}
