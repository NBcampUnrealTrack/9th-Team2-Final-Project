#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "Interaction/LCOInteractionTypes.h"
#include "Save/LCOSaveableActor.h"
#include "LCOPuzzleBase.generated.h"

class ALCOOpenableBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleSolved, ALCOPuzzleBase*, Puzzle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleFailed, ALCOPuzzleBase*, Puzzle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleActivated, ALCOPuzzleBase*, Puzzle);

// 모든 퍼즐의 부모 클래스
// 상호작용 → 해결 / 비활성 검사 → HandlePuzzleInput(자식 구현) → 자식이 정답이면 SolvePuzzle(), 틀리면 FailPuzzle() 호출
// 공통 처리: 활성화 조건(선행 퍼즐), 실패 / 리셋, 해결 결과(잠금 해제 대상, 스토리 결과), 저장 복원
// 저장: 해결 여부는 ULCOPuzzleSubsystem(퍼즐 ID 기준), 활성 상태는 LCOWorldStateSubsystem(ILCOSaveableActor)
//       → Activate() / Deactivate()로 바꾼 활성 상태도 맵 이동 / 이어하기 후 유지됨
UCLASS(Abstract, Blueprintable)
class LASTCHECKOUT_API ALCOPuzzleBase : public AActor, public ILCOInteractionReceiver, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOPuzzleBase();

	// ILCOInteractionReceiver
	// 공통 검사 후 HandlePuzzleInput으로 전달. 자식은 이 함수 대신 HandlePuzzleInput을 구현할 것
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override final;

	// 해결됐거나 비활성 상태면 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 비활성 상태면 InactivePrompt, 그 외에는 컴포넌트 기본 문구
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 활성 상태 저장 (FLCOActorSaveState::bIsEnabled 사용). 해결 여부는 ULCOPuzzleSubsystem이 따로 저장
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;

	// 활성 상태 복원. 활성화 연출 없이 OnActiveStateRestored로 모습만 맞춤
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	// ---------- 상태 변경 ----------

	// 퍼즐 해결. 정답일 때 자식 클래스에서 호출 (해결 기록 → 결과 적용 → 이벤트)
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SolvePuzzle();

	// 오답 처리. 틀렸을 때 자식 클래스에서 호출 (bResetOnFail이면 자동 리셋)
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void FailPuzzle();

	// 입력 상태 초기화 (해결된 퍼즐은 무시). 실제 초기화는 OnPuzzleReset에서 구현
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void ResetPuzzle();

	// 퍼즐 활성화 (전원 연결, 스토리 이벤트 등에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void Activate();

	// 퍼즐 비활성화 (상호작용 불가)
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void Deactivate();

	// ---------- 조회 ----------

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsSolved() const { return bIsSolved; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsActive() const { return bIsActive; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	FName GetPuzzleID() const { return PuzzleID; }

	// 리셋 이후 틀린 횟수 (힌트 표시 등)
	UFUNCTION(BlueprintPure, Category = "Puzzle")
	int32 GetFailCount() const { return FailCount; }

	// 선행 퍼즐이 모두 해결됐는지 (선행 퍼즐이 없으면 true)
	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool AreRequiredPuzzlesSolved() const;

	// ---------- 델리게이트 ----------

	// 퍼즐 해결 시 (세이브 복원 시에는 호출되지 않음)
	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FOnPuzzleSolved OnSolved;

	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FOnPuzzleFailed OnFailed;

	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FOnPuzzleActivated OnActivated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- 자식 클래스 구현 ----------

	// 퍼즐 입력 처리. InputID는 입력한 LCOInteract 컴포넌트의 InputID (버튼 구분용)
	// 해결 / 비활성 검사가 끝난 뒤에만 호출됨
	UFUNCTION(BlueprintNativeEvent, Category = "Puzzle")
	void HandlePuzzleInput(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor);

	// 입력 상태 초기화 (입력한 순서 비우기, 버튼 불 끄기 등). C++ 자식은 Super 호출 후 구현
	UFUNCTION(BlueprintNativeEvent, Category = "Puzzle")
	void OnPuzzleReset();

	// 세이브에서 해결 상태로 복원될 때 (OnPuzzleRestored BP 이벤트 직전). C++ 자식이 해결된 모습의 상태값을 맞출 때 사용
	virtual void HandleRestoredAsSolved() {}

	// ---------- BP 연출 이벤트 ----------

	// 해결 연출 (효과음, 불빛 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleSolved();

	// 오답 연출 (부저, 화면 흔들림 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleFailed();

	// 활성화 연출 (전원 켜짐 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleActivated();

	// 세이브에서 해결 상태로 복원됐을 때. 연출 없이 해결된 모습으로 바꾼다
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleRestored();

	// 플레이어가 이 퍼즐의 부품에 상호작용(F키)할 때마다 호출. InputID와 상관없이 항상, HandlePuzzleInput보다 먼저
	// (해결됐거나 비활성 상태라 상호작용이 잠겨 있을 때는 호출되지 않음)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleInteracted(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor);

	// 맵 이동 / 이어하기로 활성 상태가 복원됐을 때. 연출 없이 켜진 / 꺼진 모습으로 바꾼다
	// (OnPuzzleActivated는 실제로 켜지는 순간에만 호출되므로, 복원 시 모습은 여기서 맞출 것)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnActiveStateRestored(bool bActive);

	// ---------- 설정값 ----------

	// 퍼즐 고유 ID (저장용). 레벨 전체에서 겹치지 않게 지정 (예: "P_Lobby_Sequence")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FName PuzzleID;

	// 시작 시 활성 상태인지. false면 Activate() 호출 전까지 상호작용 불가
	// RequiredPuzzles가 있으면 무시하고, 선행 퍼즐이 모두 해결될 때 자동 활성화
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	bool bStartActive = true;

	// 선행 퍼즐. 모두 해결되면 자동으로 활성화
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle")
	TArray<TObjectPtr<ALCOPuzzleBase>> RequiredPuzzles;

	// 틀렸을 때 자동으로 ResetPuzzle() 호출
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	bool bResetOnFail = true;

	// ---------- 해결 결과 ----------

	// 해결 시 잠금 해제할 대상 (문, 금고, 서랍 등). 복원 시에는 대상의 상태를 LCOWorldStateSubsystem이 따로 복원
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle|Result")
	TArray<TObjectPtr<ALCOOpenableBase>> UnlockTargets;

	// 잠금 해제 후 바로 열기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Result")
	bool bOpenTargetsOnSolve = false;

	// 해결 시 스토리 결과 (목표 완료, 플래그 획득). 세이브 복원 시에는 적용하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Result")
	FLCOStoryResult StoryResult;

	// ---------- 상호작용 문구 ----------

	// 비활성 상태일 때 (예: "작동하지 않는다")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Prompt")
	FText InactivePrompt;

	// ---------- 상태 ----------

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Puzzle")
	bool bIsSolved = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Puzzle")
	bool bIsActive = true;

private:
	// 저장된 해결 기록이 있으면 연출 없이 해결 상태로 복원
	void TryRestoreSolvedState();

	// 잠금 해제 대상 적용 (해결 시)
	void ApplyWorldResult();

	UFUNCTION()
	void HandleRequiredPuzzleSolved(ALCOPuzzleBase* RequiredPuzzle);

	int32 FailCount = 0;

	FDelegateHandle RestoredHandle;
};
