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
			// 키 설정(사용자 설정)에 이 IMC의 매핑을 등록 → 키 변경 / 기본값 복원 / 저장 가능
			FModifyContextOptions Options;
			Options.bNotifyUserSettings = true;
			Subsystem->AddMappingContext(DefaultMappingContext, 0, Options);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOPlayerController] DefaultMappingContext is not set"));
		}
	}
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = MinViewPitch;
		PlayerCameraManager->ViewPitchMax = MaxViewPitch;
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
	// 퍼즐 보기 중에는 무시
	if (IsInPuzzleView())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Inventory ignored (puzzle view)"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Inventory requested"));
	OnInventoryRequested.Broadcast();
}

void ALCOPlayerController::RequestNotebook()
{
	// 퍼즐 보기 중에는 무시
	if (IsInPuzzleView())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Notebook ignored (puzzle view)"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Notebook requested"));
	OnNotebookRequested.Broadcast();
}

void ALCOPlayerController::RequestOptions()
{
	// 퍼즐 보기 중에는 일시정지 메뉴 대신 퍼즐에 나가기 전달
	if (IsInPuzzleView())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Puzzle exit requested: %s"), *GetNameSafe(CurrentPuzzleActor.Get()));
		OnPuzzleExitRequested.Broadcast(CurrentPuzzleActor.Get());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Options requested"));
	OnOptionsRequested.Broadcast();
}

// ---------- 퍼즐 보기 ----------

void ALCOPlayerController::BeginPuzzleView(AActor* PuzzleActor)
{
	if (!IsValid(PuzzleActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPlayerController] BeginPuzzleView: PuzzleActor is null"));
		return;
	}

	CurrentPuzzleActor = PuzzleActor;
	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Puzzle view begin: %s"), *PuzzleActor->GetName());
}

void ALCOPlayerController::EndPuzzleView()
{
	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerController] Puzzle view end: %s"), *GetNameSafe(CurrentPuzzleActor.Get()));
	CurrentPuzzleActor.Reset();
}