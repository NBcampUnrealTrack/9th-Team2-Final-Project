#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Story/LCOStoryTypes.h"
#include "LCOStorySubsystem.generated.h"  

class UDataTable;

// 증언 / 키워드 획득 시 (알림 팝업 등에 사용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOStoryFlagUnlocked, ELCOStoryFlag, Flag);

// 지배인 보고 결과 (GameMode: 2막 이동·자동 저장 / 대화: 독백 / UI: 엔딩 화면 / 레벨: 407호 문 열기 등)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOReportResolved, ELCOStoryFlag, PresentedKeyword, ELCOReportResult, Result);

// 상태 플래그가 켜지거나 꺼질 때 (예: C2_NotebookConfiscated)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOStateFlagChanged, FName, FlagName, bool, bIsSet);

// 챕터 클리어로 다음 챕터 시작 (GameMode: 시간대 진행 · 시작 맵 이동 · 자동 저장)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOChapterAdvanced, FName, CompletedChapterID, FName, NewChapterID);

/**
 * 스토리 진행 (챕터, 목표, 증언·키워드 플래그)
 *
 * [수첩 자동 기록]
 * 플래그를 획득하면 StoryNotebookTable(DT_StoryNotebook, 행 구조 = FLCONotebookEntry)에서
 * 같은 이름의 행을 찾아 수첩에 기록함
 *   - "T01" 행 → 증언 탭,  "K01" 행 → 추리 탭 (Description = 독백)
 *   - "K01_OBJ" 행이 있으면 목표 탭에도 추가 (막연한 새 목표 문구)
 * 설정: Config/DefaultGame.ini
 *   [/Script/LastCheckOut.LCOStorySubsystem]
 *   StoryNotebookTable=/Game/.../DT_StoryNotebook.DT_StoryNotebook
 */
UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOStorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// GameInstance 생성 시 엔진이 자동 호출 (생성 확인 로그)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// New Game
	// 새 게임 시작. 기존 Chapter / Objective 진행 상태를 초기화한 뒤 첫 챕터(Chapter1)를 시작
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void StartNewStory();
	
	// Chapter

	// 챕터 시작. 현재 목표는 비워짐
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void StartChapter(FName ChapterID);

	// 현재 챕터와 일치할 때만 완료 처리. 성공 여부 반환
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	bool CompleteChapter(FName ChapterID);

	// Objective

	// 현재 목표 설정 (챕터가 시작된 상태여야 함)
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void StartObjective(FName ObjectiveID);

	// 현재 목표와 일치할 때만 완료 처리. 성공 여부 반환
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	bool CompleteObjective(FName ObjectiveID);

	// 증언, 키워드 플래그

	// 해금 조건을 검사해서 만족하면 획득. 획득하면 true (이미 있거나 조건 미달이면 false)
	// 획득은 이 함수로 (조건 검사 포함)
	UFUNCTION(BlueprintCallable, Category = "LCO|Story|Flag")
	bool TryUnlock(ELCOStoryFlag Flag);

	// 해금 조건이 다 갖춰졌는지만 확인 (상태 변경 없음)
	UFUNCTION(BlueprintPure, Category = "LCO|Story|Flag")
	bool CanUnlock(ELCOStoryFlag Flag) const;

	// 이미 획득했는지
	UFUNCTION(BlueprintPure, Category = "LCO|Story|Flag")
	bool HasFlag(ELCOStoryFlag Flag) const { return StoryFlags.Contains(Flag); }

	// 지금 해금 가능하지만 아직 획득하지 않은 플래그 목록 (조합 힌트, 디버그용)
	UFUNCTION(BlueprintPure, Category = "LCO|Story|Flag")
	TArray<ELCOStoryFlag> GetUnlockableFlags() const;

	// 조건 검사 없이 강제 획득 (테스트 / 특수 연출 전용)
	UFUNCTION(BlueprintCallable, Category = "LCO|Story|Flag")
	bool SetFlag(ELCOStoryFlag Flag);

	// 이름으로 TryUnlock ("T04" 등). 대화 CSV의 UnlockFlag 열에서 사용. 모르는 이름이면 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Story|Flag")
	bool TryUnlockByName(FName FlagName);

	// 플래그 획득 시
	UPROPERTY(BlueprintAssignable, Category = "LCO|Story|Flag")
	FOnLCOStoryFlagUnlocked OnStoryFlagUnlocked;

	// 추리 (수첩 추리 탭)

	// 선택한 수첩 항목 ID들(E01, T02, K01 ...)이 어떤 키워드(K)의 조건과 정확히 일치하면 획득
	// OutFlag: 일치한 키워드 (Success / AlreadyKnown일 때만 유효)
	UFUNCTION(BlueprintCallable, Category = "LCO|Story|Deduction")
	ELCODeductionResult TryDeduce(const TArray<FName>& SelectedIDs, ELCOStoryFlag& OutFlag);

	// 추리로 얻는 키워드인지 (K로 시작)
	UFUNCTION(BlueprintPure, Category = "LCO|Story|Deduction")
	static bool IsDeductionFlag(ELCOStoryFlag Flag);

	// 플래그 이름 ("T02")
	UFUNCTION(BlueprintPure, Category = "LCO|Story|Flag")
	static FName GetFlagName(ELCOStoryFlag Flag);

	// 1막 결말 (지배인 보고)

	// 지배인 대화 선택지 표시용: 이 추리(K05 / K07)를 지금 제시할 수 있는지
	// K05, K07을 둘 다 가지고 있으면 두 선택지 모두 표시
	UFUNCTION(BlueprintPure, Category = "LCO|Story|Ending")
	bool CanReportToManager(ELCOStoryFlag PresentedKeyword) const;

	// 지배인에게 추리 제시 / 보고. 결과 판정 + 스토리 상태 변경 후 OnReportResolved 알림
	// K05 → 1막 클리어 (Chapter1 완료, Chapter2 시작)
	// K07 → END 3 / K03이 있으면 멈춤 (몇 번 골라도 동일). 둘 다 진행 상태는 바꾸지 않음
	UFUNCTION(BlueprintCallable, Category = "LCO|Story|Ending")
	ELCOReportResult ReportToManager(ELCOStoryFlag PresentedKeyword);

	// 보고 결과 알림
	UPROPERTY(BlueprintAssignable, Category = "LCO|Story|Ending")
	FOnLCOReportResolved OnReportResolved;

	// 2막 결말 (별관 출구)

	// 2막 진행 중이고 K11(구관의 존재)을 가지고 있는지 (출구 선택지 표시용)
	UFUNCTION(BlueprintPure, Category = "LCO|Story|Chapter")
	bool CanCompleteChapter2() const;

	// 2막 클리어 → 3막 시작 후 OnChapterAdvanced 알림. 조건 미달이면 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Story|Chapter")
	bool TryCompleteChapter2();

	// 챕터 클리어 알림 (지배인 보고로 넘어가는 1막 클리어는 OnReportResolved로 알림)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Story|Chapter")
	FOnLCOChapterAdvanced OnChapterAdvanced;

	// Save

	// 세이브 파일에서 읽은 진행 상태를 그대로 적용 (LCOSaveManager::ApplyLoadedData에서 호출)
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void RestoreStoryState(FName ChapterID, FName ObjectiveID, const TArray<FName>& InCompletedObjectives);

	// 획득한 플래그를 이름 목록으로 반환 (세이브 저장용, 예: "T02", "K01")
	TArray<FName> GetStoryFlagNames() const;

	// 이름 목록으로 플래그 복원 (세이브 불러오기용). 모르는 이름은 무시
	void RestoreStoryFlags(const TArray<FName>& FlagNames);

	// 상태 플래그 (켜고 끌 수 있는 진행 상태, 세이브에 저장. 예: C2_NotebookConfiscated = 수첩 압수 중)

	UFUNCTION(BlueprintCallable, Category = "LCO|Story|State")
	void SetStateFlag(FName FlagName);

	UFUNCTION(BlueprintCallable, Category = "LCO|Story|State")
	void ClearStateFlag(FName FlagName);

	UFUNCTION(BlueprintPure, Category = "LCO|Story|State")
	bool HasStateFlag(FName FlagName) const { return StateFlags.Contains(FlagName); }

	UPROPERTY(BlueprintAssignable, Category = "LCO|Story|State")
	FOnLCOStateFlagChanged OnStateFlagChanged;

	// 세이브 저장 / 불러오기용
	TArray<FName> GetStateFlagNames() const { return StateFlags.Array(); }
	void RestoreStateFlags(const TArray<FName>& FlagNames);

	// 조회

	// 현재 챕터 ID (없으면 None)
	UFUNCTION(BlueprintPure, Category = "LCO|Story")
	FName GetCurrentChapterID() const { return CurrentChapterID; }

	// 현재 목표 ID (없으면 None)
	UFUNCTION(BlueprintPure, Category = "LCO|Story")
	FName GetCurrentObjectiveID() const { return CurrentObjectiveID; }

	// 해당 목표를 완료한 적이 있는지 (수첩 UI의 완료 표시 등에 사용)
	UFUNCTION(BlueprintPure, Category = "LCO|Story")
	bool IsObjectiveCompleted(FName ObjectiveID) const { return CompletedObjectives.Contains(ObjectiveID); }

	// 완료한 목표 전체 (완료 순서)
	UFUNCTION(BlueprintPure, Category = "LCO|Story")
	const TArray<FName>& GetCompletedObjectives() const { return CompletedObjectives; }

	// 지금 진행 중인 목표가 이 목표인지 (예: C2_FollowCaretaker = 2막 관리인 따라가는 중)
	UFUNCTION(BlueprintPure, Category = "LCO|Story")
	bool IsCurrentObjective(FName ObjectiveID) const { return !ObjectiveID.IsNone() && CurrentObjectiveID == ObjectiveID; }

