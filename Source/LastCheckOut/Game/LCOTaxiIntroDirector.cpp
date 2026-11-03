#include "Game/LCOTaxiIntroDirector.h"
#include "Game/LCOGameMode.h"
#include "UI/LCODialogueSubsystem.h"
#include "UI/LCOLoadingSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	ULCODialogueSubsystem* GetDialogueSubsystem(const UObject* WorldContextObject)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
		ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetSubsystem<ULCODialogueSubsystem>() : nullptr;
	}
}

ALCOTaxiIntroDirector::ALCOTaxiIntroDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALCOTaxiIntroDirector::BeginPlay()
{
	Super::BeginPlay();

	if (bLockPlayerMovement)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			PlayerController->SetIgnoreMoveInput(true);
		}
	}

	ULCOLoadingSubsystem* LoadingSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOLoadingSubsystem>() : nullptr;
	if (bWaitForLoadingScreen && LoadingSubsystem && LoadingSubsystem->IsLoading())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Waiting for loading screen"));
		LoadingSubsystem->OnLoadingFinished.RemoveDynamic(this, &ThisClass::HandleLoadingFinished);
		LoadingSubsystem->OnLoadingFinished.AddDynamic(this, &ThisClass::HandleLoadingFinished);
		return;
	}

	ScheduleIntroDialogue();
}

void ALCOTaxiIntroDirector::HandleLoadingFinished(bool bSuccess)
{
	if (ULCOLoadingSubsystem* LoadingSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOLoadingSubsystem>() : nullptr)
	{
		LoadingSubsystem->OnLoadingFinished.RemoveDynamic(this, &ThisClass::HandleLoadingFinished);
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Loading finished (Success = %s) -> Start dialogue in %.1fs"),
		bSuccess ? TEXT("true") : TEXT("false"), StartDelay);
	ScheduleIntroDialogue();
}

void ALCOTaxiIntroDirector::ScheduleIntroDialogue()
{
	GetWorldTimerManager().SetTimer(StartDialogueTimerHandle, this, &ThisClass::StartIntroDialogue, StartDelay, false);
}

void ALCOTaxiIntroDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartDialogueTimerHandle);

	if (ULCOLoadingSubsystem* LoadingSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOLoadingSubsystem>() : nullptr)
	{
		LoadingSubsystem->OnLoadingFinished.RemoveDynamic(this, &ThisClass::HandleLoadingFinished);
	}

	if (ULCODialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this))
	{
		DialogueSubsystem->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished);
	}

	Super::EndPlay(EndPlayReason);
}

void ALCOTaxiIntroDirector::StartIntroDialogue()
{
	ULCODialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this);
	if (!DialogueSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[LCOTaxiIntroDirector] Dialogue subsystem not found; intro stopped"));
		return;
	}

	DialogueSubsystem->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished);
	DialogueSubsystem->OnDialogueFinished.AddDynamic(this, &ThisClass::HandleDialogueFinished);

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!DialogueSubsystem->StartDialogue(DialogueID, StartNodeID, PlayerPawn))
	{
		UE_LOG(LogTemp, Error, TEXT("[LCOTaxiIntroDirector] StartDialogue failed: %s / %s; intro stopped"),
			*DialogueID.ToString(), *StartNodeID.ToString());
		DialogueSubsystem->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Dialogue started: %s"), *DialogueID.ToString());
}

void ALCOTaxiIntroDirector::HandleDialogueFinished(FName FinishedDialogueID, bool bCompletedNormally)
{
	if (FinishedDialogueID != DialogueID)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Dialogue finished: %s (Completed = %s)"),
		*FinishedDialogueID.ToString(), bCompletedNormally ? TEXT("true") : TEXT("false"));

	if (ULCODialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this))
	{
		DialogueSubsystem->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished);
	}

	if (bCompletedNormally)
	{
		TravelToNextLevel();
	}
}

void ALCOTaxiIntroDirector::TravelToNextLevel()
{
	if (bTravelStarted)
	{
		return;
	}

	ALCOGameMode* GameMode = Cast<ALCOGameMode>(UGameplayStatics::GetGameMode(this));
	if (!GameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] LCOGameMode not found"));
		return;
	}

	if (NextLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] NextLevel is not set"));
		return;
	}

	bTravelStarted = true;

	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Travel: %s (Entry = %s)"),
		*NextLevel.ToString(), *NextPlayerStartTag.ToString());
	GameMode->TravelToLevel(NextLevel, NextPlayerStartTag, bAutoSaveOnArrival);
}
