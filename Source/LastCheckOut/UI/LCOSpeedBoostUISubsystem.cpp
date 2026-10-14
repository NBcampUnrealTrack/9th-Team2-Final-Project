#include "UI/LCOSpeedBoostUISubsystem.h"

#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Scene.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Item/ItemData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/LCOPlayerCharacter.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	constexpr float SpeedBoostFallbackFadeOutTime = 0.5f;

	float SmoothStep01(float Alpha)
	{
		const float ClampedAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
		return ClampedAlpha * ClampedAlpha * (3.f - 2.f * ClampedAlpha);
	}
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
		BindToQuickSlot(Character ? Character->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr);
		return;
	}

	UnbindFromCharacter();
	BoundCharacter = Character;
	BindToQuickSlot(Character ? Character->FindComponentByClass<ULCOQuickSlotComponent>() : nullptr);
	CleanupSpeedBoostPostProcess();
}

void ULCOSpeedBoostUISubsystem::UnbindFromCharacter()
{
	UnbindFromQuickSlot();
	BoundCharacter.Reset();
}

void ULCOSpeedBoostUISubsystem::BindToQuickSlot(ULCOQuickSlotComponent* QuickSlot)
{
	if (BoundQuickSlot.Get() == QuickSlot)
	{
		return;
	}

	UnbindFromQuickSlot();
	BoundQuickSlot = QuickSlot;

	if (QuickSlot)
	{
		QuickSlot->OnQuickSlotItemUsed.RemoveDynamic(this, &ThisClass::HandleQuickSlotItemUsed);
		QuickSlot->OnQuickSlotItemUsed.AddDynamic(this, &ThisClass::HandleQuickSlotItemUsed);
	}
}

void ULCOSpeedBoostUISubsystem::UnbindFromQuickSlot()
{
	if (ULCOQuickSlotComponent* QuickSlot = BoundQuickSlot.Get())
	{
		QuickSlot->OnQuickSlotItemUsed.RemoveDynamic(this, &ThisClass::HandleQuickSlotItemUsed);
	}

	BoundQuickSlot.Reset();
}

void ULCOSpeedBoostUISubsystem::ResetBuffTracking()
{
	RecordedMoveSpeedAmount = 0.f;
	RecordedMaxStaminaAmount = 0.f;
	CurrentEdgeBrightness = 0.f;
	CurrentBrightnessAlpha = 0.f;
	BrightnessTransitionStartAlpha = 0.f;
	BrightnessTransitionTargetAlpha = 0.f;
	BrightnessTransitionElapsed = 0.f;
	CurrentMoveSpeedContribution = 0.f;
	CurrentMaxStaminaContribution = 0.f;
	CurrentGlowColor = FLinearColor::Black;
	bWasMoveSpeedBuffActive = false;
	bWasMaxStaminaBuffActive = false;
	bMoveSpeedFadingOutAfterBuffEnd = false;
	bMaxStaminaFadingOutAfterBuffEnd = false;
}

void ULCOSpeedBoostUISubsystem::HandleQuickSlotItemUsed(int32 SlotIndex, FName ItemID)
{
	(void)SlotIndex;

	ULCOQuickSlotComponent* QuickSlot = BoundQuickSlot.Get();
	if (!QuickSlot || ItemID.IsNone())
	{
		return;
	}

	const UInventoryComponent* Inventory = QuickSlot->GetInventory();
	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;
	if (!ItemData)
	{
		return;
	}

	for (const FItemBuffEffect& Effect : ItemData->BuffEffects)
	{
		switch (Effect.BuffType)
		{
		case EBuffType::MoveSpeed:
			RecordedMoveSpeedAmount = FMath::Max(0.f, Effect.BuffAmount);
			bMoveSpeedFadingOutAfterBuffEnd = false;
			break;

		case EBuffType::MaxStamina:
			RecordedMaxStaminaAmount = FMath::Max(0.f, Effect.BuffAmount);
			bMaxStaminaFadingOutAfterBuffEnd = false;
			break;

		case EBuffType::None:
		default:
			break;
		}
	}
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
		ResetBuffTracking();
		UpdateSpeedBoostPostProcess(FBuffPostProcessState());
		return;
	}

	const FBuffPostProcessState State = ResolveBuffPostProcessState(DeltaTime, Character);
	UpdateSpeedBoostPostProcess(State);
}

TStatId ULCOSpeedBoostUISubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULCOSpeedBoostUISubsystem, STATGROUP_Tickables);
}

bool ULCOSpeedBoostUISubsystem::IsTickable() const
{
	return !IsTemplate();
}

