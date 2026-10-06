#pragma once

#include "CoreMinimal.h"
#include "ItemData.generated.h"

USTRUCT(BlueprintType)
struct FItemData
{
	GENERATED_BODY()

	// 아이템 고유 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemID;

	// 아이템 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText ItemName;

	// 아이템 설명
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	// 아이템 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> Icon;

	// 슬롯당 최대 보유 개수
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxStackCount = 20; // 현재 인벤토리 기본 기능에서는 미적용 추후 사용 예정
	
};