#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOSpeedBoostUISubsystem.generated.h"

class ALCOPlayerCharacter;
class APawn;
class APlayerController;
class UCameraComponent;
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
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromCharacter();

	void UpdateSpeedBoostPostProcess(float Alpha);
	float ResolveEdgeBrightness(float DeltaTime, const ALCOPlayerCharacter* Character);
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
	TWeakObjectPtr<UCameraComponent> SpeedBoostPostProcessCameraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SpeedBoostPostProcessMID;

	TSoftObjectPtr<UMaterialInterface> SpeedBoostPostProcessMaterial;
	FName EdgeBrightnessParameterName = TEXT("EdgeBrightness");

	float CurrentEdgeBrightness = 0.f;
	bool bWasMoveSpeedBuffActive = false;
	bool bFadingOutAfterBuffEnd = false;
	bool bWarnedMaterialLoadFailure = false;
	bool bWarnedNoActiveCamera = false;
};
