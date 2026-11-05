#include "Game/LCOTaxiIntroDirector.h"
#include "Game/LCOGameMode.h"
#include "UI/LCODialogueSubsystem.h"
#include "UI/LCOLoadingSubsystem.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Story/LCOStorySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"

namespace
{
	ULCODialogueSubsystem* GetDialogueSubsystem(const UObject* WorldContextObject)
	{
		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
		ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetSubsystem<ULCODialogueSubsystem>() : nullptr;
	}

	// 인트로 동안 퀵슬롯 HUD 숨김
	void SetQuickSlotHUDHidden(const UObject* WorldContextObject, bool bHidden)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(WorldContextObject, 0);
		if (ULCOQuickSlotComponent* QuickSlot = PlayerPawn ? PlayerPawn->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr)
		{
			QuickSlot->SetHUDHiddenByCutscene(bHidden);
		}
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

	// 이미 본 인트로면 건너뜀
	ULCOStorySubsystem* StorySubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOStorySubsystem>() : nullptr;
	if (!PlayedStateFlag.IsNone() && StorySubsystem && StorySubsystem->HasStateFlag(PlayedStateFlag))
	{
		// Log Test
		UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Already played (%s) -> skip"), *PlayedStateFlag.ToString());
		return;
	}

	SetQuickSlotHUDHidden(this, true);

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bLockPlayerMovement)
		{
			PlayerController->SetIgnoreMoveInput(true);
		}
		if (bLockPlayerLook)
		{
			PlayerController->SetIgnoreLookInput(true);
		}
		bInputLocked = bLockPlayerMovement || bLockPlayerLook;

		// 페이드 인 전까지 검은 화면 유지
		if (bStartScreenBlack && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
		}
	}

	// 택시 출발 사운드 (대사 확인 시점)
	if (ULCODialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this))
	{
		DialogueSubsystem->OnDialogueNodeConfirmed.AddUniqueDynamic(this, &ThisClass::HandleDialogueNodeConfirmed);
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
	GetWorldTimerManager().SetTimer(StartDialogueTimerHandle, this, &ThisClass::BeginIntroSequence, StartDelay, false);
}

void ALCOTaxiIntroDirector::BeginIntroSequence()
{
	// BeginPlay 때 폰이 아직 없었을 수 있어서 한 번 더
	SetQuickSlotHUDHidden(this, true);

	// 검은 화면에서 위치 자막 먼저 (한 번만)
	if (bStartScreenBlack && !bLocationCaptionShown && LocationCaptionClass && LocationCaptionLines.Num() > 0)
	{
		bLocationCaptionShown = true;

		APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
		LocationCaptionWidget = PlayerController ? CreateWidget<ULCOLocationCaptionWidget>(PlayerController, LocationCaptionClass) : nullptr;
		if (LocationCaptionWidget)
		{
			LocationCaptionWidget->AddToViewport(50);
			LocationCaptionWidget->OnCaptionFinished.AddUniqueDynamic(this, &ThisClass::HandleLocationCaptionFinished);
			LocationCaptionWidget->StartCaption(LocationCaptionLines, ClockStartHour * 3600 + ClockStartMinute * 60 + ClockStartSecond);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Location caption widget creation failed -> skip"));
	}

	ContinueIntroSequence();
}

void ALCOTaxiIntroDirector::HandleLocationCaptionFinished()
{
	if (LocationCaptionWidget)
	{
		LocationCaptionWidget->OnCaptionFinished.RemoveDynamic(this, &ThisClass::HandleLocationCaptionFinished);
		LocationCaptionWidget->RemoveFromParent();
		LocationCaptionWidget = nullptr;
	}

	ContinueIntroSequence();
}

void ALCOTaxiIntroDirector::ContinueIntroSequence()
{
	if (!bStartScreenBlack)
	{
		StartIntroDialogue();
		return;
	}

	// 대사를 넘길 때 페이드 인: 검은 화면 그대로 대화 시작
	if (!FadeInNodeID.IsNone())
	{
		StartIntroDialogue();
		return;
	}

	// 페이드 인이 끝난 뒤 대화 시작
	TriggerArrivalFadeIn();

	if (ArrivalFadeInDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(ArrivalFadeTimerHandle, this, &ThisClass::StartIntroDialogue, 
			ArrivalFadeInDuration, false);
	}
	else
	{
		StartIntroDialogue();
	}
}

void ALCOTaxiIntroDirector::TriggerArrivalFadeIn()
{
	if (bArrivalFadePlayed)
	{
		return;
	}
	bArrivalFadePlayed = true;

	PlayTaxiArrivalFadeIn();

	// 페이드 인이 끝나면 브금
	if (ArrivalFadeInDuration > 0.f)
	{
		GetWorldTimerManager().SetTimer(ArrivalBGMTimerHandle, this, &ThisClass::PlayArrivalBGM, 
			ArrivalFadeInDuration, false);
	}
	else
	{
		PlayArrivalBGM();
	}
}

void ALCOTaxiIntroDirector::PlayArrivalBGM_Implementation()
{
	if (!ArrivalBGM)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] PlayArrivalBGM: ArrivalBGM is not set"));
		return;
	}

	ArrivalBGMComponent = UGameplayStatics::CreateSound2D(this, ArrivalBGM, ArrivalBGMVolume, 
		1.f, 0.f, nullptr, false, true);
	
	if (!ArrivalBGMComponent)
	{
		return;
	}

	// Log Test
	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] PlayArrivalBGM: %s"), *ArrivalBGM->GetName());

	if (ArrivalBGMFadeInTime > 0.f)
	{
		ArrivalBGMComponent->FadeIn(ArrivalBGMFadeInTime, 1.f);
	}
	else
	{
		ArrivalBGMComponent->Play();
	}
}

