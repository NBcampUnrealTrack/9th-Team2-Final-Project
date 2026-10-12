#include "LCOMainMenuPlayerController.h"

#include "LCOMainMenuWidget.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"

void ALCOMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	SetupMenuCamera();

	if (!MainMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOMainMenuPC] MainMenuWidgetClass가 비어 있음"));
		return;
	}

	MainMenuWidget = CreateWidget<ULCOMainMenuWidget>(this, MainMenuWidgetClass);
	if (!MainMenuWidget)
	{
		return;
	}

	MainMenuWidget->AddToViewport();

	// UI만 입력 받도록 + 커서 표시
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void ALCOMainMenuPlayerController::SetupMenuCamera()
{
	TArray<AActor*> Cameras;
	UGameplayStatics::GetAllActorsOfClassWithTag(this, ACameraActor::StaticClass(), MenuCameraTag, Cameras);

	if (Cameras.Num() > 0)
	{
		SetViewTarget(Cameras[0]);
	}
}
