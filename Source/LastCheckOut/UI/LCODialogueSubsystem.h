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

// 대화창이 닫힐 때 (NPC / NPC 없는 대화 공통). bCompletedNormally: 끝까지 진행했는지 (ESC 등 중단이면 false)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCODialogueFinished, FName, DialogueID, bool, bCompletedNormally);

USTRUCT()
struct FLCODialogueProgressTransaction
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FName DialogueID = NAME_None;

	UPROPERTY(Transient)
	bool bStartedFirstConversation = false;

	void Reset()
	{
		DialogueID = NAME_None;
		bStartedFirstConversation = false;
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
	// NPC 첫 대화 기록 / 진행 상태 트랜잭션은 사용하지 않음. 이미 대화 중이거나 열기에 실패하면 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	bool StartDialogue(FName DialogueID, FName StartNodeID, AActor* Interactor);

	// 대화창이 닫힐 때 호출 (NPC 대화 포함)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Dialogue")
	FOnLCODialogueFinished OnDialogueFinished;

private:
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

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void TryCreateDialogue(APlayerController* PlayerController);
	void CleanupDialogue();
	void BeginDialogueProgressTransaction(FName DialogueID, bool bStartedFirstConversation);
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
	FLCODialogueProgressTransaction PendingProgressTransaction;
};
