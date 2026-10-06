#pragma once

#include "CoreMinimal.h"
#include "NPC/LCONPCBase.h"
#include "LCOCaretaker.generated.h"

/* 관리인 (추격자). AI 로직은 AIController/BehaviorTree에서 담당한다. */
UCLASS()
class LASTCHECKOUT_API ALCOCaretaker : public ALCONPCBase
{
	GENERATED_BODY()

public:
	ALCOCaretaker();
};
