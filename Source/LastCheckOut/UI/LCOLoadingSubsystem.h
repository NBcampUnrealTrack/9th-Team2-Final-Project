#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOLoadingSubsystem.generated.h"

class IInputProcessor;
class SWidget;
class UGameViewportClient;
class UInputComponent;
class ULCOLoadingOverlayWidget;
struct FWorldContext;

// Broadcast after the loading screen is removed. bSuccess is false when loading failed or timed out (fail-open).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOLoadingFinished, bool, bSuccess);

UCLASS()
class LASTCHECKOUT_API ULCOLoadingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "LCO|Loading")
	void RequestLevelTravel(FName LevelName);

	// Read-only hook for UI and future gameplay clocks (e.g. buff duration). Does not pause the world.
	UFUNCTION(BlueprintPure, Category = "LCO|Loading")
	bool IsLoading() const { return bLoading; }

	UFUNCTION(BlueprintPure, Category = "LCO|Loading")
	bool DidLastLoadingFail() const { return bLastLoadingFailed; }

	// Fired after screens are removed and player input is released (arrival events, intro dialogue, etc.).
	UPROPERTY(BlueprintAssignable, Category = "LCO|Loading")
	FOnLCOLoadingFinished OnLoadingFinished;

private:
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	bool TickLoading(float DeltaTime);
	bool InstallTravelGuard();
	bool InstallOverlay(UWorld* World);
	void BlockPlayerInput(UWorld* World);
	void ReleasePlayerInput();
	void RemoveScreens();
	void FinishLoading(bool bSuccess, const TCHAR* Reason);
	int32 GetRemainingShaders() const;

	FText SelectLoadingTipText() const;
	void ShowLoadingScreen(const FText& TipText);

	static constexpr double MinimumLoadingScreenTime = 3.0;
	static constexpr double PostCompletionHoldSeconds = 0.5;
	static constexpr double PreparationTimeoutSeconds = 120.0;
	static constexpr int32 RequiredZeroFrames = 3;

	UPROPERTY(Transient)
	TObjectPtr<ULCOLoadingOverlayWidget> OverlayWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputComponent>> InputBlockers;

	UPROPERTY(Transient)
	FText ActiveTipText;

	TWeakObjectPtr<UWorld> ActiveWorld;
	TWeakObjectPtr<UGameViewportClient> ScreenViewport;
	TSharedPtr<SWidget> TravelGuard;
	TSharedPtr<SWidget> OverlayContent;
	TSharedPtr<IInputProcessor> InputProcessor;
	FTSTicker::FDelegateHandle LoadingTicker;
	double LoadingStartTime = 0.0;
	double PreparationStartTime = 0.0;
	// Monotonic timestamp after all required readiness checks; negative means not ready.
	double RequiredLoadingCompletedTime = -1.0;
	uint64 LastCheckedFrame = MAX_uint64;
	int32 ZeroShaderFrames = 0;
	int32 RemainingShaders = 0;
	bool bLoading = false;
	bool bLastLoadingFailed = false;
};
