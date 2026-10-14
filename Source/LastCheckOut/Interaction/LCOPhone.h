#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "Story/LCOStoryTypes.h"
#include "LCOPhone.generated.h"

class ULCODialogueSubsystem;

// 전화 1통의 설정: 진행도 조건 + 재생할 대화 + 통화 후 진행 결과
USTRUCT(BlueprintType)
struct FLCOPhoneCallEntry
{
	GENERATED_BODY()

	// ---------- 조건 (비워둔 항목은 검사하지 않음) ----------

	// 현재 챕터가 이 챕터일 때
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FName RequiredChapterID;

	// 현재 목표가 이 목표일 때
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FName RequiredCurrentObjectiveID;

	// 이 목표들을 모두 완료했을 때
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	TArray<FName> RequiredCompletedObjectives;

	// 이 증언 / 키워드 플래그를 모두 획득했을 때
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	TArray<ELCOStoryFlag> RequiredFlags;

	// ---------- 대화 ----------

	// 대화 흐름 ID (Localization/DialogueFlow_<ID>.csv)
	// 비워두면 대화창을 열지 않음 → BP의 OnCallStarted에서 직접 처리하고 FinishCall() 호출 (테스트 / 특수 연출용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName DialogueID;

	// 시작 노드 ID (CSV의 NodeID)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName StartNodeID;

	// ---------- 통화를 끝까지 들었을 때 결과 ----------

	// 완료할 목표 (현재 목표와 같을 때만 완료됨). 비워두면 무시
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	FName ObjectiveToComplete;

	// 획득할 증언 / 키워드 플래그 (해금 조건을 만족해야 획득)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Result")
	TArray<ELCOStoryFlag> FlagsToUnlock;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnPhoneCallStartedSignature, ALCOPhone*, Phone, AActor*, Caller, int32, EntryIndex, FName, DialogueID, FName, StartNodeID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPhoneCallFinishedSignature, ALCOPhone*, Phone, AActor*, Caller, bool, bCompletedNormally);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhoneMenuRequestedSignature, ALCOPhone*, Phone, AActor*, User);

