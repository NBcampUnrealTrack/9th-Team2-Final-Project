#include "UI/LCOSenseUISubsystem.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/LCOPlayerCharacter.h"
#include "TimerManager.h"
#include "UI/LCOSenseWidget.h"
#include "UI/LCOMonologueSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

ULCOSenseUISubsystem::ULCOSenseUISubsystem()
{
	SenseInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Characters/Input/IA_Sense.IA_Sense")));
	ExcludedMapNames.Add(TEXT("MainMenu_Test"));
	ExcludedMapNames.Add(TEXT("PauseMenu_Test"));
}

void ULCOSenseUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateSenseUI(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCOSenseUISubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupSenseUI();
	UnbindFromPlayerController();
	CleanupSensePostProcess();

	Super::Deinitialize();
}

void ULCOSenseUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupSenseUI();
	UnbindFromPlayerController();
	TryCreateSenseUI(NewPlayerController);
}

void ULCOSenseUISubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (SenseWidget && SenseWidget->GetWorld() == World)
	{
		CleanupSenseUI();
	}

	if (SensePostProcessMID && (!SensePostProcessCameraComponent.IsValid() || SensePostProcessCameraComponent->GetWorld() == World))
	{
		CleanupSensePostProcess();
	}
}

void ULCOSenseUISubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateSenseUI(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

void ULCOSenseUISubsystem::TryCreateSenseUI(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldCreateForWorld(PlayerController->GetWorld()))
	{
		return;
	}

	if (SenseWidget && SenseWidget->GetOwningPlayer() == PlayerController)
	{
		BindToPlayerController(PlayerController);
		return;
	}

	CleanupSenseUI();
	BindToPlayerController(PlayerController);

	if (SenseWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] SenseWidgetClass is not configured"));
		return;
	}

	TSubclassOf<ULCOSenseWidget> LoadedClass = SenseWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Failed to load SenseWidgetClass: %s"), *SenseWidgetClass.ToString());
		return;
	}

	SenseWidget = CreateWidget<ULCOSenseWidget>(PlayerController, LoadedClass);
	if (SenseWidget)
	{
		SenseWidget->AddToViewport(ViewportZOrder);
		SenseWidget->BindToCharacter(BoundCharacter.Get());
	}
}

void ULCOSenseUISubsystem::CleanupSenseUI()
{
	if (SenseWidget)
	{
		SenseWidget->UnbindFromCharacter();
		SenseWidget->RemoveFromParent();
		SenseWidget = nullptr;
	}

	UnbindFromCharacter();
}

void ULCOSenseUISubsystem::BindToPlayerController(APlayerController* PlayerController)
{
	if (BoundPlayerController.Get() == PlayerController)
	{
		if (SenseStartedBindingHandle == 0 && !BindSenseInput(PlayerController))
		{
			ScheduleSenseInputBindRetry();
		}
		BindToPawn(PlayerController ? PlayerController->GetPawn() : nullptr);
		return;
	}

	UnbindFromPlayerController();
	BoundPlayerController = PlayerController;

	if (PlayerController)
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		PlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);
		if (!BindSenseInput(PlayerController))
		{
			ScheduleSenseInputBindRetry();
		}
		BindToPawn(PlayerController->GetPawn());
	}
}

void ULCOSenseUISubsystem::UnbindFromPlayerController()
{
	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	UnbindSenseInput();
	BoundPlayerController.Reset();
	UnbindFromCharacter();
	CleanupSensePostProcess();
}

void ULCOSenseUISubsystem::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void ULCOSenseUISubsystem::BindToPawn(APawn* Pawn)
{
	ALCOPlayerCharacter* Character = Cast<ALCOPlayerCharacter>(Pawn);
	if (BoundCharacter.Get() == Character)
	{
		if (SenseWidget)
		{
			SenseWidget->BindToCharacter(Character);
		}
		return;
	}

	CleanupSensePostProcess();
	UnbindFromCharacter();
	BoundCharacter = Character;

	if (Character)
	{
		Character->OnSenseUsed.RemoveDynamic(this, &ThisClass::HandleSenseUsed);
		Character->OnSenseUsed.AddDynamic(this, &ThisClass::HandleSenseUsed);
		UE_LOG(LogTemp, Log, TEXT("[LCOSenseUISubsystem] Bound OnSenseUsed to %s"), *GetNameSafe(Character));
	}

	if (SenseWidget)
	{
		SenseWidget->BindToCharacter(Character);
	}
}

void ULCOSenseUISubsystem::UnbindFromCharacter()
{
	if (ALCOPlayerCharacter* Character = BoundCharacter.Get())
	{
		Character->OnSenseUsed.RemoveDynamic(this, &ThisClass::HandleSenseUsed);
	}

	BoundCharacter.Reset();
}

