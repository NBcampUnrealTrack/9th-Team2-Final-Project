#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOStorySubsystem.generated.h"  

UCLASS()
class LASTCHECKOUT_API ULCOStorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// GameInstance 생성 시 엔진이 자동 호출 (생성 확인 로그)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ---------- New Game ----------
	// 새 게임 시작. 기존 Chapter / Objective 진행 상태를 초기화한 뒤 첫 챕터(Chapter1)를 시작
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void StartNewStory();
	
	// ---------- Chapter ----------

	// 챕터 시작. 현재 목표는 비워짐
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void StartChapter(FName ChapterID);

	// 현재 챕터와 일치할 때만 완료 처리. 성공 여부 반환
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	bool CompleteChapter(FName ChapterID);

	// ---------- Objective ----------

	// 현재 목표 설정 (챕터가 시작된 상태여야 함)
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	void StartObjective(FName ObjectiveID);

	// 현재 목표와 일치할 때만 완료 처리. 성공 여부 반환
	UFUNCTION(BlueprintCallable, Category = "LCO|Story")
	bool CompleteObjective(FName ObjectiveID);

	// ---------- 조회 ----------

	// 현재 챕터 ID (없으면 None)
	UFUNCTION(BlueprintPure, Category = "LCO|Story")
	FName GetCurrentChapterID() const { return CurrentChapterID; }

	// 현재 목표 ID (없으면 None)
	UFUNCTION(BlueprintPure, Category = "LCO|Story")
	FName GetCurrentObjectiveID() const { return CurrentObjectiveID; }

private:
	// 현재 진행 중인 챕터 ID
	UPROPERTY()
	FName CurrentChapterID = NAME_None;

	// 현재 진행 중인 목표 ID
	UPROPERTY()
	FName CurrentObjectiveID = NAME_None;
};