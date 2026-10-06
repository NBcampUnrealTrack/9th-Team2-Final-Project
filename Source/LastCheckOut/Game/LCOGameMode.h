#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LCOGameMode.generated.h"

UCLASS()
class LASTCHECKOUT_API ALCOGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ALCOGameMode();
	
protected:
	virtual void BeginPlay() override;
	
private:
	void InitializeCurrentMap();
	
	void StartGame();
};
