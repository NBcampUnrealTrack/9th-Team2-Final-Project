#include "UI/LCODialogueSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/LCOInteractComponent.h"
#include "NPC/LCONPCBase.h"
#include "Player/LCOPlayerInteractionComponent.h"
#include "TimerManager.h"
#include "UI/LCODialogueWidget.h"
#include "UObject/SoftObjectPath.h"

ULCODialogueSubsystem::ULCODialogueSubsystem()
{
	DialogueWidgetClass = TSoftClassPtr<ULCODialogueWidget>(FSoftObjectPath(TEXT("/Game/HJ/UI_Test/Widget_Test/WBP_Dialogue.WBP_Dialogue_C")));
}

void ULCODialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateDialogue(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCODialogueSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupDialogue();
	UnbindFromPlayerController();

	Super::Deinitialize();
}

void ULCODialogueSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupDialogue();
	UnbindFromPlayerController();
	TryCreateDialogue(NewPlayerController);
}

void ULCODialogueSubsystem::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void ULCODialogueSubsystem::HandleFocusChanged(ULCOInteractComponent* NewFocus)
{
	BindToFocusedInteractComponent(NewFocus);
}

void ULCODialogueSubsystem::HandleFocusedInteraction(ULCOInteractComponent* Component, AActor* Interactor)
{
	if (UWorld* World = GetWorld())
	{
		FTimerDelegate StartDialogueDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::TryStartDialogueForInteraction, Component, Interactor);
		World->GetTimerManager().SetTimerForNextTick(StartDialogueDelegate);
	}
}

void ULCODialogueSubsystem::HandleNPCConversationEnded()
{
	if (DialogueWidget && DialogueWidget->IsInViewport())
	{
		DialogueWidget->CloseDialogue(false);
	}

	if (ALCONPCBase* NPC = ActiveNPC.Get())
	{
		NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
	}

	ActiveNPC.Reset();
}

void ULCODialogueSubsystem::HandleDialogueClosed(ALCONPCBase* NPC)
{
	if (IsValid(NPC))
	{
		NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
	}

	if (ActiveNPC.Get() == NPC)
	{
		ActiveNPC.Reset();
	}
}

void ULCODialogueSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (DialogueWidget && DialogueWidget->GetWorld() == World)
	{
		CleanupDialogue();
	}
}

void ULCODialogueSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateDialogue(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

void ULCODialogueSubsystem::TryCreateDialogue(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldCreateDialogueForWorld(PlayerController->GetWorld()))
	{
		return;
	}

	BindToPlayerController(PlayerController);

	if (DialogueWidget && DialogueWidget->GetOwningPlayer() == PlayerController)
	{
		return;
	}

	if (DialogueWidget)
	{
		CleanupDialogue();
	}

	if (DialogueWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] DialogueWidgetClass is not configured"));
		return;
	}

	TSubclassOf<ULCODialogueWidget> LoadedWidgetClass = DialogueWidgetClass.LoadSynchronous();
	if (!LoadedWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Failed to load DialogueWidgetClass: %s"), *DialogueWidgetClass.ToString());
		return;
	}

	TArray<UUserWidget*> ExistingWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PlayerController, ExistingWidgets, LoadedWidgetClass, false);
	for (UUserWidget* ExistingWidget : ExistingWidgets)
	{
		if (ULCODialogueWidget* ExistingDialogueWidget = Cast<ULCODialogueWidget>(ExistingWidget))
		{
			if (ExistingDialogueWidget->GetOwningPlayer() == PlayerController)
			{
				DialogueWidget = ExistingDialogueWidget;
				DialogueWidget->OnDialogueWidgetClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
				DialogueWidget->OnDialogueWidgetClosed.AddDynamic(this, &ThisClass::HandleDialogueClosed);
				return;
			}
		}
	}

	DialogueWidget = CreateWidget<ULCODialogueWidget>(PlayerController, LoadedWidgetClass);
	if (DialogueWidget)
	{
		DialogueWidget->OnDialogueWidgetClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
		DialogueWidget->OnDialogueWidgetClosed.AddDynamic(this, &ThisClass::HandleDialogueClosed);
	}
}

