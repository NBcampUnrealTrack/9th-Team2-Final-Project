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
	void HandleDialogueClosed(ALCONPCBase* NPC);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void TryCreateDialogue(APlayerController* PlayerController);
	void CleanupDialogue();
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
};
