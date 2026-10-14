#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Save/LCOWorldStateTypes.h"
#include "LCOSaveableActor.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class ULCOSaveableActor : public UInterface
{
	GENERATED_BODY()
};

// 맵을 떠났다가 돌아와도 상태가 유지되어야 하는 액터가 구현 (문, 서랍장, 트리거 등)
// - 맵을 떠날 때 / 저장할 때: SaveActorState()로 현재 상태를 받아 기록
// - 맵에 다시 들어왔을 때: LoadActorState()로 기록된 상태를 적용 (모든 BeginPlay 이후 호출)
// ※ 퍼즐은 LCOPuzzleSubsystem(퍼즐 ID 기준)이 따로 저장하므로 구현하지 않음
class LASTCHECKOUT_API ILCOSaveableActor
{
	GENERATED_BODY()

public:
	// 현재 상태를 구조체로 반환
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|WorldState")
	FLCOActorSaveState SaveActorState() const;

	// 기록된 상태 적용. 연출(애니메이션, 사운드) 없이 상태만 맞출 것
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LCO|WorldState")
	void LoadActorState(const FLCOActorSaveState& State);
};
