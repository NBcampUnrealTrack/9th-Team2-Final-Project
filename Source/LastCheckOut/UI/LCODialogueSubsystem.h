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
