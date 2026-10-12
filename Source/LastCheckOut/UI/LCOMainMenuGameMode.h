#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LCOMainMenuGameMode.generated.h"

/**
 * 메인 메뉴 맵 전용 GameMode
 * - 폰 없음, 메인 메뉴 PlayerController 사용
 * - LCOGameMode(StartGame 로직)와 분리해서 메뉴 맵에서 게임 로직이 돌지 않게 함
 * - 메뉴 맵 World Settings > GameMode Override에 BP 자식 클래스 지정
 */
UCLASS()
class LASTCHECKOUT_API ALCOMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALCOMainMenuGameMode();
};
