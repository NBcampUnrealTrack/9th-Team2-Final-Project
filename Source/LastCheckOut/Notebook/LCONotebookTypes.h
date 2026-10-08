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
	Photo      UMETA(DisplayName = "사진·도면")
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

// 지도 탭의 구역 1개 (본관 / 별관 / 지하 / 외부 등)
USTRUCT(BlueprintType)
struct FLCONotebookMapArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
	FText AreaName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
	TObjectPtr<UTexture2D> MapImage = nullptr;
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
