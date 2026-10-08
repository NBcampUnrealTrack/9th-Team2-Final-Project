#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOPauseMenuSubsystem.generated.h"

class ULCOPauseMenuWidget;
class UWorld;

/**
 * Local-player scoped owner for the pause menu widget.
 * Creates the widget once per playable world so LCOPauseMenuWidget can bind to
 * ALCOPlayerController::OnOptionsRequested without per-map Level Blueprint setup.
 */
UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOPauseMenuSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void TryCreatePauseMenu(APlayerController* PlayerController);
	void CleanupPauseMenu();
	bool ShouldCreatePauseMenuForWorld(const UWorld* World) const;
	FString GetComparableMapName(const UWorld* World) const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|PauseMenu")
	TSoftClassPtr<ULCOPauseMenuWidget> PauseMenuWidgetClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|PauseMenu")
	TArray<FName> ExcludedMapNames;

	UPROPERTY(Transient)
	TObjectPtr<ULCOPauseMenuWidget> PauseMenuWidget;
};
