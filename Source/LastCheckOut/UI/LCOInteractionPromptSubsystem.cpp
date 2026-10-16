#include "UI/LCOInteractionPromptSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/LCOContainer.h"
#include "Interaction/LCODoor.h"
#include "Interaction/LCOInteractComponent.h"
#include "Interaction/LCOPhone.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "NPC/LCONPCBase.h"
#include "Player/LCOPlayerCharacter.h"
#include "Player/LCOPlayerInteractionComponent.h"
#include "Player/LCOStaminaComponent.h"
#include "TimerManager.h"
#include "UI/LCOInteractionWidget.h"

namespace
{
	const FLinearColor LCOStaminaNormalColor(0.78f, 0.68f, 0.48f, 1.f);
	const FLinearColor LCOStaminaExhaustedColor(1.f, 0.05f, 0.03f, 1.f);
}

void ULCOInteractionPromptSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
		{
			LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
			LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
		}

		TryCreateInteractionPrompt(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCOInteractionPromptSubsystem::Deinitialize()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}

	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupInteractionPrompt();
	UnbindFromPlayerController();

	Super::Deinitialize();
}

void ULCOInteractionPromptSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupInteractionPrompt();
	UnbindFromPlayerController();
	TryCreateInteractionPrompt(NewPlayerController);
}

void ULCOInteractionPromptSubsystem::NotifyBlockingWidgetVisibilityChanged(UUserWidget* Widget, bool bIsVisible)
{
	if (bIsVisible)
	{
		TrackBlockingWidget(Widget);
	}
	else
	{
		UntrackBlockingWidget(Widget);
	}

	RefreshInteractionPrompt();
}

void ULCOInteractionPromptSubsystem::RefreshInteractionPrompt()
{
	if (!IsValid(InteractionWidget))
	{
		InteractionWidget = FindExistingInteractionWidget(BoundPlayerController.Get());
		if (InteractionWidget)
		{
			InteractionWidget->HideInteraction();
		}
		else
		{
			return;
		}
	}

	ULCOInteractComponent* FocusedComponent = FocusedInteractionComponent.Get();
	AActor* Interactor = nullptr;
	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		Interactor = PlayerController->GetPawn();
	}

	if (!IsInteractionPromptSuppressed() && IsValid(FocusedComponent) && FocusedComponent->CanInteract(Interactor))
	{
		InteractionWidget->ShowInteraction(GetPromptText(FocusedComponent, Interactor));
	}
	else
	{
		InteractionWidget->HideInteraction();
	}
}

void ULCOInteractionPromptSubsystem::HandleFocusChanged(ULCOInteractComponent* NewFocus)
{
	BindToFocusedInteractComponent(NewFocus);
	FocusedInteractionComponent = NewFocus;
	RefreshInteractionPrompt();
}

void ULCOInteractionPromptSubsystem::HandleFocusedInteraction(ULCOInteractComponent* Component, AActor* Interactor)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::RefreshInteractionPrompt));
	}
}

void ULCOInteractionPromptSubsystem::HandleLanguageChanged(FName NewLanguage)
{
	RefreshInteractionPrompt();
}

void ULCOInteractionPromptSubsystem::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
	RefreshInteractionPrompt();
	RefreshStaminaBar();
}

void ULCOInteractionPromptSubsystem::HandleStaminaChanged(int32 CurrentStamina, int32 MaxStamina, float Percent)
{
	UpdateStaminaBar(CurrentStamina, MaxStamina, Percent);
}

void ULCOInteractionPromptSubsystem::HandleSprintStateChanged(bool bIsSprinting)
{
	RefreshStaminaBar();
}

void ULCOInteractionPromptSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (InteractionWidget && InteractionWidget->GetWorld() == World)
	{
		CleanupInteractionPrompt();
	}

	if (StaminaProgressBar && StaminaProgressBar->GetWorld() == World)
	{
		StaminaProgressBar = nullptr;
	}
}

void ULCOInteractionPromptSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateInteractionPrompt(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

void ULCOInteractionPromptSubsystem::TryCreateInteractionPrompt(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldCreateInteractionPromptForWorld(PlayerController->GetWorld()))
	{
		return;
	}

	BindToPlayerController(PlayerController);

	if (InteractionWidget && InteractionWidget->GetOwningPlayer() == PlayerController)
	{
		RefreshInteractionPrompt();
		StaminaProgressBar = FindExistingStaminaBar(PlayerController);
		RefreshStaminaBar();
		return;
	}

	InteractionWidget = FindExistingInteractionWidget(PlayerController);
	if (InteractionWidget)
	{
		InteractionWidget->HideInteraction();
		RefreshInteractionPrompt();
	}

	StaminaProgressBar = FindExistingStaminaBar(PlayerController);
	RefreshStaminaBar();
}

