#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOSpeedBoostUISubsystem.generated.h"

class ALCOPlayerCharacter;
class APawn;
class APlayerController;
class UCameraComponent;
class ULCOQuickSlotComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UWorld;

UCLASS()
class LASTCHECKOUT_API ULCOSpeedBoostUISubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	ULCOSpeedBoostUISubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	struct FBuffPostProcessState
	{
		float EdgeBrightness = 0.f;
		float BrightnessAlpha = 0.f;
		FLinearColor GlowColor = FLinearColor::Black;
		bool bHasVisibleBuff = false;
		bool bAnyBuffActive = false;
	};

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleQuickSlotItemUsed(int32 SlotIndex, FName ItemID);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromCharacter();
	void BindToQuickSlot(ULCOQuickSlotComponent* QuickSlot);
	void UnbindFromQuickSlot();
	void ResetBuffTracking();

	void UpdateSpeedBoostPostProcess(const FBuffPostProcessState& State);
	FBuffPostProcessState ResolveBuffPostProcessState(float DeltaTime, const ALCOPlayerCharacter* Character);
	float ResolveMoveSpeedContribution(float DeltaTime, const ALCOPlayerCharacter* Character, bool bMoveSpeedBuffActive, float MoveSpeedAlpha);
	float ResolveMaxStaminaContribution(float DeltaTime, const ALCOPlayerCharacter* Character, bool bMaxStaminaBuffActive, float MaxStaminaAlpha);
	float UpdateBrightnessAlpha(float DeltaTime, bool bAnyBuffActive);
	FLinearColor ResolveTargetGlowColor(bool bMoveSpeedVisible, bool bMaxStaminaVisible) const;
	bool EnsureSpeedBoostPostProcess(APlayerController* PlayerController);
	UCameraComponent* FindActiveCameraComponent(APlayerController* PlayerController) const;
	void RemoveSpeedBoostPostProcessBlendable();
	void SetSpeedBoostPostProcessBlendableWeight(float Weight);
	void CleanupSpeedBoostPostProcess();

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALCOPlayerCharacter> BoundCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOQuickSlotComponent> BoundQuickSlot;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> SpeedBoostPostProcessCameraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpeedBoostPostProcessMID;

	TSoftObjectPtr<UMaterialInterface> SpeedBoostPostProcessMaterial;
	FName EdgeBrightnessParameterName = TEXT("EdgeBrightness");
	FName GlowColorParameterName = TEXT("GlowColor");
	FName BrightnessAlphaParameterName = TEXT("BrightnessAlpha");

	UPROPERTY(EditDefaultsOnly, Category = "LCO|BuffPostProcess", meta = (ClampMin = "0.01"))
	float MoveSpeedFullBrightnessBonus = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|BuffPostProcess", meta = (ClampMin = "0.01"))
	float MaxStaminaFullBrightnessBonus = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|BuffPostProcess", meta = (ClampMin = "0.01"))
	float ColorInterpTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|BuffPostProcess")
	FLinearColor MoveSpeedGlowColor = FLinearColor(0.77f, 0.91f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LCO|BuffPostProcess")
	FLinearColor MaxStaminaGlowColor = FLinearColor(1.f, 0.9f, 0.54f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LCO|BuffPostProcess")
	FLinearColor CombinedGlowColor = FLinearColor(0.46f, 0.96f, 0.86f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LCO|BuffPostProcess", meta = (ClampMin = "0.01"))
	float BrightnessTransitionTime = 1.f;

	float RecordedMoveSpeedAmount = 0.f;
	float RecordedMaxStaminaAmount = 0.f;
	float CurrentEdgeBrightness = 0.f;
	float CurrentBrightnessAlpha = 0.f;
	float BrightnessTransitionStartAlpha = 0.f;
	float BrightnessTransitionTargetAlpha = 0.f;
	float BrightnessTransitionElapsed = 0.f;
	float CurrentMoveSpeedContribution = 0.f;
	float CurrentMaxStaminaContribution = 0.f;
	FLinearColor CurrentGlowColor = FLinearColor::Black;
	bool bWasMoveSpeedBuffActive = false;
	bool bWasMaxStaminaBuffActive = false;
	bool bMoveSpeedFadingOutAfterBuffEnd = false;
	bool bMaxStaminaFadingOutAfterBuffEnd = false;
	bool bWarnedMaterialLoadFailure = false;
	bool bWarnedNoActiveCamera = false;
};
