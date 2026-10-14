#pragma once

#include "CoreMinimal.h"
#include "LCOWorldStateTypes.generated.h"

// 서랍장 등에 남은 아이템 1칸
USTRUCT(BlueprintType)
struct FLCOSavedItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|WorldState")
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|WorldState")
	int32 Quantity = 0;
};

// 액터 1개의 저장 상태. 액터마다 필요한 값만 채워서 사용 (안 쓰는 값은 기본값 그대로)
USTRUCT(BlueprintType)
struct FLCOActorSaveState
{
	GENERATED_BODY()

	// 문 / 서랍장: 열림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|WorldState")
	bool bIsOpen = false;

	// 문 / 서랍장: 잠김
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|WorldState")
	bool bIsLocked = false;

	// 트리거 등: 켜짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|WorldState")
	bool bIsEnabled = true;

	// 서랍장: 남은 아이템 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LCO|WorldState")
	TArray<FLCOSavedItem> Items;
};

// 맵 1개의 저장 상태 (액터 이름 → 상태)
USTRUCT()
struct FLCOMapSaveState
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FLCOActorSaveState> Actors;
};
