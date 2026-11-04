#include "Interaction/LCOPhone.h"
#include "Story/LCOStorySubsystem.h"
#include "Save/LCOSaveManager.h"
#include "UI/LCODialogueSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

ALCOPhone::ALCOPhone()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	UsePrompt = NSLOCTEXT("LCOInteraction", "PhoneUsePrompt", "전화기 사용");
}

void ALCOPhone::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	if (IsMenuOpen() || IsInCall())
	{
		return;
	}

	MenuUser = Interactor;

	UE_LOG(LogTemp, Log, TEXT("[LCOPhone] %s MenuRequested by %s"), *GetName(), *GetNameSafe(Interactor));

	OnMenuRequested(Interactor);
	OnMenuRequestedDelegate.Broadcast(this, Interactor);
}

bool ALCOPhone::StartCall()
{
	AActor* Caller = ConsumeMenuUser();
	if (!IsValid(Caller) || IsInCall())
	{
		return false;
	}

	const int32 EntryIndex = FindAvailableCallIndex();
	if (EntryIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPhone] %s: No call available"), *GetName());
		OnNoCallAvailable(Caller);
		return false;
	}

	ActiveEntryIndex = EntryIndex;
	ActiveCaller = Caller;

	const FLCOPhoneCallEntry& Entry = CallEntries[EntryIndex];

	UE_LOG(LogTemp, Log, TEXT("[LCOPhone] %s CallStarted: Entry %d (Dialogue: %s, StartNode: %s)"),
		*GetName(), EntryIndex, *Entry.DialogueID.ToString(), *Entry.StartNodeID.ToString());

	OnCallStarted(Caller, EntryIndex, Entry.DialogueID, Entry.StartNodeID);
	OnCallStartedDelegate.Broadcast(this, Caller, EntryIndex, Entry.DialogueID, Entry.StartNodeID);

	// BP 이벤트에서 이미 통화를 끝냈거나, 대화 없이 BP가 직접 처리하는 통화
	if (!IsInCall() || Entry.DialogueID.IsNone())
	{
		return true;
	}

	if (!StartCallDialogue(Caller, Entry))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhone] %s: Failed to open dialogue %s (StartNode: %s)"),
			*GetName(), *Entry.DialogueID.ToString(), *Entry.StartNodeID.ToString());
		FinishCall(false);
		return false;
	}

	return true;
}

bool ALCOPhone::SaveAtPhone()
{
	ConsumeMenuUser();

	if (!CanSave())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhone] %s: Cannot save now (already saving)"), *GetName());
		OnGameSaved(false);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPhone] %s SaveRequested"), *GetName());
	return RequestSave();
}

void ALCOPhone::CloseMenu()
{
	ConsumeMenuUser();
}

bool ALCOPhone::CanSave() const
{
	if (bWaitingForSave)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOSaveManager* SaveManager = GameInstance ? GameInstance->GetSubsystem<ULCOSaveManager>() : nullptr;
	return SaveManager && !SaveManager->IsSaving();
}

AActor* ALCOPhone::ConsumeMenuUser()
{
	if (!IsMenuOpen())
	{
		return nullptr;
	}

	AActor* User = MenuUser.Get();
	MenuUser.Reset();

	OnMenuClosed();
	return User;
}

bool ALCOPhone::StartCallDialogue(AActor* Caller, const FLCOPhoneCallEntry& Entry)
{
	const APawn* CallerPawn = Cast<APawn>(Caller);
	const APlayerController* PC = CallerPawn ? CallerPawn->GetController<APlayerController>() : nullptr;
	ULCODialogueSubsystem* DialogueSubsystem = ULocalPlayer::GetSubsystem<ULCODialogueSubsystem>(PC ? PC->GetLocalPlayer() : nullptr);
	if (!DialogueSubsystem)
	{
		return false;
	}

	// 대화창이 바로 닫혀도 종료를 받을 수 있도록 먼저 바인딩
	BoundDialogueSubsystem = DialogueSubsystem;
	DialogueSubsystem->OnDialogueFinished.AddUniqueDynamic(this, &ALCOPhone::HandleDialogueFinished);

	if (!DialogueSubsystem->StartDialogue(Entry.DialogueID, Entry.StartNodeID, Caller))
	{
		UnbindFromDialogue();
		return false;
	}

	return true;
}

void ALCOPhone::UnbindFromDialogue()
{
	if (ULCODialogueSubsystem* DialogueSubsystem = BoundDialogueSubsystem.Get())
	{
		DialogueSubsystem->OnDialogueFinished.RemoveDynamic(this, &ALCOPhone::HandleDialogueFinished);
	}

	BoundDialogueSubsystem.Reset();
}

void ALCOPhone::HandleDialogueFinished(FName DialogueID, bool bCompletedNormally)
{
	// 다른 대화(NPC 등)가 닫힌 경우는 무시
	if (!CallEntries.IsValidIndex(ActiveEntryIndex) || CallEntries[ActiveEntryIndex].DialogueID != DialogueID)
	{
		return;
	}

	FinishCall(bCompletedNormally);
}

