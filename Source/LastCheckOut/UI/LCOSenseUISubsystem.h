#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOSenseUISubsystem.generated.h"

class ALCOPlayerCharacter;
class APawn;
class APlayerController;
class UCameraComponent;
class UInputAction;
class ULCOSenseWidget;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UWorld;

UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOSenseUISubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	ULCOSenseUISubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintPure, Category = "LCO|SenseUI")
	ULCOSenseWidget* GetSenseWidget() const { return SenseWidget; }

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void TryCreateSenseUI(APlayerController* PlayerController);
	void CleanupSenseUI();

	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromCharacter();

	bool BindSenseInput(APlayerController* PlayerController);
	void UnbindSenseInput();
	void ScheduleSenseInputBindRetry();
	void HandleSenseInputBindRetry();
	void HandleSenseInputStarted();
	void HandleSenseInputReleased();
	void RefreshHoldStateNextTick();

	UFUNCTION()
	void HandleSenseUsed(bool bSuccess);

	void StartSensePostProcessEffect();
	void UpdateSensePostProcessEffect(float DeltaTime);
	void EnsureSensePostProcess(APlayerController* PlayerController);
	void CleanupSensePostProcess();
	UCameraComponent* FindActiveCameraComponent(APlayerController* PlayerController) const;
	void RemoveSensePostProcessBlendable();
	void SetSensePostProcessBlendableWeight(float Weight);
	void SetSensePostProcessStrength(float Strength);
	void SetSensePostProcessZoomStrength(float Strength);

	bool ShouldCreateForWorld(const UWorld* World) const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI")
	TSoftClassPtr<ULCOSenseWidget> SenseWidgetClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI")
	TSoftObjectPtr<UInputAction> SenseInputAction;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI")
	TArray<FName> ExcludedMapNames;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI")
	int32 ViewportZOrder = 4;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess")
	TSoftObjectPtr<UMaterialInterface> SensePostProcessMaterial;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess")
	FName EffectStrengthParameterName = TEXT("EffectStrength");

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess")
	FName ZoomStrengthParameterName = TEXT("ZoomStrength");

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess")
	FName ScanOriginParameterName = TEXT("ScanOrigin");

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess")
	FName ScanProgressParameterName = TEXT("ScanProgress");

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess", meta = (ClampMin = "0.0"))
	float EffectMaxStrength = 1.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess", meta = (ClampMin = "0.01"))
	float EffectDuration = 4.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess", meta = (ClampMin = "0.0"))
	float EffectEnterTime = 0.5f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess", meta = (ClampMin = "0.0"))
	float EffectWaveTime = 1.f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess", meta = (ClampMin = "0.0"))
	float EffectWaveAmplitudeRatio = 0.5f;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|SenseUI|PostProcess", meta = (ClampMin = "0.01"))
	float ScanDuration = 1.5f;

	UPROPERTY(Transient)
	TObjectPtr<ULCOSenseWidget> SenseWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> SensePostProcessCameraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SensePostProcessMID;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALCOPlayerCharacter> BoundCharacter;

	uint32 SenseStartedBindingHandle = 0;
	uint32 SenseCompletedBindingHandle = 0;
	uint32 SenseCanceledBindingHandle = 0;
	FTimerHandle SenseInputBindRetryTimer;

	float EffectElapsedTime = 0.f;
	float EffectStartStrength = 0.f;
	float ZoomStartStrength = 0.f;
	float CurrentEffectStrength = 0.f;
	float CurrentZoomStrength = 0.f;
	bool bSenseEffectActive = false;
};