ULCOSpeedBoostUISubsystem::FBuffPostProcessState ULCOSpeedBoostUISubsystem::ResolveBuffPostProcessState(float DeltaTime, const ALCOPlayerCharacter* Character)
{
	if (!Character)
	{
		ResetBuffTracking();
		return FBuffPostProcessState();
	}

	const bool bMoveSpeedBuffActive = Character->IsMoveSpeedBuffActive();
	const bool bMaxStaminaBuffActive = Character->IsMaxStaminaBuffActive();
	const float CharacterAlpha = FMath::Clamp(Character->GetMoveSpeedBuffEffectAlpha(), 0.f, 1.f);
	const float MaxStaminaAlpha = FMath::Clamp(Character->GetMaxStaminaBuffEffectAlpha(), 0.f, 1.f);

	const float MoveSpeedContribution = ResolveMoveSpeedContribution(DeltaTime, Character, bMoveSpeedBuffActive, CharacterAlpha);
	const float MaxStaminaContribution = ResolveMaxStaminaContribution(DeltaTime, Character, bMaxStaminaBuffActive, MaxStaminaAlpha);
	const float TargetBrightness = FMath::Clamp(MoveSpeedContribution + MaxStaminaContribution, 0.f, 1.f);

	const bool bMoveSpeedVisible = bMoveSpeedBuffActive || MoveSpeedContribution > KINDA_SMALL_NUMBER;
	const bool bMaxStaminaVisible = bMaxStaminaBuffActive || MaxStaminaContribution > KINDA_SMALL_NUMBER;
	const bool bAnyBuffActive = bMoveSpeedBuffActive || bMaxStaminaBuffActive;
	const FLinearColor TargetGlowColor = ResolveTargetGlowColor(bMoveSpeedVisible, bMaxStaminaVisible);
	const float ColorInterpSpeed = 1.f / FMath::Max(ColorInterpTime, KINDA_SMALL_NUMBER);

	CurrentEdgeBrightness = TargetBrightness;
	const float BrightnessAlpha = UpdateBrightnessAlpha(DeltaTime, bAnyBuffActive);
	CurrentGlowColor = FMath::CInterpTo(CurrentGlowColor, TargetGlowColor, DeltaTime, ColorInterpSpeed);

	bWasMoveSpeedBuffActive = bMoveSpeedBuffActive;
	bWasMaxStaminaBuffActive = bMaxStaminaBuffActive;

	FBuffPostProcessState State;
	State.EdgeBrightness = CurrentEdgeBrightness;
	State.BrightnessAlpha = BrightnessAlpha;
	State.GlowColor = CurrentGlowColor;
	State.bHasVisibleBuff = bMoveSpeedVisible
		|| bMaxStaminaVisible
		|| CurrentEdgeBrightness > KINDA_SMALL_NUMBER
		|| BrightnessAlpha > KINDA_SMALL_NUMBER;
	State.bAnyBuffActive = bAnyBuffActive;
	return State;
}

float ULCOSpeedBoostUISubsystem::ResolveMoveSpeedContribution(float DeltaTime, const ALCOPlayerCharacter* Character, bool bMoveSpeedBuffActive, float MoveSpeedAlpha)
{
	(void)Character;

	if (bMoveSpeedBuffActive)
	{
		CurrentMoveSpeedContribution = FMath::Clamp(RecordedMoveSpeedAmount / MoveSpeedFullBrightnessBonus, 0.f, 1.f) * MoveSpeedAlpha;
		bMoveSpeedFadingOutAfterBuffEnd = false;
	}
	else
	{
		if (bWasMoveSpeedBuffActive)
		{
			bMoveSpeedFadingOutAfterBuffEnd = true;
		}

		if (bMoveSpeedFadingOutAfterBuffEnd)
		{
			const float FadeSpeed = 1.f / SpeedBoostFallbackFadeOutTime;
			CurrentMoveSpeedContribution = FMath::FInterpConstantTo(CurrentMoveSpeedContribution, 0.f, DeltaTime, FadeSpeed);
		}
	}

	if (!bMoveSpeedBuffActive && CurrentMoveSpeedContribution <= KINDA_SMALL_NUMBER)
	{
		CurrentMoveSpeedContribution = 0.f;
		RecordedMoveSpeedAmount = 0.f;
		bMoveSpeedFadingOutAfterBuffEnd = false;
	}

	return FMath::Clamp(CurrentMoveSpeedContribution, 0.f, 1.f);
}

float ULCOSpeedBoostUISubsystem::ResolveMaxStaminaContribution(float DeltaTime, const ALCOPlayerCharacter* Character, bool bMaxStaminaBuffActive, float MaxStaminaAlpha)
{
	float MaxStaminaContribution = CurrentMaxStaminaContribution;

	if (Character && bMaxStaminaBuffActive)
	{
		MaxStaminaContribution = FMath::Clamp(RecordedMaxStaminaAmount / MaxStaminaFullBrightnessBonus, 0.f, 1.f) * MaxStaminaAlpha;
		bMaxStaminaFadingOutAfterBuffEnd = false;
	}
	else
	{
		if (bWasMaxStaminaBuffActive)
		{
			bMaxStaminaFadingOutAfterBuffEnd = true;
		}

		if (bMaxStaminaFadingOutAfterBuffEnd)
		{
			const float FadeSpeed = 1.f / SpeedBoostFallbackFadeOutTime;
			MaxStaminaContribution = FMath::FInterpConstantTo(MaxStaminaContribution, 0.f, DeltaTime, FadeSpeed);
		}
		else
		{
			MaxStaminaContribution = 0.f;
		}
	}

	if (!bMaxStaminaBuffActive && MaxStaminaContribution <= KINDA_SMALL_NUMBER)
	{
		MaxStaminaContribution = 0.f;
		RecordedMaxStaminaAmount = 0.f;
		bMaxStaminaFadingOutAfterBuffEnd = false;
	}

	CurrentMaxStaminaContribution = FMath::Clamp(MaxStaminaContribution, 0.f, 1.f);
	return CurrentMaxStaminaContribution;
}

