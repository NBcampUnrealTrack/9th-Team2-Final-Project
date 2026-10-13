#include "UI/LCOCameraUISubsystem.h"

#include "CameraCapture/LCOCameraCaptureComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/LCOCameraUIWidget.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "UObject/UObjectGlobals.h"

void ULCOCameraUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateCameraUI(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCOCameraUISubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupCameraUI();

	Super::Deinitialize();
}

void ULCOCameraUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupCameraUI();
	TryCreateCameraUI(NewPlayerController);
}

// ---------- 맵 이동 ----------

void ULCOCameraUISubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (CameraUIWidget && CameraUIWidget->GetWorld() == World)
	{
		CleanupCameraUI();
	}
}

void ULCOCameraUISubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateCameraUI(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

// ---------- 생성 / 정리 ----------

void ULCOCameraUISubsystem::TryCreateCameraUI(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldCreateForWorld(PlayerController->GetWorld()))
	{
		return;
	}

	// 이미 같은 컨트롤러용으로 만들어져 있으면 바인딩만 갱신
	if (CameraUIWidget && CameraUIWidget->GetOwningPlayer() == PlayerController)
	{
		BindToPlayerController(PlayerController);
		return;
	}

	CleanupCameraUI();

	if (CameraUIWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOCameraUISubsystem] CameraUIWidgetClass 미설정 - DefaultGame.ini 확인"));
		return;
	}

	TSubclassOf<ULCOCameraUIWidget> LoadedClass = CameraUIWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOCameraUISubsystem] CameraUIWidgetClass 로드 실패: %s"), *CameraUIWidgetClass.ToString());
		return;
	}

	CameraUIWidget = CreateWidget<ULCOCameraUIWidget>(PlayerController, LoadedClass);
	if (!CameraUIWidget)
	{
		return;
	}

	// 항상 화면에 올려두고, 내용(팝업/문구)만 위젯 안에서 보이기/숨기기
	CameraUIWidget->AddToViewport(ViewportZOrder);

	BindToPlayerController(PlayerController);
}

void ULCOCameraUISubsystem::CleanupCameraUI()
{
	UnbindFromCameraComponent();
	UnbindFromPlayerController();

	if (!CameraUIWidget)
	{
		return;
	}

	CameraUIWidget->UnbindCloseInput();
	CameraUIWidget->RemoveFromParent();
	CameraUIWidget = nullptr;
}

// ---------- 바인딩 ----------

void ULCOCameraUISubsystem::BindToPlayerController(APlayerController* PlayerController)
{
	if (BoundPlayerController.Get() != PlayerController)
	{
		UnbindFromPlayerController();
	}

	if (!PlayerController)
	{
		return;
	}

	// 캐릭터가 바뀌면(리스폰 / 맵 이동 후 빙의) 다시 바인딩
	PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	PlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	BoundPlayerController = PlayerController;

	BindToPawn(PlayerController->GetPawn());
}

void ULCOCameraUISubsystem::UnbindFromPlayerController()
{
	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	BoundPlayerController = nullptr;
}

void ULCOCameraUISubsystem::BindToPawn(APawn* Pawn)
{
	ULCOCameraCaptureComponent* CameraComponent = Pawn ? Pawn->FindComponentByClass<ULCOCameraCaptureComponent>() : nullptr;
	ULCOQuickSlotComponent* QuickSlotComponent = Pawn ? Pawn->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr;

	if (BoundCameraComponent.Get() == CameraComponent && BoundQuickSlotComponent.Get() == QuickSlotComponent)
	{
		return;
	}

	UnbindFromCameraComponent();

	if (CameraComponent)
	{
		CameraComponent->OnPhotoCaptured.AddDynamic(this, &ThisClass::HandlePhotoCaptured);
		CameraComponent->OnCaptureFailed.AddDynamic(this, &ThisClass::HandleCaptureFailed);
		BoundCameraComponent = CameraComponent;
	}

	// 촬영 모드 (퀵슬롯 1번 카메라 들기 / 맨손)
	if (QuickSlotComponent)
	{
		QuickSlotComponent->OnCameraEquipChanged.AddDynamic(this, &ThisClass::HandleCameraEquipChanged);
		BoundQuickSlotComponent = QuickSlotComponent;
	}

	if (CameraUIWidget)
	{
		CameraUIWidget->SetCameraMode(QuickSlotComponent && QuickSlotComponent->IsCameraEquipped());
	}
}

void ULCOCameraUISubsystem::UnbindFromCameraComponent()
{
	if (ULCOCameraCaptureComponent* CameraComponent = BoundCameraComponent.Get())
	{
		CameraComponent->OnPhotoCaptured.RemoveDynamic(this, &ThisClass::HandlePhotoCaptured);
		CameraComponent->OnCaptureFailed.RemoveDynamic(this, &ThisClass::HandleCaptureFailed);
	}

	if (ULCOQuickSlotComponent* QuickSlotComponent = BoundQuickSlotComponent.Get())
	{
		QuickSlotComponent->OnCameraEquipChanged.RemoveDynamic(this, &ThisClass::HandleCameraEquipChanged);
	}

	BoundCameraComponent = nullptr;
	BoundQuickSlotComponent = nullptr;
}

void ULCOCameraUISubsystem::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

// ---------- 카메라 이벤트 → 위젯 ----------

void ULCOCameraUISubsystem::HandlePhotoCaptured(FName CaptureID, UTexture2D* PhotoImage, FText PhotoTitle, FText PhotoDescription)
{
	if (CameraUIWidget)
	{
		CameraUIWidget->ShowPhoto(CaptureID, PhotoImage, PhotoTitle, PhotoDescription);
	}
}

void ULCOCameraUISubsystem::HandleCaptureFailed(ELCOCameraCaptureResult Result)
{
	if (CameraUIWidget)
	{
		CameraUIWidget->ShowCaptureFailed(Result);
	}
}

void ULCOCameraUISubsystem::HandleCameraEquipChanged(bool bEquipped)
{
	if (CameraUIWidget)
	{
		CameraUIWidget->SetCameraMode(bEquipped);
	}
}

// ---------- 내부 ----------

bool ULCOCameraUISubsystem::ShouldCreateForWorld(const UWorld* World) const
{
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	const FString MapName = UWorld::RemovePIEPrefix(World->GetMapName());
	if (MapName.IsEmpty())
	{
		return false;
	}

	for (const FName& ExcludedMapName : ExcludedMapNames)
	{
		if (MapName.Equals(ExcludedMapName.ToString(), ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	return true;
}