void ULCOSenseUISubsystem::UnbindSenseInput()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SenseInputBindRetryTimer);
	}

	APlayerController* PlayerController = BoundPlayerController.Get();
	UEnhancedInputComponent* EIC = PlayerController ? Cast<UEnhancedInputComponent>(PlayerController->InputComponent) : nullptr;
	if (EIC)
	{
		if (SenseStartedBindingHandle != 0)
		{
			EIC->RemoveBindingByHandle(SenseStartedBindingHandle);
		}
		if (SenseCompletedBindingHandle != 0)
		{
			EIC->RemoveBindingByHandle(SenseCompletedBindingHandle);
		}
		if (SenseCanceledBindingHandle != 0)
		{
			EIC->RemoveBindingByHandle(SenseCanceledBindingHandle);
		}
	}

	SenseStartedBindingHandle = 0;
	SenseCompletedBindingHandle = 0;
	SenseCanceledBindingHandle = 0;
}

bool ULCOSenseUISubsystem::BindSenseInput(APlayerController* PlayerController)
{
	if (SenseStartedBindingHandle != 0)
	{
		return true;
	}

	if (!PlayerController)
	{
		return false;
	}

	if (SenseInputAction.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] SenseInputAction is not configured"));
		return false;
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerController->InputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[LCOSenseUISubsystem] EnhancedInputComponent not ready; retrying sense input bind"));
		return false;
	}

	UInputAction* LoadedAction = SenseInputAction.LoadSynchronous();
	if (!LoadedAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Failed to load SenseInputAction: %s"), *SenseInputAction.ToString());
		return false;
	}

	SenseStartedBindingHandle = EIC->BindAction(LoadedAction, ETriggerEvent::Started, this, &ThisClass::HandleSenseInputStarted).GetHandle();
	SenseCompletedBindingHandle = EIC->BindAction(LoadedAction, ETriggerEvent::Completed, this, &ThisClass::HandleSenseInputReleased).GetHandle();
	SenseCanceledBindingHandle = EIC->BindAction(LoadedAction, ETriggerEvent::Canceled, this, &ThisClass::HandleSenseInputReleased).GetHandle();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SenseInputBindRetryTimer);
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOSenseUISubsystem] Sense input bound"));
	return true;
}

void ULCOSenseUISubsystem::ScheduleSenseInputBindRetry()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(SenseInputBindRetryTimer))
	{
		return;
	}

	World->GetTimerManager().SetTimer(SenseInputBindRetryTimer, this, &ThisClass::HandleSenseInputBindRetry, 0.05f, false);
}

void ULCOSenseUISubsystem::HandleSenseInputBindRetry()
{
	APlayerController* PlayerController = BoundPlayerController.Get();
	if (!PlayerController || SenseStartedBindingHandle != 0)
	{
		return;
	}

	if (!BindSenseInput(PlayerController))
	{
		ScheduleSenseInputBindRetry();
	}
}

void ULCOSenseUISubsystem::HandleSenseInputStarted()
{
	if (SenseWidget)
	{
		SenseWidget->NotifySenseInputStarted();
	}

	RefreshHoldStateNextTick();
}

void ULCOSenseUISubsystem::HandleSenseInputReleased()
{
	if (SenseWidget)
	{
		SenseWidget->NotifySenseInputReleased();
	}
}

void ULCOSenseUISubsystem::RefreshHoldStateNextTick()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (SenseWidget)
			{
				SenseWidget->RefreshHoldState();
			}
		}));
	}
}

void ULCOSenseUISubsystem::HandleSenseUsed(bool bSuccess)
{
	UE_LOG(LogTemp, Log, TEXT("[LCOSenseUISubsystem] OnSenseUsed received. Success=%s"), bSuccess ? TEXT("true") : TEXT("false"));
	StartSensePostProcessEffect();

	// 실패 독백 (성공 독백은 감각 볼륨이 볼륨별 문구로 표시)
	if (!bSuccess && !SenseFailMonologueKey.IsNone())
	{
		ULCOMonologueSubsystem::ShowMonologueByKey(BoundPlayerController.Get(), SenseFailMonologueKey);
	}
}

