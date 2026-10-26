#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "LCONotebookTypes.generated.h"

class UTexture2D;

// 수첩 항목 분류 (지도는 항목이 아니라 별도 데이터라 여기 없음)
UENUM(BlueprintType)
enum class ELCONotebookCategory : uint8
{
    Objective  UMETA(DisplayName = "목표"),
    Clue       UMETA(DisplayName = "단서·증거"),
    Testimony  UMETA(DisplayName = "증언"),
    Photo      UMETA(DisplayName = "사진·도면"),
    Deduction  UMETA(DisplayName = "추리")   // 조합 성공 시 독백 기록 (새 항목은 항상 맨 아래에 추가)
};

/**
 * 수첩 항목 1개
 * - 데이터 담당은 이 형식으로 ULCONotebookSubsystem::AddEntry 호출
 * - FTableRowBase 상속이라 DataTable 행으로 바로 써도 됨 (RowName = EntryID 권장)
 */
USTRUCT(BlueprintType)
struct FLCONotebookEntry : public FTableRowBase
{
    GENERATED_BODY()

    // 고유 ID (중복 등록 방지 기준)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    FName EntryID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    ELCONotebookCategory Category = ELCONotebookCategory::Clue;

    // 목록 안 묶음 (단서·증언 탭에서 같은 GroupID끼리 머리말 아래로 묶임)
    // - 증언: 말한 사람 (예: Professor, Manager, Bellboy)
    // - 증거: 발견 장소 (예: MainBuilding, Annex, Room407)
    // - 머리말 문구는 UI.csv의 "NOTEBOOK_GROUP_<GroupID>" 키 (없으면 GroupID 그대로 표시)
    // - 비워두면 머리말 없이 목록 맨 아래에 표시
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    FName GroupID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook", meta = (MultiLine = true))
    FText Description;

    // 상세 페이지 이미지 (없으면 비워둠)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    TObjectPtr<UTexture2D> Image = nullptr;

    // 새로 얻은 항목 (수첩에서 한 번 열어보면 false)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    bool bIsNew = true;
};

// ---------------------------------------------------------------------
// 지도
// ---------------------------------------------------------------------

/**
 * 구역 안의 층 1개 (B1, 1F, 2F ...)
 * WorldMin / WorldMax = 지도 이미지로 잘라낸 범위가 레벨에서 차지하는 영역
 * - X, Y: 이미지 가장자리에 대응하는 월드 좌표
 * - Z: 이 층으로 판정할 높이 범위 (층 바닥 조금 아래 ~ 다음 층 바닥 바로 아래)
 */
USTRUCT(BlueprintType)
struct FLCONotebookMapFloor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    FText FloorName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    TObjectPtr<UTexture2D> MapImage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook|Location")
    FVector WorldMin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook|Location")
    FVector WorldMax = FVector::ZeroVector;

    // 기본 방향: 이미지 위쪽 = 월드 +X(북), 이미지 오른쪽 = 월드 +Y(동)
    // 레벨 배치 방향이 다르면 아래 옵션 조합으로 맞춤
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook|Location")
    bool bSwapAxes = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook|Location")
    bool bFlipU = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook|Location")
    bool bFlipV = false;

    // 위치 마커 화살표 방향 보정 (도 단위)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook|Location")
    float MarkerYawOffset = 0.f;

    // 위치 정보가 입력된 층인지 (비워두면 마커 없이 이미지만 표시)
    bool HasWorldBounds() const
    {
        return WorldMax.X > WorldMin.X && WorldMax.Y > WorldMin.Y;
    }

    bool ContainsLocation(const FVector& Loc) const
    {
        return HasWorldBounds()
            && Loc.X >= WorldMin.X && Loc.X <= WorldMax.X
            && Loc.Y >= WorldMin.Y && Loc.Y <= WorldMax.Y
            && Loc.Z >= WorldMin.Z && Loc.Z <= WorldMax.Z;
    }

    // 월드 좌표 → 지도 이미지 비율 좌표 (0~1, 좌상단 기준)
    FVector2D WorldToUV(const FVector& Loc) const
    {
        const FVector Size = WorldMax - WorldMin;
        const float RatioX = Size.X > KINDA_SMALL_NUMBER ? (Loc.X - WorldMin.X) / Size.X : 0.f;
        const float RatioY = Size.Y > KINDA_SMALL_NUMBER ? (Loc.Y - WorldMin.Y) / Size.Y : 0.f;

        float U = bSwapAxes ? RatioX : RatioY;
        float V = bSwapAxes ? RatioY : 1.f - RatioX;

        if (bFlipU) { U = 1.f - U; }
        if (bFlipV) { V = 1.f - V; }

        return FVector2D(FMath::Clamp(U, 0.f, 1.f), FMath::Clamp(V, 0.f, 1.f));
    }
};

// 지도 탭의 구역 1개 (본관 / 별관 / 지하 / 외부 등)
USTRUCT(BlueprintType)
struct FLCONotebookMapArea
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    FText AreaName;

    // 층이 없는 구역(외부 등)은 이 이미지 한 장만 사용
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    TObjectPtr<UTexture2D> MapImage = nullptr;

    // 층이 있는 구역은 위층부터 순서대로 입력 (예: 4F, 3F, 2F, 1F, B1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
    TArray<FLCONotebookMapFloor> Floors;
};

/**
 * 지도 탭 데이터 (DA_NotebookMap)
 * 콘텐츠 브라우저 → Miscellaneous → Data Asset → LCONotebookMapData 로 생성
 */
UCLASS(BlueprintType)
class LASTCHECKOUT_API ULCONotebookMapData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 위에서부터 순서대로 지도 탭 버튼으로 표시
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notebook")
    TArray<FLCONotebookMapArea> Areas;
};