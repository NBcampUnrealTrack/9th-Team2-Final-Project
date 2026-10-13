#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOLoadingSubsystem.generated.h"

UCLASS()
class LASTCHECKOUT_API ULCOLoadingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "LCO|Loading")
	void RequestLevelTravel(FName LevelName);

private:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	FText SelectLoadingTipText() const;
	void ShowLoadingScreen(const FText& TipText);

	static constexpr float MinimumLoadingScreenTime = 3.0f;
};