float ULCOSpeedBoostUISubsystem::UpdateBrightnessAlpha(float DeltaTime, bool bAnyBuffActive)
{
	const float DesiredTargetAlpha = bAnyBuffActive ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(BrightnessTransitionTargetAlpha, DesiredTargetAlpha, KINDA_SMALL_NUMBER))
	{
		BrightnessTransitionStartAlpha = CurrentBrightnessAlpha;
		BrightnessTransitionTargetAlpha = DesiredTargetAlpha;
		BrightnessTransitionElapsed = 0.f;
	}

	if (FMath::IsNearlyEqual(CurrentBrightnessAlpha, BrightnessTransitionTargetAlpha, KINDA_SMALL_NUMBER))
	{
		CurrentBrightnessAlpha = BrightnessTransitionTargetAlpha;
		return CurrentBrightnessAlpha;
	}

	BrightnessTransitionElapsed += FMath::Max(DeltaTime, 0.f);
	const float Duration = FMath::Max(BrightnessTransitionTime, KINDA_SMALL_NUMBER);
	const float SmoothAlpha = SmoothStep01(BrightnessTransitionElapsed / Duration);
	CurrentBrightnessAlpha = FMath::Lerp(BrightnessTransitionStartAlpha, BrightnessTransitionTargetAlpha, SmoothAlpha);

	if (SmoothAlpha >= 1.f - KINDA_SMALL_NUMBER)
	{
		CurrentBrightnessAlpha = BrightnessTransitionTargetAlpha;
	}

	return FMath::Clamp(CurrentBrightnessAlpha, 0.f, 1.f);
}

FLinearColor ULCOSpeedBoostUISubsystem::ResolveTargetGlowColor(bool bMoveSpeedVisible, bool bMaxStaminaVisible) const
{
	if (bMoveSpeedVisible && bMaxStaminaVisible)
	{
		return CombinedGlowColor;
	}

	if (bMoveSpeedVisible)
	{
		return MoveSpeedGlowColor;
	}

	if (bMaxStaminaVisible)
	{
		return MaxStaminaGlowColor;
	}

	return FLinearColor::Black;
}

void ULCOSpeedBoostUISubsystem::UpdateSpeedBoostPostProcess(const FBuffPostProcessState& State)
{
	const float ClampedBrightness = FMath::Clamp(State.EdgeBrightness, 0.f, 1.f);
	const float ClampedBrightnessAlpha = FMath::Clamp(State.BrightnessAlpha, 0.f, 1.f);
	if (!State.bHasVisibleBuff && ClampedBrightness <= 0.f && ClampedBrightnessAlpha <= 0.f)
	{
		if (SpeedBoostPostProcessMID)
		{
			SpeedBoostPostProcessMID->SetScalarParameterValue(EdgeBrightnessParameterName, 0.f);
			SpeedBoostPostProcessMID->SetScalarParameterValue(BrightnessAlphaParameterName, 0.f);
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

	SpeedBoostPostProcessMID->SetScalarParameterValue(EdgeBrightnessParameterName, ClampedBrightness);
	SpeedBoostPostProcessMID->SetScalarParameterValue(BrightnessAlphaParameterName, ClampedBrightnessAlpha);
	SpeedBoostPostProcessMID->SetVectorParameterValue(GlowColorParameterName, State.GlowColor);
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
		SpeedBoostPostProcessMID->SetScalarParameterValue(BrightnessAlphaParameterName, CurrentBrightnessAlpha);
		SpeedBoostPostProcessMID->SetVectorParameterValue(GlowColorParameterName, FLinearColor::Black);
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
	ResetBuffTracking();

	if (SpeedBoostPostProcessMID)
	{
		SpeedBoostPostProcessMID->SetScalarParameterValue(EdgeBrightnessParameterName, 0.f);
		SpeedBoostPostProcessMID->SetScalarParameterValue(BrightnessAlphaParameterName, 0.f);
		SpeedBoostPostProcessMID->SetVectorParameterValue(GlowColorParameterName, FLinearColor::Black);
	}

	RemoveSpeedBoostPostProcessBlendable();
	SpeedBoostPostProcessMID = nullptr;
}
