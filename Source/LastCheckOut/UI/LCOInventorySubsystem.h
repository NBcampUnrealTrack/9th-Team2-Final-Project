#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOInventorySubsystem.generated.h"

class ULCOInventoryWidget;
class UWorld;

UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOInventorySubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	ULCOInventorySubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

private:
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void TryCreateInventory(APlayerController* PlayerController);
	void CleanupInventory();
	bool ShouldCreateInventoryForWorld(const UWorld* World) const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Inventory")
	TSoftClassPtr<ULCOInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<ULCOInventoryWidget> InventoryWidget;
};
