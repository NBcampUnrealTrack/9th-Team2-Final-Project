#pragma once

#include "CoreMinimal.h"
#include "Interaction/LCOOpenableBase.h"
#include "LCODoor.generated.h"

// 상호작용으로 열고 닫는 문
// 열기 / 잠금 / 열쇠 로직은 ALCOOpenableBase에서 처리하고, 메시와 열림 연출은 BP 자식에서 구현한다
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCODoor : public ALCOOpenableBase
{
	GENERATED_BODY()
};
