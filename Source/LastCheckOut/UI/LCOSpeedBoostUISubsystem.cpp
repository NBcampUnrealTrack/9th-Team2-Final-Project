#include "UI/LCOSpeedBoostUISubsystem.h"

#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/LCOPlayerCharacter.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	constexpr float SpeedBoostFallbackFadeOutTime = 0.5f;
}

ULCOSpeedBoostUISubsystem::ULCOSpeedBoostUISubsystem()
{
	SpeedBoostPostProcessMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/HJ/Material_Test/M_PP_SpeedBoost.M_PP_SpeedBoost")));
}

void ULCOSpeedBoostUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		BindToPlayerController(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCOSpeedBoostUISubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);

	UnbindFromPlayerController();
	CleanupSpeedBoostPostProcess();

	Super::Deinitialize();
}

void ULCOSpeedBoostUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	UnbindFromPlayerController();
	BindToPlayerController(NewPlayerController);
}

void ULCOSpeedBoostUISubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (SpeedBoostPostProcessMID && (!SpeedBoostPostProcessCameraComponent.IsValid() || SpeedBoostPostProcessCameraComponent->GetWorld() == World))
	{
		CleanupSpeedBoostPostProcess();
	}
}

void ULCOSpeedBoostUISubsystem::BindToPlayerController(APlayerController* PlayerController)
{
	if (PlayerController && !PlayerController->IsLocalPlayerController())
	{
		UnbindFromPlayerController();
		return;
	}

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

void ULCOSpeedBoostUISubsystem::UnbindFromPlayerController()
{
	if (APlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	BoundPlayerController.Reset();
	UnbindFromCharacter();
	CleanupSpeedBoostPostProcess();
}

void ULCOSpeedBoostUISubsystem::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void ULCOSpeedBoostUISubsystem::BindToPawn(APawn* Pawn)
{
	ALCOPlayerCharacter* Character = Cast<ALCOPlayerCharacter>(Pawn);
	if (BoundCharacter.Get() == Character)
	{
		return;
	}

	UnbindFromCharacter();
	BoundCharacter = Character;
	CleanupSpeedBoostPostProcess();
}

void ULCOSpeedBoostUISubsystem::UnbindFromCharacter()
{
	BoundCharacter.Reset();
}

void ULCOSpeedBoostUISubsystem::Tick(float DeltaTime)
{
	if (!BoundPlayerController.IsValid())
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			BindToPlayerController(LocalPlayer->GetPlayerController(GetWorld()));
		}
	}

	ALCOPlayerCharacter* Character = BoundCharacter.Get();
	if (!Character)
	{
		CurrentEdgeBrightness = 0.f;
		bWasMoveSpeedBuffActive = false;
		bFadingOutAfterBuffEnd = false;
		UpdateSpeedBoostPostProcess(0.f);
		return;
	}

	const float Alpha = ResolveEdgeBrightness(DeltaTime, Character);
	UpdateSpeedBoostPostProcess(Alpha);
}

TStatId ULCOSpeedBoostUISubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULCOSpeedBoostUISubsystem, STATGROUP_Tickables);
}

bool ULCOSpeedBoostUISubsystem::IsTickable() const
{
	return !IsTemplate();
}

float ULCOSpeedBoostUISubsystem::ResolveEdgeBrightness(float DeltaTime, const ALCOPlayerCharacter* Character)
{
	if (!Character)
	{
		CurrentEdgeBrightness = 0.f;
		bWasMoveSpeedBuffActive = false;
		bFadingOutAfterBuffEnd = false;
		return 0.f;
	}

	const bool bMoveSpeedBuffActive = Character->IsMoveSpeedBuffActive();
	const float CharacterAlpha = FMath::Clamp(Character->GetMoveSpeedBuffEffectAlpha(), 0.f, 1.f);

	if (bMoveSpeedBuffActive)
	{
		CurrentEdgeBrightness = CharacterAlpha;
		bFadingOutAfterBuffEnd = false;
	}
	else
	{
		if (bWasMoveSpeedBuffActive)
		{
			CurrentEdgeBrightness = FMath::Max(CurrentEdgeBrightness, CharacterAlpha);
			bFadingOutAfterBuffEnd = true;
		}

		if (bFadingOutAfterBuffEnd)
		{
			const float FadeSpeed = 1.f / SpeedBoostFallbackFadeOutTime;
			CurrentEdgeBrightness = FMath::FInterpConstantTo(CurrentEdgeBrightness, 0.f, DeltaTime, FadeSpeed);
		}
		else
		{
			CurrentEdgeBrightness = CharacterAlpha;
		}
	}

	if (!bMoveSpeedBuffActive && CurrentEdgeBrightness <= KINDA_SMALL_NUMBER)
	{
		CurrentEdgeBrightness = 0.f;
		bFadingOutAfterBuffEnd = false;
	}

	bWasMoveSpeedBuffActive = bMoveSpeedBuffActive;
	return FMath::Clamp(CurrentEdgeBrightness, 0.f, 1.f);
}

