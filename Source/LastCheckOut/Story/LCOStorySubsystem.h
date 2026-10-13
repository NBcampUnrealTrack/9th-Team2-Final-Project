#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Story/LCOStoryTypes.h"
#include "LCOStorySubsystem.generated.h"  

UCLASS()
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

	// Save

	// 세이브 파일에서 읽은 진행 상태를 그대로 적용 (LCOSaveManager::ApplyLoadedData에서 호출)
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void RestoreStoryState(FName ChapterID, FName ObjectiveID, const TArray<FName>& InCompletedObjectives);

	// 획득한 플래그를 이름 목록으로 반환 (세이브 저장용, 예: "T02", "K01")
	TArray<FName> GetStoryFlagNames() const;

	// 이름 목록으로 플래그 복원 (세이브 불러오기용). 모르는 이름은 무시
	void RestoreStoryFlags(const TArray<FName>& FlagNames);

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

	// 획득한 증언 / 키워드 플래그 (새 게임 시 초기화)
	UPROPERTY()
	TSet<ELCOStoryFlag> StoryFlags;

	// 플래그별 해금 조건 (코드에서 고정, 세이브 대상 아님)
	UPROPERTY()
	TMap<ELCOStoryFlag, FLCOUnlockRule> UnlockRules;
};