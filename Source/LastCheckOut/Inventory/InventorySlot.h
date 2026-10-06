#pragma once

#include "CoreMinimal.h"
#include "InventorySlot.generated.h"

USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	// 슬롯에 저장된 아이템의 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

	// 해당 슬롯에 저장된 아이템의 현재 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Quantity = 0;
};