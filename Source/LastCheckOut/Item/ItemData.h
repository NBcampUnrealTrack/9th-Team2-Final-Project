
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemData.generated.h"

// 아이템의 종류
UENUM(BlueprintType)
enum class EItemType : uint8
{
    Consumable, // 소비 아이템
    KeyItem,    // 열쇠, 호텔 도면 등
    Evidence    // 증거품
};

// 개별 버프 효과의 종류
UENUM(BlueprintType)
enum class EBuffType : uint8
{
    None,       // 버프 효과 없음
    MoveSpeed,  // 이동속도 증가
    MaxStamina  // 최대 스태미나 증가
};

// 아이템 하나에 적용할 개별 버프 효과 데이터
USTRUCT(BlueprintType)
struct FItemBuffEffect
{
    GENERATED_BODY()

    // 적용할 버프 종류
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Buff")
    EBuffType BuffType = EBuffType::None;

    // 버프 효과 수치
    // MoveSpeed : 이동속도 증가량
    // MaxStamina : 최대 스태미나 증가량
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Buff",
        meta = (ClampMin = "0.0"))
    float BuffAmount = 0.0f;

    // 버프 지속 시간(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Buff",
        meta = (ClampMin = "0.0"))
    float BuffDuration = 0.0f;
};

// 아이템 데이터 테이블의 한 행을 구성하는 구조체
USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
    GENERATED_BODY()

    // 아이템 고유 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FName ItemID;

    // 아이템 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FText ItemName;

    // 아이템 설명
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FText Description;

    // 아이템 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    TObjectPtr<UTexture2D> Icon = nullptr;

    // 슬롯당 최대 보유 개수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item",
        meta = (ClampMin = "1"))
    int32 MaxStackCount = 20;

    // 아이템 종류
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    EItemType ItemType = EItemType::Consumable;

    // 아이템 사용 시 적용할 버프 효과 목록
    // 배열이므로 하나의 아이템에 여러 버프를 설정할 수 있음
    // 예: 홍삼 블랙커피 = 이동속도 증가 + 최대 스태미나 증가
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item|Buff")
    TArray<FItemBuffEffect> BuffEffects;
};