void ULCOSpeedBoostUISubsystem::UpdateSpeedBoostPostProcess(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (ClampedAlpha <= 0.f)
	{
		if (SpeedBoostPostProcessMID)
		{
			SpeedBoostPostProcessMID->SetScalarParameterValue(EdgeBrightnessParameterName, 0.f);
		}
		CleanupSpeedBoostPostProcess();
		return;
	}

	APlayerController* PlayerController = BoundPlayerController.Get();
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		CleanupSpeedBoostPostProcess();
		return;
	}

	if (!EnsureSpeedBoostPostProcess(PlayerController))
	{
		return;
	}

	SpeedBoostPostProcessMID->SetScalarParameterValue(EdgeBrightnessParameterName, ClampedAlpha);
	SetSpeedBoostPostProcessBlendableWeight(1.f);
}

bool ULCOSpeedBoostUISubsystem::EnsureSpeedBoostPostProcess(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	if (!SpeedBoostPostProcessMID)
	{
		UMaterialInterface* LoadedMaterial = SpeedBoostPostProcessMaterial.LoadSynchronous();
		if (!LoadedMaterial)
		{
			if (!bWarnedMaterialLoadFailure)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCOSpeedBoostUISubsystem] Failed to load SpeedBoostPostProcessMaterial: %s"), *SpeedBoostPostProcessMaterial.ToString());
				bWarnedMaterialLoadFailure = true;
			}
			return false;
		}

		SpeedBoostPostProcessMID = UMaterialInstanceDynamic::Create(LoadedMaterial, this);
		if (!SpeedBoostPostProcessMID)
		{
			if (!bWarnedMaterialLoadFailure)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCOSpeedBoostUISubsystem] Failed to create speed boost post process MID from material: %s"), *GetNameSafe(LoadedMaterial));
				bWarnedMaterialLoadFailure = true;
			}
			return false;
		}

		SpeedBoostPostProcessMID->SetScalarParameterValue(EdgeBrightnessParameterName, 0.f);
		bWarnedMaterialLoadFailure = false;
	}

	UCameraComponent* ActiveCameraComponent = FindActiveCameraComponent(PlayerController);
	if (!ActiveCameraComponent)
	{
		RemoveSpeedBoostPostProcessBlendable();
		if (!bWarnedNoActiveCamera)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOSpeedBoostUISubsystem] Cannot apply speed boost post process: active CameraComponent was not found on ViewTarget"));
			bWarnedNoActiveCamera = true;
		}
		return false;
	}

	bWarnedNoActiveCamera = false;

	const bool bCameraChanged = SpeedBoostPostProcessCameraComponent.Get() != ActiveCameraComponent;
	if (bCameraChanged)
	{
		RemoveSpeedBoostPostProcessBlendable();
		SpeedBoostPostProcessCameraComponent = ActiveCameraComponent;
	}

	return true;
}

UCameraComponent* ULCOSpeedBoostUISubsystem::FindActiveCameraComponent(APlayerController* PlayerController) const
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
		if (CameraComponent && CameraComponent->IsRegistered() && CameraComponent->IsActive())
		{
			return CameraComponent;
		}
	}

	return nullptr;
}

void ULCOSpeedBoostUISubsystem::RemoveSpeedBoostPostProcessBlendable()
{
	UCameraComponent* CameraComponent = SpeedBoostPostProcessCameraComponent.Get();
	if (!CameraComponent || !SpeedBoostPostProcessMID)
	{
		SpeedBoostPostProcessCameraComponent.Reset();
		return;
	}

	CameraComponent->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
		[this](const FWeightedBlendable& Blendable)
		{
			return Blendable.Object == SpeedBoostPostProcessMID;
		});

	SpeedBoostPostProcessCameraComponent.Reset();
}

void ULCOSpeedBoostUISubsystem::SetSpeedBoostPostProcessBlendableWeight(float Weight)
{
	UCameraComponent* CameraComponent = SpeedBoostPostProcessCameraComponent.Get();
	if (!CameraComponent || !SpeedBoostPostProcessMID)
	{
		return;
	}

	bool bFoundBlendable = false;
	for (FWeightedBlendable& Blendable : CameraComponent->PostProcessSettings.WeightedBlendables.Array)
	{
		if (Blendable.Object == SpeedBoostPostProcessMID)
		{
			Blendable.Weight = Weight;
			bFoundBlendable = true;
			break;
		}
	}

	if (!bFoundBlendable)
	{
		CameraComponent->PostProcessSettings.AddBlendable(SpeedBoostPostProcessMID, Weight);
	}
}

void ULCOSpeedBoostUISubsystem::CleanupSpeedBoostPostProcess()
{
	CurrentEdgeBrightness = 0.f;
	bFadingOutAfterBuffEnd = false;

	if (SpeedBoostPostProcessMID)
	{
		SpeedBoostPostProcessMID->SetScalarParameterValue(EdgeBrightnessParameterName, 0.f);
	}

	RemoveSpeedBoostPostProcessBlendable();
	SpeedBoostPostProcessMID = nullptr;
}