private:
	// 현재 진행 중인 챕터 ID
	UPROPERTY()
	FName CurrentChapterID = NAME_None;

	// 현재 진행 중인 목표 ID
	UPROPERTY()
	FName CurrentObjectiveID = NAME_None;

	// 완료한 목표 ID 목록 (새 게임 시 초기화)
	UPROPERTY()
	TArray<FName> CompletedObjectives;

	// 증언, 키워드 플래그

	// 해금 조건표 채우기 (Initialize에서 1회 호출)
	void BuildUnlockRules();

	// 조건표에 규칙 1개 추가
	void AddUnlockRule(ELCOStoryFlag Flag, const TArray<FName>& RequiredEvidence, const TArray<ELCOStoryFlag>& RequiredFlags);

	// 부족한 조건을 로그로 출력 (TryUnlock 실패 시 원인 확인용)
	void LogMissingRequirements(ELCOStoryFlag Flag) const;

	// 획득한 플래그를 수첩에 기록 (StoryNotebookTable 행 기준)
	void RecordFlagToNotebook(ELCOStoryFlag Flag) const;

	// 증언 / 추리 독백 / 추리 후 새 목표 문구 (행 구조 = FLCONotebookEntry)
	UPROPERTY(Config)
	TSoftObjectPtr<UDataTable> StoryNotebookTable;

	// 획득한 증언 / 키워드 플래그 (새 게임 시 초기화)
	UPROPERTY()
	TSet<ELCOStoryFlag> StoryFlags;

	// 켜져 있는 상태 플래그 (새 게임 시 초기화)
	UPROPERTY()
	TSet<FName> StateFlags;

	// 플래그별 해금 조건 (코드에서 고정, 세이브 대상 아님)
	UPROPERTY()
	TMap<ELCOStoryFlag, FLCOUnlockRule> UnlockRules;
};