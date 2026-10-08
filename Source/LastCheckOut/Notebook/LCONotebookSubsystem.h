#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Notebook/LCONotebookTypes.h"
#include "LCONotebookSubsystem.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCONotebookEntryAdded, const FLCONotebookEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCONotebookChanged);

/**
 * 수첩 기록 저장소 (GameInstance 단위라 레벨 이동해도 유지)
 *
 * [데이터 담당 사용법]
 * - 단서 획득, 증언 확보, 사진 촬영, 목표 갱신 시 AddEntry (또는 AddEntryFromTable) 호출
 * - 새 게임 시작 시 ClearAll 호출
 * - UI는 OnNotebookChanged를 받아 갱신하므로 UI 쪽은 신경 쓰지 않아도 됨
 */
UCLASS()
class LASTCHECKOUT_API ULCONotebookSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---------- 등록 / 삭제 ----------

	// 항목 추가. EntryID가 비었거나 이미 있으면 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	bool AddEntry(const FLCONotebookEntry& Entry);

	// DataTable 행으로 추가 (EntryID가 비어 있으면 RowName 사용)
	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	bool AddEntryFromTable(UDataTable* Table, FName RowName);

	// 이미 있는 항목 내용 교체 (목표 문구 갱신 등). 없으면 추가
	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void UpdateEntry(const FLCONotebookEntry& Entry);

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	bool RemoveEntry(FName EntryID);

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void ClearAll();

	// ---------- 조회 ----------

	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	bool HasEntry(FName EntryID) const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	bool FindEntry(FName EntryID, FLCONotebookEntry& OutEntry) const;

	// 해당 분류 항목 (획득 순서)
	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	TArray<FLCONotebookEntry> GetEntriesByCategory(ELCONotebookCategory Category) const;

	// 해당 분류에 안 읽은 항목이 있는지 (탭 알림 표시용)
	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	bool HasNewEntries(ELCONotebookCategory Category) const;

	// ---------- 읽음 처리 ----------

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void MarkEntryAsRead(FName EntryID);

	// ---------- 델리게이트 ----------

	// 새 항목 추가 시 (획득 알림 팝업 등에 사용)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Notebook")
	FOnLCONotebookEntryAdded OnNotebookEntryAdded;

	// 추가/수정/삭제/읽음 등 내용이 바뀔 때마다
	UPROPERTY(BlueprintAssignable, Category = "LCO|Notebook")
	FOnLCONotebookChanged OnNotebookChanged;

private:
	int32 FindEntryIndex(FName EntryID) const;

	UPROPERTY()
	TArray<FLCONotebookEntry> Entries;
};
