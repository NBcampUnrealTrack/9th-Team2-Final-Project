#include "Save/LCOWorldStateSubsystem.h"
#include "LastCheckOut.h"
#include "Save/LCOSaveableActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"

void ULCOWorldStateSubsystem::CaptureWorldState(UWorld* World)
{
	if (!World)
	{
		return;
	}

	// 이 맵의 기록 (없으면 새로 만듦)
	const FName MapKey = GetMapKey(World);
	FLCOMapSaveState& MapState = MapStates.FindOrAdd(MapKey);

	int32 CapturedCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->Implements<ULCOSaveableActor>())
		{
			continue;
		}

		MapState.Actors.Add(Actor->GetFName(), ILCOSaveableActor::Execute_SaveActorState(Actor));
		++CapturedCount;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOWorldState] Capture: %s (Actors = %d)"), *MapKey.ToString(), CapturedCount);
}

void ULCOWorldStateSubsystem::ApplyWorldState(UWorld* World)
{
	if (!World)
	{
		return;
	}

	const FName MapKey = GetMapKey(World);
	const FLCOMapSaveState* MapState = MapStates.Find(MapKey);
	if (!MapState)
	{
		// 처음 들어온 맵: 에디터에 배치한 상태 그대로
		UE_LOG(LogLCOCore, Log, TEXT("[LCOWorldState] Apply: %s (no record)"), *MapKey.ToString());
		return;
	}

	int32 AppliedCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->Implements<ULCOSaveableActor>())
		{
			continue;
		}

		// 기록이 없는 액터는 에디터에 배치한 상태 그대로
		if (const FLCOActorSaveState* State = MapState->Actors.Find(Actor->GetFName()))
		{
			ILCOSaveableActor::Execute_LoadActorState(Actor, *State);
			++AppliedCount;
		}
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOWorldState] Apply: %s (Applied = %d)"), *MapKey.ToString(), AppliedCount);
}

void ULCOWorldStateSubsystem::ClearAll()
{
	MapStates.Empty();

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOWorldState] ClearAll"));
}

void ULCOWorldStateSubsystem::RestoreAllMapStates(const TMap<FName, FLCOMapSaveState>& InMapStates)
{
	MapStates = InMapStates;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOWorldState] RestoreAllMapStates: Maps = %d"), MapStates.Num());
}

FName ULCOWorldStateSubsystem::GetMapKey(const UWorld* World)
{
	return World ? FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) : NAME_None;
}
