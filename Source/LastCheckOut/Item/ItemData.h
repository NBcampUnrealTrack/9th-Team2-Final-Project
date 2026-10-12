#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemData.generated.h"

UENUM(BlueprintType)
enum class EItemType : uint8
{
	Consumable, // 소비 아이템
	KeyItem,    // 열쇠, 호텔도면 등
	Evidence   // 증거품
};

// 아이템 사용 시 적용되는 버프의 종류
UENUM(BlueprintType)
enum class EBuffType : uint8
{
	None,        // 버프 효과 없음
	MoveSpeed,   // 이동속도 증가
	MaxStamina   // 최대 스태미나 증가
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
	
	// 아이템 종류
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EItemType ItemType = EItemType::Consumable;

	// 아이템 사용 시 적용되는 버프 종류
	// 버프가 없는 아이템은 None으로 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBuffType BuffType = EBuffType::None;
	
	// 버프 효과 수치
	// MoveSpeed : 증가할 이동속도
	// MaxStamina : 증가할 최대 스태미나
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BuffAmount = 0.0f;

	// 버프 지속 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BuffDuration = 0.0f;
	
};