// 전화 걸기 / 저장하기 메뉴를 제공하는 전화기
// 상호작용 → 메뉴 열기 이벤트(OnMenuRequested) → 메뉴 UI에서 StartCall() / SaveAtPhone() / CloseMenu() 호출
// 통화 선택: CallEntries를 위에서부터 검사해서 조건을 만족하는 첫 항목 사용 (진행이 늦은 항목을 위에 둘 것)
// 대사 출력: DialogueID가 있으면 ULCODialogueSubsystem::StartDialogue로 대화창을 열고, 대화창이 닫히면 자동으로 FinishCall() 호출
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOPhone : public AActor, public ILCOInteractionReceiver
{
	GENERATED_BODY()

public:
	ALCOPhone();

	// ILCOInteractionReceiver
	// 메뉴 열기 이벤트만 호출 (통화 / 저장은 메뉴에서 선택)
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 메뉴가 열려 있거나, 통화 중이거나, 저장 중이면 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ---------- 메뉴 버튼 ----------

	// [전화 걸기] 메뉴를 닫고 조건에 맞는 통화 시작. 걸 수 있는 통화가 없으면 OnNoCallAvailable 호출 후 false
	UFUNCTION(BlueprintCallable, Category = "Phone")
	bool StartCall();

	// [저장하기] 메뉴를 닫고 게임 저장. 저장 요청에 실패하면 false (결과는 OnGameSaved)
	UFUNCTION(BlueprintCallable, Category = "Phone")
	bool SaveAtPhone();

	// [취소] 메뉴만 닫기
	UFUNCTION(BlueprintCallable, Category = "Phone")
	void CloseMenu();

	// 지금 걸 수 있는 통화가 있는지 (메뉴의 전화 걸기 버튼 활성화용)
	UFUNCTION(BlueprintPure, Category = "Phone")
	bool CanCall() const { return FindAvailableCallIndex() != INDEX_NONE; }

	// 지금 저장할 수 있는지 (다른 저장이 진행 중이면 false)
	UFUNCTION(BlueprintPure, Category = "Phone")
	bool CanSave() const;

	UFUNCTION(BlueprintPure, Category = "Phone")
	bool IsMenuOpen() const { return MenuUser.IsValid(); }

	// ---------- 통화 ----------

	// 통화가 끝났을 때 호출 (대화창이 닫히면 자동 호출. DialogueID가 없는 통화는 BP에서 호출)
	// bCompletedNormally가 true면 진행 결과를 적용 (bSaveOnCallFinished면 저장까지). false(ESC 등으로 중단)면 아무것도 적용하지 않음
	UFUNCTION(BlueprintCallable, Category = "Phone")
	void FinishCall(bool bCompletedNormally = true);

	// 현재 진행도에서 재생될 통화 인덱스 (없으면 INDEX_NONE). 디버그 / UI용
	UFUNCTION(BlueprintPure, Category = "Phone")
	int32 FindAvailableCallIndex() const;

	UFUNCTION(BlueprintPure, Category = "Phone")
	bool IsInCall() const { return ActiveEntryIndex != INDEX_NONE; }

	// 메뉴를 열어야 할 때 (UI 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "Phone")
	FOnPhoneMenuRequestedSignature OnMenuRequestedDelegate;

	// 통화가 시작될 때 (대화 시스템 연결용)
	UPROPERTY(BlueprintAssignable, Category = "Phone")
	FOnPhoneCallStartedSignature OnCallStartedDelegate;

	// 통화가 끝났을 때
	UPROPERTY(BlueprintAssignable, Category = "Phone")
	FOnPhoneCallFinishedSignature OnCallFinishedDelegate;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- BP 이벤트 ----------

	// 메뉴 열기. 여기서 전화 걸기 / 저장하기 메뉴 UI를 띄운다
	UFUNCTION(BlueprintImplementableEvent, Category = "Phone")
	void OnMenuRequested(AActor* User);

	// 메뉴가 닫혔을 때 (버튼 선택 / 취소 공통. 메뉴 UI 정리용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Phone")
	void OnMenuClosed();

	// 통화 시작 (수화기 드는 소리 등). 대화창은 이 이벤트 다음에 열림
	// DialogueID가 없는 통화는 여기서 직접 연출하고 끝나면 FinishCall() 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "Phone")
	void OnCallStarted(AActor* Caller, int32 EntryIndex, FName DialogueID, FName StartNodeID);

	// 통화 종료 (수화기 내려놓는 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Phone")
	void OnCallFinished(AActor* Caller, bool bCompletedNormally);

	// 조건에 맞는 통화가 없을 때 (신호음만 들림 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Phone")
	void OnNoCallAvailable(AActor* Caller);

	// 저장이 끝났을 때 (저장 완료 안내 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Phone")
	void OnGameSaved(bool bSuccess);

	// ---------- 설정값 ----------

	// 통화 목록. 위에서부터 검사해서 조건을 만족하는 첫 항목 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phone")
	TArray<FLCOPhoneCallEntry> CallEntries;

	// 통화를 끝까지 들었을 때 자동 저장 (기본은 끔. 저장은 메뉴의 저장하기로)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phone")
	bool bSaveOnCallFinished = false;

	// 상호작용 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phone|Prompt")
	FText UsePrompt;

private:
	// 메뉴 상태 해제 + OnMenuClosed 호출. 메뉴를 연 액터 반환
	AActor* ConsumeMenuUser();

	// 진행도가 Entry의 조건을 모두 만족하는지
	bool IsEntryAvailable(const FLCOPhoneCallEntry& Entry) const;

	// 통화 결과(목표 완료, 플래그 획득) 적용
	void ApplyCallResult(const FLCOPhoneCallEntry& Entry) const;

	// Caller의 대화 시스템으로 대화창 열기. 실패하면 false
	bool StartCallDialogue(AActor* Caller, const FLCOPhoneCallEntry& Entry);

	void UnbindFromDialogue();

	UFUNCTION()
	void HandleDialogueFinished(FName DialogueID, bool bCompletedNormally);

	// SaveManager에 저장 요청. 요청에 실패하면 OnGameSaved(false) 호출 후 false
	bool RequestSave();

	UFUNCTION()
	void HandleSaveFinished(bool bSuccess);

	// 대화창 종료를 기다리는 대화 시스템 (대화창으로 진행 중인 통화가 없으면 nullptr)
	TWeakObjectPtr<ULCODialogueSubsystem> BoundDialogueSubsystem;

	// 진행 중인 통화 (없으면 INDEX_NONE)
	int32 ActiveEntryIndex = INDEX_NONE;

	TWeakObjectPtr<AActor> ActiveCaller;

	// 메뉴를 연 액터 (메뉴가 닫혀 있으면 nullptr)
	TWeakObjectPtr<AActor> MenuUser;

	// 이 전화기가 요청한 저장이 진행 중인지 (SaveManager 델리게이트 바인딩 여부)
	bool bWaitingForSave = false;
};
