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
#include "UI/LCONotebookUISubsystem.h"
#include "UI/LCONotebookWidget.h"
#include "Story/LCOStorySubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"

ULCODialogueSubsystem::ULCODialogueSubsystem()
{
	DialogueWidgetClass = TSoftClassPtr<ULCODialogueWidget>(FSoftObjectPath(TEXT("/Game/HJ/UI_Test/Widget_Test/WBP_Dialogue.WBP_Dialogue_C")));
}

void ULCODialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (ULCOStorySubsystem* Story = GetStorySubsystem())
	{
		BoundStorySubsystem = Story;
		Story->OnStateFlagChanged.AddUniqueDynamic(this, &ThisClass::HandleStoryStateFlagChanged);
	}

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateDialogue(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCODialogueSubsystem::Deinitialize()
{
	if (ULCOStorySubsystem* Story = BoundStorySubsystem.Get())
		Story->OnStateFlagChanged.RemoveDynamic(this, &ThisClass::HandleStoryStateFlagChanged);
	BoundStorySubsystem.Reset();
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

bool ULCODialogueSubsystem::StartDialogue(FName DialogueID, FName StartNodeID, AActor* Interactor)
{
	// NPC 대화 중이면 시작하지 않음
	if (ActiveNPC.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] StartDialogue ignored: NPC conversation is active (%s)"), *GetNameSafe(ActiveNPC.Get()));
		return false;
	}

	APlayerController* PlayerController = BoundPlayerController.Get();
	if (!PlayerController)
	{
		const ULocalPlayer* LocalPlayer = GetLocalPlayer();
		PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	}

	if (!DialogueWidget || DialogueWidget->GetOwningPlayer() != PlayerController)
	{
		TryCreateDialogue(PlayerController);
	}

	if (!DialogueWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] StartDialogue failed: dialogue widget is not available"));
		return false;
	}

	if (DialogueID != FName(TEXT("WorkSenior")))
	{
		return DialogueWidget->OpenDialogueByID(DialogueID, StartNodeID, Interactor);
	}

	// Do not replace the transaction belonging to an already active conversation.
	if (DialogueWidget->IsDialogueActive()) return false;
	const bool bFirstCall = !CompletedFirstConversationNPCIds.Contains(DialogueID);
	const FName CallStartNode = bFirstCall ? FName(TEXT("FIRST_001")) : FName(TEXT("SECOND_001"));
	BeginDialogueProgressTransaction(DialogueID, bFirstCall);
	if (!DialogueWidget->OpenDialogueByID(DialogueID, CallStartNode, Interactor, bFirstCall))
	{
		DiscardDialogueProgressTransaction(DialogueID);
		return false;
	}
	// HandleDialogueClosed commits this transaction before broadcasting OnDialogueFinished.
	return true;
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

void ULCODialogueSubsystem::HandleDialogueNodeEntered(FName DialogueID, FName NodeID)
{
	OnDialogueNodeEntered.Broadcast(DialogueID, NodeID);
}

void ULCODialogueSubsystem::HandleDialogueNodeConfirmed(FName DialogueID, FName NodeID)
{
	OnDialogueNodeConfirmed.Broadcast(DialogueID, NodeID);
}

void ULCODialogueSubsystem::HandleDialogueClosed(ALCONPCBase* NPC, FName DialogueID, bool bCompletedNormally, bool bCompletedFirstConversation)
{
	CommitDialogueProgressTransaction(DialogueID, bCompletedNormally, bCompletedFirstConversation);

	if (IsValid(NPC))
	{
		NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
	}

	if (ActiveNPC.Get() == NPC)
	{
		ActiveNPC.Reset();
	}

	OnDialogueFinished.Broadcast(DialogueID, bCompletedNormally);
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
				DialogueWidget->OnDialogueNodeEntered.AddUniqueDynamic(this, &ThisClass::HandleDialogueNodeEntered);
				DialogueWidget->OnDialogueNodeConfirmed.AddUniqueDynamic(this, &ThisClass::HandleDialogueNodeConfirmed);
				return;
			}
		}
	}

	DialogueWidget = CreateWidget<ULCODialogueWidget>(PlayerController, LoadedWidgetClass);
	if (DialogueWidget)
	{
		DialogueWidget->OnDialogueWidgetClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
		DialogueWidget->OnDialogueWidgetClosed.AddDynamic(this, &ThisClass::HandleDialogueClosed);
		DialogueWidget->OnDialogueNodeEntered.AddUniqueDynamic(this, &ThisClass::HandleDialogueNodeEntered);
		DialogueWidget->OnDialogueNodeConfirmed.AddUniqueDynamic(this, &ThisClass::HandleDialogueNodeConfirmed);
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
	PendingProgressTransaction.Reset();

	if (!DialogueWidget)
	{
		return;
	}

	DialogueWidget->OnDialogueWidgetClosed.RemoveDynamic(this, &ThisClass::HandleDialogueClosed);
	DialogueWidget->UnbindFromOptionsInput();
	DialogueWidget->OnDialogueNodeEntered.RemoveDynamic(this, &ThisClass::HandleDialogueNodeEntered);
	DialogueWidget->OnDialogueNodeConfirmed.RemoveDynamic(this, &ThisClass::HandleDialogueNodeConfirmed);
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

void ULCODialogueSubsystem::SetSeniorCleanerState(FName State, bool bEnabled)
{
	if (State == FName(TEXT("NOTEBOOK_CONFISCATED")) || State == FName(TEXT("C2_NotebookConfiscated")))
	{
		SetNotebookConfiscated(bEnabled);
		return;
	}
	static const TSet<FName> Supported = { TEXT("SC_407_CONFIRMED"), TEXT("SC_INTERROGATION_ATTEMPTED"),
		TEXT("SC_POST_T15_SEEN") };
	if (!Supported.Contains(State)) return;
	if (bEnabled) SeniorCleanerStates.Add(State);
	else SeniorCleanerStates.Remove(State);
}

ULCOStorySubsystem* ULCODialogueSubsystem::GetStorySubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<ULCOStorySubsystem>() : nullptr;
}

