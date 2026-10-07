#include "Player/LCOPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"

void ALCOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOPlayerController] DefaultMappingContext is not set"));
		}
	}

	SetUIMode(false);
}

void ALCOPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LCOPlayerController] EnhancedInputComponent not found"));
		return;
	}

	if (InventoryAction)
	{
		EIC->BindAction(InventoryAction, ETriggerEvent::Started, this, &ALCOPlayerController::RequestInventory);
	}

	if (NotebookAction)
	{
		EIC->BindAction(NotebookAction, ETriggerEvent::Started, this, &ALCOPlayerController::RequestNotebook);
	}

	if (OptionsAction)
	{
		EIC->BindAction(OptionsAction, ETriggerEvent::Started, this, &ALCOPlayerController::RequestOptions);
	}
}

void ALCOPlayerController::SetUIMode(bool bEnable)
{
	bInUIMode = bEnable;

	if (bEnable)
	{
		// GameAndUI: 메뉴가 열려 있어도 I / J / O 키로 닫을 수 있도록
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;

		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;

		ResetIgnoreMoveInput();
		ResetIgnoreLookInput();
	}
}

void ALCOPlayerController::RequestInventory()
{
	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Inventory requested"));
	OnInventoryRequested.Broadcast();
}

void ALCOPlayerController::RequestNotebook()
{
	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Notebook requested"));
	OnNotebookRequested.Broadcast();
}

void ALCOPlayerController::RequestOptions()
{
	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Options requested"));
	OnOptionsRequested.Broadcast();
}