void ULCOSenseUISubsystem::StartSensePostProcessEffect()
{
	APlayerController* PlayerController = BoundPlayerController.Get();
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Cannot start sense post process: no bound player controller"));
		return;
	}

	EnsureSensePostProcess(PlayerController);
	if (!SensePostProcessMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Cannot start sense post process: MID was not created"));
		return;
	}
	if (!SensePostProcessCameraComponent.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Cannot start sense post process: active CameraComponent was not found"));
		return;
	}

	EffectElapsedTime = 0.f;
	EffectStartStrength = CurrentEffectStrength;
	ZoomStartStrength = CurrentZoomStrength;
	bSenseEffectActive = true;
	const AActor* ScanOriginActor = PlayerController->GetPawn();
	if (!ScanOriginActor)
	{
		ScanOriginActor = BoundCharacter.Get();
	}
	if (ScanOriginActor)
	{
		const FVector ScanOrigin = ScanOriginActor->GetActorLocation();
		SensePostProcessMID->SetVectorParameterValue(ScanOriginParameterName, FLinearColor(ScanOrigin.X, ScanOrigin.Y, ScanOrigin.Z, 1.f));
	}
	SensePostProcessMID->SetScalarParameterValue(ScanProgressParameterName, 0.f);
	SetSensePostProcessStrength(CurrentEffectStrength);
	SetSensePostProcessZoomStrength(CurrentZoomStrength);
	UE_LOG(LogTemp, Log, TEXT("[LCOSenseUISubsystem] Sense post process effect started"));
}

void ULCOSenseUISubsystem::UpdateSensePostProcessEffect(float DeltaTime)
{
	if (!bSenseEffectActive)
	{
		return;
	}

	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		EnsureSensePostProcess(PlayerController);
	}

	if (!SensePostProcessMID || !SensePostProcessCameraComponent.IsValid())
	{
		return;
	}

	EffectElapsedTime += DeltaTime;
	const float ScanProgress = FMath::Clamp(EffectElapsedTime / FMath::Max(ScanDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	SensePostProcessMID->SetScalarParameterValue(ScanProgressParameterName, ScanProgress);

	const float ClampedDuration = FMath::Max(EffectDuration, KINDA_SMALL_NUMBER);
	const float ClampedEnterTime = FMath::Min(FMath::Max(EffectEnterTime, KINDA_SMALL_NUMBER), ClampedDuration);
	const float WaveEndTime = FMath::Min(ClampedEnterTime + FMath::Max(EffectWaveTime, 0.f), ClampedDuration);
	const float DecayDuration = FMath::Max(ClampedDuration - WaveEndTime, KINDA_SMALL_NUMBER);

	float NewStrength = EffectMaxStrength;
	float NewZoomStrength = 1.f;
	if (EffectElapsedTime < ClampedEnterTime)
	{
		const float Alpha = FMath::Clamp(EffectElapsedTime / ClampedEnterTime, 0.f, 1.f);
		NewStrength = FMath::Lerp(EffectStartStrength, EffectMaxStrength, Alpha);
		NewZoomStrength = FMath::Lerp(ZoomStartStrength, 1.f, Alpha);
	}
	else if (EffectElapsedTime < WaveEndTime)
	{
		const float WaveAlpha = FMath::Clamp((EffectElapsedTime - ClampedEnterTime) / FMath::Max(WaveEndTime - ClampedEnterTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
		const float Wave = FMath::Square(FMath::Sin(PI * WaveAlpha));
		NewZoomStrength = 1.f - (EffectWaveAmplitudeRatio * Wave);
	}
	else
	{
		const float Alpha = FMath::Clamp((EffectElapsedTime - WaveEndTime) / DecayDuration, 0.f, 1.f);
		const float EaseOutAlpha = 1.f - FMath::Square(1.f - Alpha);
		NewStrength = FMath::Lerp(EffectMaxStrength, 0.f, EaseOutAlpha);
		NewZoomStrength = FMath::Lerp(1.f, 0.f, EaseOutAlpha);
	}

	SetSensePostProcessStrength(NewStrength);
	SetSensePostProcessZoomStrength(NewZoomStrength);

	if (EffectElapsedTime >= ClampedDuration)
	{
		bSenseEffectActive = false;
		SetSensePostProcessStrength(0.f);
		SetSensePostProcessZoomStrength(0.f);
		RemoveSensePostProcessBlendable();
	}
}

void ULCOSenseUISubsystem::EnsureSensePostProcess(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Cannot create sense post process: PlayerController is null"));
		return;
	}

	if (!SensePostProcessMID)
	{
		if (SensePostProcessMaterial.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Cannot create sense post process: SensePostProcessMaterial is not configured"));
			return;
		}

		UMaterialInterface* LoadedMaterial = SensePostProcessMaterial.LoadSynchronous();
		if (!LoadedMaterial)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Failed to load SensePostProcessMaterial: %s"), *SensePostProcessMaterial.ToString());
			return;
		}

		SensePostProcessMID = UMaterialInstanceDynamic::Create(LoadedMaterial, this);
		if (!SensePostProcessMID)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Failed to create sense post process MID from material: %s"), *GetNameSafe(LoadedMaterial));
			return;
		}

		SensePostProcessMID->SetScalarParameterValue(EffectStrengthParameterName, 0.f);
		SensePostProcessMID->SetScalarParameterValue(ZoomStrengthParameterName, 0.f);
		SensePostProcessMID->SetScalarParameterValue(ScanProgressParameterName, 1.f);
	}

	UCameraComponent* ActiveCameraComponent = FindActiveCameraComponent(PlayerController);
	if (!ActiveCameraComponent)
	{
		RemoveSensePostProcessBlendable();
		UE_LOG(LogTemp, Warning, TEXT("[LCOSenseUISubsystem] Cannot apply sense post process: active CameraComponent was not found on ViewTarget"));
		return;
	}

	const bool bCameraChanged = SensePostProcessCameraComponent.Get() != ActiveCameraComponent;
	if (bCameraChanged)
	{
		RemoveSensePostProcessBlendable();
		SensePostProcessCameraComponent = ActiveCameraComponent;
	}
	SetSensePostProcessBlendableWeight(CurrentEffectStrength > 0.f ? 1.f : 0.f);
}