void ULCOInteractionPromptSubsystem::CleanupInteractionPrompt()
{
	UnbindFromFocusedInteractComponent();
	UnbindFromInteractionComponent();
	FocusedInteractionComponent.Reset();
	BlockingWidgets.Empty();
	StaminaProgressBar = nullptr;

	if (!IsValid(InteractionWidget))
	{
		return;
	}

	InteractionWidget->HideInteraction();
	InteractionWidget = nullptr;
}

ULCOInteractionWidget* ULCOInteractionPromptSubsystem::FindExistingInteractionWidget(APlayerController* PlayerController) const
{
	if (!PlayerController || InteractionWidgetName.IsNone())
	{
		return nullptr;
	}

	TArray<UUserWidget*> ExistingWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, ExistingWidgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (!IsValid(ExistingWidget) || ExistingWidget->GetOwningPlayer() != PlayerController)
		{
			continue;
		}

		if (ULCOInteractionWidget* ExistingInteractionWidget = Cast<ULCOInteractionWidget>(ExistingWidget))
		{
			if (ExistingInteractionWidget->GetFName() == InteractionWidgetName)
			{
				return ExistingInteractionWidget;
			}
		}

		if (UWidget* NamedWidget = ExistingWidget->GetWidgetFromName(InteractionWidgetName))
		{
			if (ULCOInteractionWidget* ChildInteractionWidget = Cast<ULCOInteractionWidget>(NamedWidget))
			{
				return ChildInteractionWidget;
			}
		}
	}

	return nullptr;
}

void ULCOInteractionPromptSubsystem::BindToPlayerController(APlayerController* PlayerController)
{
	if (BoundPlayerController.Get() == PlayerController)
	{
		BindToPawn(PlayerController ? PlayerController->GetPawn() : nullptr);
		return;
	}

	UnbindFromPlayerController();
	BoundPlayerController = PlayerController;

	if (PlayerController)
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		PlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		BindToPawn(PlayerController->GetPawn());
	}
}

void ULCOInteractionPromptSubsystem::UnbindFromPlayerController()
{
	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	BoundPlayerController.Reset();
	UnbindFromInteractionComponent();
	UnbindFromStaminaOwner();
}

void ULCOInteractionPromptSubsystem::BindToPawn(APawn* Pawn)
{
	BindToStaminaOwner(Cast<ALCOPlayerCharacter>(Pawn));

	ULCOPlayerInteractionComponent* InteractionComponent = Pawn ? Pawn->FindComponentByClass<ULCOPlayerInteractionComponent>() : nullptr;
	if (BoundInteractionComponent.Get() == InteractionComponent)
	{
		FocusedInteractionComponent = InteractionComponent ? InteractionComponent->GetFocusedComponent() : nullptr;
		BindToFocusedInteractComponent(FocusedInteractionComponent.Get());
		return;
	}

	UnbindFromInteractionComponent();
	BoundInteractionComponent = InteractionComponent;

	if (InteractionComponent)
	{
		InteractionComponent->OnFocusChanged.RemoveDynamic(this, &ThisClass::HandleFocusChanged);
		InteractionComponent->OnFocusChanged.AddDynamic(this, &ThisClass::HandleFocusChanged);
		FocusedInteractionComponent = InteractionComponent->GetFocusedComponent();
		BindToFocusedInteractComponent(FocusedInteractionComponent.Get());
	}
	else
	{
		UnbindFromFocusedInteractComponent();
		FocusedInteractionComponent.Reset();
	}
}

void ULCOInteractionPromptSubsystem::UnbindFromInteractionComponent()
{
	UnbindFromFocusedInteractComponent();

	if (ULCOPlayerInteractionComponent* InteractionComponent = BoundInteractionComponent.Get())
	{
		InteractionComponent->OnFocusChanged.RemoveDynamic(this, &ThisClass::HandleFocusChanged);
	}

	BoundInteractionComponent.Reset();
}