void ALCOTaxiIntroDirector::PlayTaxiArrivalFadeIn_Implementation()
{
	// Log Test
	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] PlayTaxiArrivalFadeIn (%.1fs)"), ArrivalFadeInDuration);

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(1.f, 0.f, 
			ArrivalFadeInDuration, FLinearColor::Black, false, false);
	}
}

void ALCOTaxiIntroDirector::HandleDialogueNodeConfirmed(FName ConfirmedDialogueID, FName NodeID)
{
	// 다른 NPC 대화는 무시
	if (ConfirmedDialogueID != DialogueID)
	{
		return;
	}

	// 검은 화면 대사를 넘길 때 페이드 인
	if (bStartScreenBlack && !FadeInNodeID.IsNone() && NodeID == FadeInNodeID)
	{
		TriggerArrivalFadeIn();
	}

	// 택시 출발 사운드 (한 번만)
	if (!bDepartureSoundPlayed && NodeID == DepartureSoundNodeID)
	{
		bDepartureSoundPlayed = true;
		PlayTaxiDepartureSound();
	}
}

void ALCOTaxiIntroDirector::PlayTaxiDepartureSound_Implementation()
{
	if (!TaxiDepartureSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] PlayTaxiDepartureSound: TaxiDepartureSound is not set"));
		return;
	}

	// Log Test
	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] PlayTaxiDepartureSound: %s"), *TaxiDepartureSound->GetName());
	UGameplayStatics::PlaySound2D(this, TaxiDepartureSound, DepartureSoundVolume);
}

void ALCOTaxiIntroDirector::UnlockPlayerInput()
{
	SetQuickSlotHUDHidden(this, false);

	if (!bInputLocked)
	{
		return;
	}
	bInputLocked = false;

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bLockPlayerMovement)
		{
			PlayerController->SetIgnoreMoveInput(false);
		}
		if (bLockPlayerLook)
		{
			PlayerController->SetIgnoreLookInput(false);
		}
	}

	// Log Test
	UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Player input unlocked"));
}

void ALCOTaxiIntroDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StartDialogueTimerHandle);
	GetWorldTimerManager().ClearTimer(ArrivalFadeTimerHandle);
	GetWorldTimerManager().ClearTimer(ArrivalBGMTimerHandle);

	if (LocationCaptionWidget)
	{
		LocationCaptionWidget->OnCaptionFinished.RemoveDynamic(this, &ThisClass::HandleLocationCaptionFinished);
		LocationCaptionWidget->RemoveFromParent();
		LocationCaptionWidget = nullptr;
	}

	if (ULCOLoadingSubsystem* LoadingSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOLoadingSubsystem>() : nullptr)
	{
		LoadingSubsystem->OnLoadingFinished.RemoveDynamic(this, &ThisClass::HandleLoadingFinished);
	}

	if (ULCODialogueSubsystem* DialogueSubsystem = GetDialogueSubsystem(this))
	{
		DialogueSubsystem->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished);
		DialogueSubsystem->OnDialogueNodeConfirmed.RemoveDynamic(this, &ThisClass::HandleDialogueNodeConfirmed);
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
		UnlockPlayerInput();   // 대화 실패 시 갇히지 않게
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

	// 인트로를 본 기록
	if (!PlayedStateFlag.IsNone())
	{
		if (ULCOStorySubsystem* StorySubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOStorySubsystem>() : nullptr)
		{
			StorySubsystem->SetStateFlag(PlayedStateFlag);

			// Log Test
			UE_LOG(LogTemp, Warning, TEXT("[LCOTaxiIntroDirector] Intro played flag set: %s"), *PlayedStateFlag.ToString());
		}
	}

	// 다음 맵이 없으면 (호텔 도착 맵) 여기서 잠금 해제
	if (NextLevel.IsNull())
	{
		UnlockPlayerInput();
		return;
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
