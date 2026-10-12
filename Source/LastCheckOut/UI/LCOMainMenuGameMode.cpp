#include "LCOMainMenuGameMode.h"

#include "LCOMainMenuPlayerController.h"

ALCOMainMenuGameMode::ALCOMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ALCOMainMenuPlayerController::StaticClass();
}