void ULCOInteractionPromptSubsystem::BindToStaminaOwner(ALCOPlayerCharacter* Character)
{
	ULCOStaminaComponent* StaminaComponent = Character ? Character->GetStamina() : nullptr;
	if (BoundStaminaCharacter.Get() == Character && BoundStaminaComponent.Get() == StaminaComponent)
	{
		return;
	}

	UnbindFromStaminaOwner();

	BoundStaminaCharacter = Character;
	BoundStaminaComponent = StaminaComponent;
	bStaminaExhausted = StaminaComponent ? StaminaComponent->IsExhausted() : false;

	if (Character)
	{
		Character->OnSprintStateChanged.RemoveDynamic(this, &ThisClass::HandleSprintStateChanged);
		Character->OnSprintStateChanged.AddDynamic(this, &ThisClass::HandleSprintStateChanged);
	}

	if (StaminaComponent)
	{
		StaminaComponent->OnStaminaChanged.RemoveDynamic(this, &ThisClass::HandleStaminaChanged);
		StaminaComponent->OnStaminaChanged.AddDynamic(this, &ThisClass::HandleStaminaChanged);
	}
}

void ULCOInteractionPromptSubsystem::UnbindFromStaminaOwner()
{
	if (ALCOPlayerCharacter* Character = BoundStaminaCharacter.Get())
	{
		Character->OnSprintStateChanged.RemoveDynamic(this, &ThisClass::HandleSprintStateChanged);
	}

	if (ULCOStaminaComponent* StaminaComponent = BoundStaminaComponent.Get())
	{
		StaminaComponent->OnStaminaChanged.RemoveDynamic(this, &ThisClass::HandleStaminaChanged);
	}

	BoundStaminaCharacter.Reset();
	BoundStaminaComponent.Reset();
	bStaminaExhausted = false;
}

void ULCOInteractionPromptSubsystem::BindToFocusedInteractComponent(ULCOInteractComponent* InteractComponent)
{
	if (BoundFocusedInteractComponent.Get() == InteractComponent)
	{
		return;
	}

	UnbindFromFocusedInteractComponent();
	BoundFocusedInteractComponent = InteractComponent;

	if (InteractComponent)
	{
		InteractComponent->OnInteracted.RemoveDynamic(this, &ThisClass::HandleFocusedInteraction);
		InteractComponent->OnInteracted.AddDynamic(this, &ThisClass::HandleFocusedInteraction);
	}
}

void ULCOInteractionPromptSubsystem::UnbindFromFocusedInteractComponent()
{
	if (ULCOInteractComponent* InteractComponent = BoundFocusedInteractComponent.Get())
	{
		InteractComponent->OnInteracted.RemoveDynamic(this, &ThisClass::HandleFocusedInteraction);
	}

	BoundFocusedInteractComponent.Reset();
}

void ULCOInteractionPromptSubsystem::RefreshStaminaBar()
{
	ULCOStaminaComponent* StaminaComponent = BoundStaminaComponent.Get();
	if (!StaminaComponent)
	{
		if (StaminaProgressBar)
		{
			StaminaProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	UpdateStaminaBar(
		StaminaComponent->GetCurrentStamina(),
		StaminaComponent->GetMaxStamina(),
		StaminaComponent->GetStaminaPercent());
}

void ULCOInteractionPromptSubsystem::UpdateStaminaBar(int32 CurrentStamina, int32 MaxStamina, float Percent)
{
	if (!StaminaProgressBar)
	{
		StaminaProgressBar = FindExistingStaminaBar(BoundPlayerController.Get());
		if (!StaminaProgressBar)
		{
			return;
		}
	}

	StaminaProgressBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));

	if (const ULCOStaminaComponent* StaminaComponent = BoundStaminaComponent.Get())
	{
		bStaminaExhausted = StaminaComponent->IsExhausted();
	}
	else if (CurrentStamina <= 0)
	{
		bStaminaExhausted = true;
	}
	else if (CurrentStamina >= 20)
	{
		bStaminaExhausted = false;
	}

	StaminaProgressBar->SetFillColorAndOpacity(bStaminaExhausted ? LCOStaminaExhaustedColor : LCOStaminaNormalColor);

	const bool bIsSprinting = BoundStaminaCharacter.IsValid() && BoundStaminaCharacter->IsSprinting();
	const bool bIsBelowMax = MaxStamina > 0 && CurrentStamina < MaxStamina;
	StaminaProgressBar->SetVisibility((bIsSprinting || bIsBelowMax) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

UProgressBar* ULCOInteractionPromptSubsystem::FindExistingStaminaBar(APlayerController* PlayerController) const
{
	if (!PlayerController || StaminaProgressBarName.IsNone())
	{
		return nullptr;
	}

	TArray<UUserWidget*> ExistingWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, ExistingWidgets, UUserWidget::StaticClass(), false);

	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (!IsValid(ExistingWidget) || ExistingWidget->GetOwningPlayer() != PlayerController || ExistingWidget->GetFName() != HudWidgetName)
		{
			continue;
		}

		if (UProgressBar* ProgressBar = Cast<UProgressBar>(ExistingWidget->GetWidgetFromName(StaminaProgressBarName)))
		{
			return ProgressBar;
		}
	}

	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (!IsValid(ExistingWidget) || ExistingWidget->GetOwningPlayer() != PlayerController)
		{
			continue;
		}

		if (UProgressBar* ProgressBar = Cast<UProgressBar>(ExistingWidget->GetWidgetFromName(StaminaProgressBarName)))
		{
			return ProgressBar;
		}
	}

	return nullptr;
}