void ULCOSenseUISubsystem::CleanupSensePostProcess()
{
	bSenseEffectActive = false;
	EffectElapsedTime = 0.f;
	EffectStartStrength = 0.f;
	ZoomStartStrength = 0.f;
	CurrentEffectStrength = 0.f;
	CurrentZoomStrength = 0.f;

	if (SensePostProcessMID)
	{
		SensePostProcessMID->SetScalarParameterValue(EffectStrengthParameterName, 0.f);
		SensePostProcessMID->SetScalarParameterValue(ZoomStrengthParameterName, 0.f);
	}

	RemoveSensePostProcessBlendable();
	SensePostProcessMID = nullptr;
}

UCameraComponent* ULCOSenseUISubsystem::FindActiveCameraComponent(APlayerController* PlayerController) const
{
	if (!PlayerController)
	{
		return nullptr;
	}

	AActor* ViewTarget = PlayerController->GetViewTarget();
	if (!ViewTarget)
	{
		return nullptr;
	}

	TArray<UCameraComponent*> CameraComponents;
	ViewTarget->GetComponents<UCameraComponent>(CameraComponents);

	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		if (!CameraComponent)
		{
			continue;
		}

		if (CameraComponent->IsRegistered() && CameraComponent->IsActive())
		{
			return CameraComponent;
		}
	}

	return nullptr;
}

void ULCOSenseUISubsystem::RemoveSensePostProcessBlendable()
{
	UCameraComponent* CameraComponent = SensePostProcessCameraComponent.Get();
	if (!CameraComponent || !SensePostProcessMID)
	{
		SensePostProcessCameraComponent.Reset();
		return;
	}

	CameraComponent->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
		[this](const FWeightedBlendable& Blendable)
		{
			return Blendable.Object == SensePostProcessMID;
		});

	SensePostProcessCameraComponent.Reset();
}

void ULCOSenseUISubsystem::SetSensePostProcessBlendableWeight(float Weight)
{
	UCameraComponent* CameraComponent = SensePostProcessCameraComponent.Get();
	if (!CameraComponent || !SensePostProcessMID)
	{
		return;
	}

	bool bFoundBlendable = false;
	for (FWeightedBlendable& Blendable : CameraComponent->PostProcessSettings.WeightedBlendables.Array)
	{
		if (Blendable.Object == SensePostProcessMID)
		{
			Blendable.Weight = Weight;
			bFoundBlendable = true;
		}
	}

	if (!bFoundBlendable)
	{
		CameraComponent->PostProcessSettings.AddBlendable(SensePostProcessMID, Weight);
	}
}

void ULCOSenseUISubsystem::SetSensePostProcessStrength(float Strength)
{
	CurrentEffectStrength = FMath::Clamp(Strength, 0.f, EffectMaxStrength);

	if (SensePostProcessMID)
	{
		SensePostProcessMID->SetScalarParameterValue(EffectStrengthParameterName, CurrentEffectStrength);
	}

	SetSensePostProcessBlendableWeight(CurrentEffectStrength > 0.f ? 1.f : 0.f);
}

void ULCOSenseUISubsystem::SetSensePostProcessZoomStrength(float Strength)
{
	const float MaxZoomStrength = 1.f + FMath::Max(EffectWaveAmplitudeRatio, 0.f);
	CurrentZoomStrength = FMath::Clamp(Strength, 0.f, MaxZoomStrength);

	if (SensePostProcessMID)
	{
		SensePostProcessMID->SetScalarParameterValue(ZoomStrengthParameterName, CurrentZoomStrength);
	}
}

bool ULCOSenseUISubsystem::ShouldCreateForWorld(const UWorld* World) const
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

void ULCOSenseUISubsystem::Tick(float DeltaTime)
{
	UpdateSensePostProcessEffect(DeltaTime);
}

TStatId ULCOSenseUISubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULCOSenseUISubsystem, STATGROUP_Tickables);
}

bool ULCOSenseUISubsystem::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && bSenseEffectActive;
}
