#pragma once

#include "CoreMinimal.h"
#include "LCOCaretakerTypes.generated.h"

UENUM(BlueprintType)
enum class ELCOCaretakerState : uint8
{
	Idle,
	Patrol,
	Suspicious,
	Search,
	Chase,
	LostPlayer,
	Return
};

namespace LCOBlackboardKeys
{
	const FName State = TEXT("State");
	const FName TargetActor = TEXT("TargetActor");
	const FName LastKnownLocation = TEXT("LastKnownLocation");
	const FName PatrolLocation = TEXT("PatrolLocation");
	const FName SearchLocation = TEXT("SearchLocation");
}