bool ULCODialogueSubsystem::IsNotebookConfiscated() const
{
	if (const ULCOStorySubsystem* Story = GetStorySubsystem())
		return Story->HasStateFlag(TEXT("C2_NotebookConfiscated"));
	return SeniorCleanerStates.Contains(FName(TEXT("C2_NotebookConfiscated")));
}

void ULCODialogueSubsystem::HandleStoryStateFlagChanged(FName FlagName, bool bIsSet)
{
	if (FlagName != FName(TEXT("C2_NotebookConfiscated"))) return;
	if (bIsSet)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (ULCONotebookUISubsystem* NotebookUI = LocalPlayer->GetSubsystem<ULCONotebookUISubsystem>())
			{
				if (ULCONotebookWidget* Notebook = NotebookUI->GetNotebookWidget()) Notebook->CloseNotebook();
			}
		}
	}
	if (DialogueWidget) DialogueWidget->RefreshNotebookConfiscationState();
}

void ULCODialogueSubsystem::SetNotebookConfiscated(bool bConfiscated)
{
	if (ULCOStorySubsystem* Story = GetStorySubsystem())
	{
		SeniorCleanerStates.Remove(FName(TEXT("NOTEBOOK_CONFISCATED")));
		SeniorCleanerStates.Remove(FName(TEXT("C2_NotebookConfiscated")));
		if (bConfiscated) Story->SetStateFlag(TEXT("C2_NotebookConfiscated"));
		else Story->ClearStateFlag(TEXT("C2_NotebookConfiscated"));
		return;
	}
	if (IsNotebookConfiscated() == bConfiscated) return;
	const FName State(TEXT("C2_NotebookConfiscated"));
	if (bConfiscated) SeniorCleanerStates.Add(State);
	else SeniorCleanerStates.Remove(State);
	HandleStoryStateFlagChanged(State, bConfiscated);
}

void ULCODialogueSubsystem::BeginDialogueProgressTransaction(FName DialogueID, bool bStartedFirstConversation, FName ProgressKey)
{
	PendingProgressTransaction.DialogueID = DialogueID;
	PendingProgressTransaction.bStartedFirstConversation = bStartedFirstConversation;
	PendingProgressTransaction.ProgressKey = ProgressKey.IsNone() ? DialogueID : ProgressKey;
}

