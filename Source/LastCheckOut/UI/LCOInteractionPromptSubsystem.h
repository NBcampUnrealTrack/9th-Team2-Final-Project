#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOInteractionPromptSubsystem.generated.h"

class APawn;
class AActor;
class APlayerController;
class ALCOPlayerCharacter;
class ULCOInteractComponent;
class ULCOInteractionWidget;
class ULCOLocalizationSubsystem;
class ULCOPlayerInteractionComponent;
class ULCOStaminaComponent;
class UProgressBar;
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

	UFUNCTION()
	void HandleStaminaChanged(int32 CurrentStamina, int32 MaxStamina, float Percent);

	UFUNCTION()
	void HandleSprintStateChanged(bool bIsSprinting);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void TryCreateInteractionPrompt(APlayerController* PlayerController);
	void CleanupInteractionPrompt();
	ULCOInteractionWidget* FindExistingInteractionWidget(APlayerController* PlayerController) const;
	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromInteractionComponent();
	void BindToStaminaOwner(ALCOPlayerCharacter* Character);
	void UnbindFromStaminaOwner();
	void BindToFocusedInteractComponent(ULCOInteractComponent* InteractComponent);
	void UnbindFromFocusedInteractComponent();
	void RefreshStaminaBar();
	void UpdateStaminaBar(int32 CurrentStamina, int32 MaxStamina, float Percent);
	UProgressBar* FindExistingStaminaBar(APlayerController* PlayerController) const;
	FText GetPromptText(ULCOInteractComponent* FocusedComponent) const;
	ULCOLocalizationSubsystem* GetLocalizationSubsystem() const;
	bool ShouldCreateInteractionPromptForWorld(const UWorld* World) const;
	bool IsInteractionPromptSuppressed() const;
	void TrackBlockingWidget(UUserWidget* Widget);
	void UntrackBlockingWidget(UUserWidget* Widget);
	void CompactBlockingWidgets();

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Interaction")
	FName InteractionWidgetName = TEXT("WBP_TestInteraction");

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Stamina")
	FName HudWidgetName = TEXT("WBP_TestHUD");

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Stamina")
	FName StaminaProgressBarName = TEXT("ProgressBar_Stamina");

	UPROPERTY(Transient)
	TObjectPtr<ULCOInteractionWidget> InteractionWidget;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> StaminaProgressBar;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALCOPlayerCharacter> BoundStaminaCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOStaminaComponent> BoundStaminaComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOPlayerInteractionComponent> BoundInteractionComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOInteractComponent> FocusedInteractionComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOInteractComponent> BoundFocusedInteractComponent;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UUserWidget>> BlockingWidgets;

	bool bStaminaExhausted = false;
};