bool ALCOPhone::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return IsMenuOpen() || IsInCall() || !CanSave();
}

FText ALCOPhone::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return UsePrompt;
}

void ALCOPhone::FinishCall(bool bCompletedNormally)
{
	if (!IsInCall())
	{
		return;
	}

	const int32 EntryIndex = ActiveEntryIndex;
	AActor* Caller = ActiveCaller.Get();

	ActiveEntryIndex = INDEX_NONE;
	ActiveCaller.Reset();
	UnbindFromDialogue();

	UE_LOG(LogTemp, Log, TEXT("[LCOPhone] %s CallFinished: Entry %d (Completed: %d)"), *GetName(), EntryIndex, bCompletedNormally);

	if (bCompletedNormally && CallEntries.IsValidIndex(EntryIndex))
	{
		ApplyCallResult(CallEntries[EntryIndex]);

		// WorkSenior saves only through the explicit company-report action.
		if (bSaveOnCallFinished && CallEntries[EntryIndex].DialogueID != FName(TEXT("WorkSenior")))
		{
			RequestSave();
		}
	}

	OnCallFinished(Caller, bCompletedNormally);
	OnCallFinishedDelegate.Broadcast(this, Caller, bCompletedNormally);
}

int32 ALCOPhone::FindAvailableCallIndex() const
{
	for (int32 Index = 0; Index < CallEntries.Num(); ++Index)
	{
		if (IsEntryAvailable(CallEntries[Index]))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool ALCOPhone::IsEntryAvailable(const FLCOPhoneCallEntry& Entry) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOStorySubsystem* Story = GameInstance ? GameInstance->GetSubsystem<ULCOStorySubsystem>() : nullptr;
	if (!Story)
	{
		return false;
	}

	if (!Entry.RequiredChapterID.IsNone() && Story->GetCurrentChapterID() != Entry.RequiredChapterID)
	{
		return false;
	}

	if (!Entry.RequiredCurrentObjectiveID.IsNone() && Story->GetCurrentObjectiveID() != Entry.RequiredCurrentObjectiveID)
	{
		return false;
	}

	for (const FName& ObjectiveID : Entry.RequiredCompletedObjectives)
	{
		if (!Story->IsObjectiveCompleted(ObjectiveID))
		{
			return false;
		}
	}

	for (const ELCOStoryFlag Flag : Entry.RequiredFlags)
	{
		if (!Story->HasFlag(Flag))
		{
			return false;
		}
	}

	return true;
}

void ALCOPhone::ApplyCallResult(const FLCOPhoneCallEntry& Entry) const
{
	UGameInstance* GameInstance = GetGameInstance();
	ULCOStorySubsystem* Story = GameInstance ? GameInstance->GetSubsystem<ULCOStorySubsystem>() : nullptr;
	if (!Story)
	{
		return;
	}

	if (!Entry.ObjectiveToComplete.IsNone())
	{
		Story->CompleteObjective(Entry.ObjectiveToComplete);
	}

	for (const ELCOStoryFlag Flag : Entry.FlagsToUnlock)
	{
		Story->TryUnlock(Flag);
	}
}

bool ALCOPhone::RequestSave()
{
	UGameInstance* GameInstance = GetGameInstance();
	ULCOSaveManager* SaveManager = GameInstance ? GameInstance->GetSubsystem<ULCOSaveManager>() : nullptr;
	if (!SaveManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhone] %s: SaveManager not found"), *GetName());
		OnGameSaved(false);
		return false;
	}

	SaveManager->OnSaveFinished.AddUniqueDynamic(this, &ALCOPhone::HandleSaveFinished);
	bWaitingForSave = true;

	// 다른 곳에서 이미 저장 중이면 false. 이때는 저장 완료 이벤트를 기다리지 않음
	if (!SaveManager->SaveGame())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhone] %s: SaveGame request failed (already saving)"), *GetName());
		SaveManager->OnSaveFinished.RemoveDynamic(this, &ALCOPhone::HandleSaveFinished);
		bWaitingForSave = false;
		OnGameSaved(false);
		return false;
	}

	return true;
}

void ALCOPhone::HandleSaveFinished(bool bSuccess)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (ULCOSaveManager* SaveManager = GameInstance->GetSubsystem<ULCOSaveManager>())
		{
			SaveManager->OnSaveFinished.RemoveDynamic(this, &ALCOPhone::HandleSaveFinished);
		}
	}

	bWaitingForSave = false;

	UE_LOG(LogTemp, Log, TEXT("[LCOPhone] %s GameSaved (Success: %d)"), *GetName(), bSuccess);
	OnGameSaved(bSuccess);
}

void ALCOPhone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromDialogue();

	// GameInstance 서브시스템은 레벨보다 오래 살아 있으므로, 저장 대기 중에 사라지면 바인딩 해제
	if (bWaitingForSave)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (ULCOSaveManager* SaveManager = GameInstance->GetSubsystem<ULCOSaveManager>())
			{
				SaveManager->OnSaveFinished.RemoveDynamic(this, &ALCOPhone::HandleSaveFinished);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}