void ULCODialogueSubsystem::CommitDialogueProgressTransaction(FName DialogueID, bool bCompletedNormally, bool bCompletedFirstConversation)
{
	if (PendingProgressTransaction.DialogueID != DialogueID)
	{
		PendingProgressTransaction.Reset();
		return;
	}

	if (bCompletedNormally && bCompletedFirstConversation && PendingProgressTransaction.bStartedFirstConversation)
	{
		CompletedFirstConversationNPCIds.Add(PendingProgressTransaction.ProgressKey);
	}

	PendingProgressTransaction.Reset();
}

void ULCODialogueSubsystem::GetChapterDialogueSettings(FName NPCId, FString& OutNodePrefix, FName& OutProgressKey) const
{
	// FIRST_01 / DEFAULT_01, 기록 이름 = NPCId
	OutNodePrefix.Reset();
	OutProgressKey = NPCId;

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	const ULCOStorySubsystem* StorySubsystem = GameInstance ? GameInstance->GetSubsystem<ULCOStorySubsystem>() : nullptr;
	if (!StorySubsystem)
	{
		return;
	}

	// "Chapter2" → 2
	const FString ChapterString = StorySubsystem->GetCurrentChapterID().ToString();
	const FString ChapterPrefix(TEXT("Chapter"));
	if (!ChapterString.StartsWith(ChapterPrefix))
	{
		return;
	}

	const int32 ChapterNumber = FCString::Atoi(*ChapterString.RightChop(ChapterPrefix.Len()));
	if (ChapterNumber < 2)
	{
		return;
	}

	// NPC의 CSV에 해당 챕터 대화가 없다면 → 1막 대화 사용
	const FString ChapterNodePrefix = FString::Printf(TEXT("C%d_"), ChapterNumber);
	if (!HasDialogueFlowNode(NPCId, FName(*(ChapterNodePrefix + TEXT("FIRST_01")))))
	{
		return;
	}

	OutNodePrefix = ChapterNodePrefix;
	OutProgressKey = FName(*FString::Printf(TEXT("%s_%s"), *NPCId.ToString(), *ChapterString));
}

bool ULCODialogueSubsystem::HasDialogueFlowNode(FName DialogueID, FName NodeID)
{
	const FString FlowPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Localization"),
		FString::Printf(TEXT("DialogueFlow_%s.csv"), *DialogueID.ToString()));

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FlowPath))
	{
		return false;
	}

	// 첫 번째 열(NodeID)이 같은 줄이 있는지
	const FString NodePrefix = NodeID.ToString() + TEXT(",");
	for (const FString& Line : Lines)
	{
		if (Line.TrimStart().StartsWith(NodePrefix))
		{
			return true;
		}
	}
	return false;
}

void ULCODialogueSubsystem::DiscardDialogueProgressTransaction(FName DialogueID)
{
	if (PendingProgressTransaction.DialogueID == DialogueID || DialogueID.IsNone())
	{
		PendingProgressTransaction.Reset();
	}
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
		DiscardDialogueProgressTransaction(PreviousNPC->GetNPCId());
	}

	ActiveNPC = NPC;
	NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
	NPC->OnConversationEnded.AddDynamic(this, &ThisClass::HandleNPCConversationEnded);

	const FName NPCId = NPC->GetNPCId();

	FString NodePrefix;
	FName ProgressKey;
	GetChapterDialogueSettings(NPCId, NodePrefix, ProgressKey);

	const bool bPlayFirstConversation = !CompletedFirstConversationNPCIds.Contains(ProgressKey);

	UE_LOG(LogTemp, Log, TEXT("[LCODialogue] NPC dialogue: NPC = %s, NodePrefix = %s, ProgressKey = %s, First = %s"),
		*NPCId.ToString(), NodePrefix.IsEmpty() ? TEXT("None") : *NodePrefix, *ProgressKey.ToString(),
		bPlayFirstConversation ? TEXT("true") : TEXT("false"));

	BeginDialogueProgressTransaction(NPCId, bPlayFirstConversation, ProgressKey);

	if (!DialogueWidget->OpenDialogue(NPC, Interactor, bPlayFirstConversation, NodePrefix))
	{
		NPC->OnConversationEnded.RemoveDynamic(this, &ThisClass::HandleNPCConversationEnded);
		ActiveNPC.Reset();
		DiscardDialogueProgressTransaction(NPCId);
		if (NPC->IsInConversation())
		{
			NPC->EndConversation();
		}
		return;
	}
}

bool ULCODialogueSubsystem::ShouldCreateDialogueForWorld(const UWorld* World) const
{
	return World && World->IsGameWorld();
}