void ULCODialogueSubsystem::CleanupDialogue()
{
	UnbindFromFocusedInteractComponent();
	UnbindFromInteractionComponent();

	if (ALCONPCBase* NPC = ActiveNPC.Get())
	{
		NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
	}
	ActiveNPC.Reset();

	if (!DialogueWidget)
	{
		return;
	}

	DialogueWidget->OnDialogueWidgetClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
	if (DialogueWidget->IsInViewport())
	{
		DialogueWidget->CloseDialogue(true);
	}
	else
	{
		DialogueWidget->RemoveFromParent();
	}

	DialogueWidget = nullptr;
}

void ULCODialogueSubsystem::BindToPlayerController(APlayerController* PlayerController)
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

void ULCODialogueSubsystem::UnbindFromPlayerController()
{
	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	BoundPlayerController.Reset();
	UnbindFromInteractionComponent();
}

void ULCODialogueSubsystem::BindToPawn(APawn* Pawn)
{
	ULCOPlayerInteractionComponent* InteractionComponent = Pawn ? Pawn->FindComponentByClass<ULCOPlayerInteractionComponent>() : nullptr;
	if (BoundInteractionComponent.Get() == InteractionComponent)
	{
		BindToFocusedInteractComponent(InteractionComponent ? InteractionComponent->GetFocusedComponent() : nullptr);
		return;
	}

	UnbindFromInteractionComponent();
	BoundInteractionComponent = InteractionComponent;

	if (InteractionComponent)
	{
		InteractionComponent->OnFocusChanged.RemoveDynamic(this, &ThisClass::HandleFocusChanged);
		InteractionComponent->OnFocusChanged.AddDynamic(this, &ThisClass::HandleFocusChanged);
		BindToFocusedInteractComponent(InteractionComponent->GetFocusedComponent());
	}
}

void ULCODialogueSubsystem::UnbindFromInteractionComponent()
{
	UnbindFromFocusedInteractComponent();

	if (ULCOPlayerInteractionComponent* InteractionComponent = BoundInteractionComponent.Get())
	{
		InteractionComponent->OnFocusChanged.RemoveDynamic(this, &ThisClass::HandleFocusChanged);
	}

	BoundInteractionComponent.Reset();
}

void ULCODialogueSubsystem::BindToFocusedInteractComponent(ULCOInteractComponent* InteractComponent)
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

void ULCODialogueSubsystem::UnbindFromFocusedInteractComponent()
{
	if (ULCOInteractComponent* InteractComponent = BoundFocusedInteractComponent.Get())
	{
		InteractComponent->OnInteracted.RemoveDynamic(this, &ThisClass::HandleFocusedInteraction);
	}

	BoundFocusedInteractComponent.Reset();
}

void ULCODialogueSubsystem::TryStartDialogueForInteraction(ULCOInteractComponent* Component, AActor* Interactor)
{
	if (!IsValid(Component) || !IsValid(Interactor))
	{
		return;
	}

	ALCONPCBase* NPC = Cast<ALCONPCBase>(Component->GetInteractionTarget());
	if (!IsValid(NPC) || !NPC->IsInConversation())
	{
		return;
	}

	APlayerController* PlayerController = BoundPlayerController.Get();
	if (!DialogueWidget || DialogueWidget->GetOwningPlayer() != PlayerController)
	{
		TryCreateDialogue(PlayerController);
	}

	if (!DialogueWidget)
	{
		return;
	}

	if (ALCONPCBase* PreviousNPC = ActiveNPC.Get())
	{
		PreviousNPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
	}

	ActiveNPC = NPC;
	NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
	NPC->OnConversationEnded.AddDynamic(this, &ThisClass::HandleNPCConversationEnded);

	const FName NPCId = NPC->GetNPCId();
	const bool bPlayFirstConversation = !CompletedFirstConversationNPCIds.Contains(NPCId);

	if (!DialogueWidget->OpenDialogue(NPC, Interactor, bPlayFirstConversation))
	{
		NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
		ActiveNPC.Reset();
		if (NPC->IsInConversation())
		{
			NPC->EndConversation();
		}
		return;
	}

	if (bPlayFirstConversation)
	{
		CompletedFirstConversationNPCIds.Add(NPCId);
	}
}

bool ULCODialogueSubsystem::ShouldCreateDialogueForWorld(const UWorld* World) const
{
	return World && World->IsGameWorld();
}
