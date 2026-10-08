#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOInteractionPromptSubsystem.generated.h"

class APawn;
class AActor;
class APlayerController;
class ULCOInteractComponent;
class ULCOInteractionWidget;
class ULCOLocalizationSubsystem;
class ULCOPlayerInteractionComponent;
class UUserWidget;
class UWorld;

UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOInteractionPromptSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	void NotifyBlockingWidgetVisibilityChanged(UUserWidget* Widget, bool bIsVisible);
	void RefreshInteractionPrompt();

private:
	UFUNCTION()
	void HandleFocusChanged(ULCOInteractComponent* NewFocus);

	UFUNCTION()
	void HandleFocusedInteraction(ULCOInteractComponent* Component, AActor* Interactor);

	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void TryCreateInteractionPrompt(APlayerController* PlayerController);
	void CleanupInteractionPrompt();
	ULCOInteractionWidget* FindExistingInteractionWidget(APlayerController* PlayerController) const;
	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromInteractionComponent();
	void BindToFocusedInteractComponent(ULCOInteractComponent* InteractComponent);
	void UnbindFromFocusedInteractComponent();
	FText GetPromptText(ULCOInteractComponent* FocusedComponent) const;
	ULCOLocalizationSubsystem* GetLocalizationSubsystem() const;
	bool ShouldCreateInteractionPromptForWorld(const UWorld* World) const;
	bool IsInteractionPromptSuppressed() const;
	void TrackBlockingWidget(UUserWidget* Widget);
	void UntrackBlockingWidget(UUserWidget* Widget);
	void CompactBlockingWidgets();

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Interaction")
	FName InteractionWidgetName = TEXT("WBP_TestInteraction");

	UPROPERTY(Transient)
	TObjectPtr<ULCOInteractionWidget> InteractionWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOPlayerInteractionComponent> BoundInteractionComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOInteractComponent> FocusedInteractionComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOInteractComponent> BoundFocusedInteractComponent;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UUserWidget>> BlockingWidgets;
};
