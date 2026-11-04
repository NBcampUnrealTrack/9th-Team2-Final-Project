#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCODialogueSubsystem.generated.h"

class ALCONPCBase;
class APawn;
class APlayerController;
class ULCOInteractComponent;
class ULCODialogueWidget;
class ULCOPlayerInteractionComponent;
class UWorld;
class ULCOStorySubsystem;

// 대화창이 닫힐 때 (NPC / NPC 없는 대화 공통). bCompletedNormally: 끝까지 진행했는지 (ESC 등 중단이면 false)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCODialogueFinished, FName, DialogueID, bool, bCompletedNormally);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCODialogueNodeEvent, FName, DialogueID, FName, NodeID);

USTRUCT()
struct FLCODialogueProgressTransaction
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FName DialogueID = NAME_None;

	UPROPERTY(Transient)
	bool bStartedFirstConversation = false;

	// 첫 대화 완료 기록에 쓸 이름 (1막: NPCId, 2막 이후: NPCId_Chapter2). 비어 있으면 DialogueID 사용
	UPROPERTY(Transient)
	FName ProgressKey = NAME_None;

	void Reset()
	{
		DialogueID = NAME_None;
		bStartedFirstConversation = false;
		ProgressKey = NAME_None;
	}
};

UCLASS(Config = Game)
class LASTCHECKOUT_API ULCODialogueSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	ULCODialogueSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// NPC 없이 대화 시작 (전화기 등). DialogueFlow_<DialogueID>.csv를 StartNodeID부터 재생
	// WorkSenior는 기존 첫 대화 완료 기록으로 FIRST_001 / SECOND_001을 선택한다.
	// 그 외 NPC 없는 대화는 진행 기록을 사용하지 않는다. 이미 대화 중이거나 열기에 실패하면 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	bool StartDialogue(FName DialogueID, FName StartNodeID, AActor* Interactor);

	TArray<FName> GetCompletedFirstConversations() const
	{
		TArray<FName> Result = CompletedFirstConversationNPCIds.Array();
		// Namespaced UI state travels through the existing dialogue save field.
		for (FName State : SeniorCleanerStates)
		{
			// Story owns notebook persistence; retain legacy storage only if Story is unavailable.
			if (GetStorySubsystem() && (State == FName(TEXT("NOTEBOOK_CONFISCATED")) || State == FName(TEXT("C2_NotebookConfiscated")))) continue;
			Result.Add(FName(*(TEXT("__LCO_SC_STATE__") + State.ToString())));
		}
		return Result;
	}

	void RestoreCompletedFirstConversations(const TArray<FName>& InIds)
	{
		CompletedFirstConversationNPCIds.Empty();
		SeniorCleanerStates.Empty();
		for (FName ID : InIds)
		{
			const FString Value = ID.ToString();
			const FString Prefix(TEXT("__LCO_SC_STATE__"));
			if (Value.StartsWith(Prefix))
			{
				const FName State(*Value.RightChop(Prefix.Len()));
				// SaveManager restores Story first. Old UI mirrors must not re-confiscate a returned notebook.
				if (GetStorySubsystem() && (State == FName(TEXT("NOTEBOOK_CONFISCATED")) || State == FName(TEXT("C2_NotebookConfiscated")))) continue;
				SetSeniorCleanerState(State, true);
			}
			else CompletedFirstConversationNPCIds.Add(ID);
		}
		HandleStoryStateFlagChanged(TEXT("C2_NotebookConfiscated"), IsNotebookConfiscated());
	}

	void ResetDialogueProgress()
	{
		CompletedFirstConversationNPCIds.Empty();
		SeniorCleanerStates.Empty();
	}

	UFUNCTION(BlueprintPure, Category = "LCO|Dialogue|SeniorCleaner")
	bool HasSeniorCleanerState(FName State) const
	{
		if (State == FName(TEXT("NOTEBOOK_CONFISCATED")) || State == FName(TEXT("C2_NotebookConfiscated")))
			return IsNotebookConfiscated();
		return SeniorCleanerStates.Contains(State);
	}

	// The three SC_* states plus the legacy/canonical notebook state names are accepted.
	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue|SeniorCleaner")
	void SetSeniorCleanerState(FName State, bool bEnabled);

	// Story's C2_NotebookConfiscated is authoritative, including after load without change events.
	UFUNCTION(BlueprintPure, Category = "LCO|Dialogue")
	bool IsNotebookConfiscated() const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	void SetNotebookConfiscated(bool bConfiscated);

	// 대화창이 닫힐 때 호출 (NPC 대화 포함)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Dialogue")
	FOnLCODialogueFinished OnDialogueFinished;

	UPROPERTY(BlueprintAssignable, Category = "LCO|Dialogue")
	FOnLCODialogueNodeEvent OnDialogueNodeEntered;

	UPROPERTY(BlueprintAssignable, Category = "LCO|Dialogue")
	FOnLCODialogueNodeEvent OnDialogueNodeConfirmed;

private:
	ULCOStorySubsystem* GetStorySubsystem() const;

	UFUNCTION()
	void HandleStoryStateFlagChanged(FName FlagName, bool bIsSet);

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleFocusChanged(ULCOInteractComponent* NewFocus);

	UFUNCTION()
	void HandleFocusedInteraction(ULCOInteractComponent* Component, AActor* Interactor);

	UFUNCTION()
	void HandleNPCConversationEnded();

	UFUNCTION()
	void HandleDialogueClosed(ALCONPCBase* NPC, FName DialogueID, bool bCompletedNormally, bool bCompletedFirstConversation);

	UFUNCTION()
	void HandleDialogueNodeEntered(FName DialogueID, FName NodeID);

	UFUNCTION()
	void HandleDialogueNodeConfirmed(FName DialogueID, FName NodeID);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void TryCreateDialogue(APlayerController* PlayerController);
	void CleanupDialogue();
	void BeginDialogueProgressTransaction(FName DialogueID, bool bStartedFirstConversation, FName ProgressKey = NAME_None);

	// 현재 챕터에 맞는 NPC 대화 시작 노드 앞글자(OutNodePrefix)와 첫 대화 기록 이름(OutProgressKey)을 구함
	// 2막 이후라도 CSV에 해당 챕터 노드(C2_FIRST_01)가 없으면 1막 대화를 그대로 사용
	void GetChapterDialogueSettings(FName NPCId, FString& OutNodePrefix, FName& OutProgressKey) const;

	static bool HasDialogueFlowNode(FName DialogueID, FName NodeID);
	void CommitDialogueProgressTransaction(FName DialogueID, bool bCompletedNormally, bool bCompletedFirstConversation);
	void DiscardDialogueProgressTransaction(FName DialogueID);
	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromInteractionComponent();
	void BindToFocusedInteractComponent(ULCOInteractComponent* InteractComponent);
	void UnbindFromFocusedInteractComponent();
	void TryStartDialogueForInteraction(ULCOInteractComponent* Component, AActor* Interactor);
	bool ShouldCreateDialogueForWorld(const UWorld* World) const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Dialogue")
	TSoftClassPtr<ULCODialogueWidget> DialogueWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULCODialogueWidget> DialogueWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOPlayerInteractionComponent> BoundInteractionComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOInteractComponent> BoundFocusedInteractComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALCONPCBase> ActiveNPC;

	UPROPERTY(Transient)
	TSet<FName> CompletedFirstConversationNPCIds;

	UPROPERTY(Transient)
	TSet<FName> SeniorCleanerStates;

	UPROPERTY(Transient)
	FLCODialogueProgressTransaction PendingProgressTransaction;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOStorySubsystem> BoundStorySubsystem;
};