FText ULCOInteractionPromptSubsystem::GetPromptText(ULCOInteractComponent* FocusedComponent, AActor* Interactor) const
{
	if (!FocusedComponent)
	{
		return FText::GetEmpty();
	}

	ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem();
	AActor* FocusedActor = FocusedComponent->GetOwner();

	if (const ALCODoor* Door = Cast<ALCODoor>(FocusedActor))
	{
		return LocalizationSubsystem
			? LocalizationSubsystem->GetUIText(Door->IsOpen() ? TEXT("INTERACT_CLOSE_DOOR") : TEXT("INTERACT_OPEN_DOOR"))
			: FocusedComponent->GetInteractPrompt();
	}

	if (Cast<ALCOContainer>(FocusedActor))
	{
		return LocalizationSubsystem
			? LocalizationSubsystem->GetUIText(TEXT("INTERACT_EXAMINE"))
			: FocusedComponent->GetInteractPrompt();
	}

	AActor* InteractionTarget = FocusedComponent->GetInteractionTarget();
	if (Cast<ALCOPhone>(InteractionTarget ? InteractionTarget : FocusedActor))
	{
		if (LocalizationSubsystem)
		{
			const FName TextKey(TEXT("INTERACT_USE_PHONE"));
			const FText LocalizedPrompt = LocalizationSubsystem->GetUIText(TextKey);
			if (!LocalizedPrompt.IsEmpty() && LocalizedPrompt.ToString() != TextKey.ToString()) return LocalizedPrompt;
		}
		return FocusedComponent->GetInteractPrompt(Interactor);
	}
	if (Cast<ALCONPCBase>(InteractionTarget ? InteractionTarget : FocusedActor))
	{
		return LocalizationSubsystem
			? LocalizationSubsystem->GetUIText(TEXT("INTERACT_TALK"))
			: FocusedComponent->GetInteractPrompt(Interactor);
	}

	return FocusedComponent->GetInteractPrompt(Interactor);
}

ULCOLocalizationSubsystem* ULCOInteractionPromptSubsystem::GetLocalizationSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer && LocalPlayer->GetGameInstance()
		? LocalPlayer->GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>()
		: nullptr;
}

bool ULCOInteractionPromptSubsystem::ShouldCreateInteractionPromptForWorld(const UWorld* World) const
{
	return World && World->IsGameWorld();
}

bool ULCOInteractionPromptSubsystem::IsInteractionPromptSuppressed() const
{
	if (const UWorld* World = GetWorld())
	{
		if (World->IsPaused())
		{
			return true;
		}
	}

	if (const APlayerController* PlayerController = BoundPlayerController.Get())
	{
		if (PlayerController->bShowMouseCursor)
		{
			return true;
		}
	}

	for (const TWeakObjectPtr<UUserWidget>& BlockingWidget : BlockingWidgets)
	{
		const UUserWidget* Widget = BlockingWidget.Get();
		if (Widget && Widget->IsInViewport())
		{
			return true;
		}
	}

	return false;
}

void ULCOInteractionPromptSubsystem::TrackBlockingWidget(UUserWidget* Widget)
{
	if (!Widget || Widget == InteractionWidget.Get())
	{
		return;
	}

	CompactBlockingWidgets();
	for (const TWeakObjectPtr<UUserWidget>& BlockingWidget : BlockingWidgets)
	{
		if (BlockingWidget.Get() == Widget)
		{
			return;
		}
	}

	BlockingWidgets.Add(Widget);
}

void ULCOInteractionPromptSubsystem::UntrackBlockingWidget(UUserWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	for (int32 Index = BlockingWidgets.Num() - 1; Index >= 0; --Index)
	{
		if (!BlockingWidgets[Index].IsValid() || BlockingWidgets[Index].Get() == Widget)
		{
			BlockingWidgets.RemoveAtSwap(Index);
		}
	}
}

void ULCOInteractionPromptSubsystem::CompactBlockingWidgets()
{
	for (int32 Index = BlockingWidgets.Num() - 1; Index >= 0; --Index)
	{
		if (!BlockingWidgets[Index].IsValid())
		{
			BlockingWidgets.RemoveAtSwap(Index);
		}
	}
}
