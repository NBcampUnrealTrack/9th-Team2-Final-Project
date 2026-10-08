#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemData.generated.h"

UENUM(BlueprintType)
enum class EItemType : uint8
{
	Normal,     // 일반 아이템
	Consumable, // 소비 아이템
	KeyItem,    // 열쇠 등
	Evidence   // 증거품
};

USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
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
	int32 MaxStackCount = 20; 
	
	// 아이템의 종류를 구분하여 아이템 사용 방식 및 기능을 결정
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemType ItemType = EItemType::Normal;
	
	
};