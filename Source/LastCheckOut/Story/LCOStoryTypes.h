#pragma once

#include "CoreMinimal.h"
#include "LCOStoryTypes.generated.h"

// 증언, 조합 키워드 플래그 ID (기획 문서 번호와 동일)
// - 획득, 조건 검사: LCOStorySubsystem (TryUnlock / CanUnlock / HasFlag)
// - 세이브에는 이름("T01")으로 저장되므로 기존 항목의 이름은 바꾸지 말 것
// - 새 항목은 항상 맨 아래에 추가
UENUM(BlueprintType)
enum class ELCOStoryFlag : uint8
{
	// 증언
	T01 UMETA(DisplayName = "T01 [교수] 수십 년 단골 (지질 조사)"),
	T02 UMETA(DisplayName = "T02 [교수] 화재 전 직원들 이상 행동"),
	T03 UMETA(DisplayName = "T03 [지배인] 실족사 조기 종결 희망"),
	T04 UMETA(DisplayName = "T04 [벨보이] 피해자 혼자 산책로로"),
	T05 UMETA(DisplayName = "T05 [관리인] 산책로 정기 점검"),
	T06 UMETA(DisplayName = "T06 [프론트] 다음 날 투숙객이 발견·신고"),
	T07 UMETA(DisplayName = "T07 [청소부] 청소 창고의 옛 도면"),
	T08 UMETA(DisplayName = "T08 [교수] 잦은 사고에도 호텔 호황"),
	T09 UMETA(DisplayName = "T09 [지배인] 옛 신관은 현재 별관"),
	T10 UMETA(DisplayName = "T10 [프론트] 사고 기록 출처 추궁"),
	T11 UMETA(DisplayName = "T11 [프론트] 별관 407호 투숙객 안 받음"),

	// 조합 키워드 (추리)
	K01 UMETA(DisplayName = "K01 1981년 화재의 의문점"),
	K02 UMETA(DisplayName = "K02 신관의 건축 시기와 구조 변경"),
	K03 UMETA(DisplayName = "K03 과거 사망 사건의 공통점"),
	K04 UMETA(DisplayName = "K04 실족사의 가능성"),
	K05 UMETA(DisplayName = "K05 407호에 대한 의문"),
	K06 UMETA(DisplayName = "K06 실족사에 대한 호텔의 대응"),
	K07 UMETA(DisplayName = "K07 조사 종결"),
	K08 UMETA(DisplayName = "K08 실족사 사고에 대하여"),
};

// 수첩 추리 탭 조합 결과
UENUM(BlueprintType)
enum class ELCODeductionResult : uint8
{
	Success       UMETA(DisplayName = "성공"),           // 새 키워드 획득
	AlreadyKnown  UMETA(DisplayName = "이미 아는 내용"),  // 맞는 조합이지만 이미 획득
	NoMatch       UMETA(DisplayName = "연결 없음"),       // 맞는 조합 없음
	NotEnough     UMETA(DisplayName = "항목 부족")        // 2개 미만 선택
};

// 플래그 1개의 해금 조건: 필요한 증거물 + 필요한 플래그 (둘 다 비어 있으면 조건 없음)
USTRUCT()
struct FLCOUnlockRule
{
	GENERATED_BODY()

	// 필요한 증거물 ID (수첩 기록 ID = 아이템 ID, 예: "E01")
	UPROPERTY()
	TArray<FName> RequiredEvidence;

	// 먼저 획득해야 하는 플래그 (예: K01)
	UPROPERTY()
	TArray<ELCOStoryFlag> RequiredFlags;
};

// 지배인에게 추리를 제시 / 보고한 결과
UENUM(BlueprintType)
enum class ELCOReportResult : uint8
{
	NotEnough      UMETA(DisplayName = "제시 불가"),        // 필요한 추리가 없거나 1막이 아님
	Chapter1Clear  UMETA(DisplayName = "1막 클리어"),       // K05 제시 → 407호 조사 허가
	End3           UMETA(DisplayName = "END 3"),          // K07 보고 (K03 없음) → 사고 조사 종결
	Reconsidered   UMETA(DisplayName = "조사 종결 멈춤")    // K07 보고 (K03 있음) → 주인공이 멈추고 조사 계속
};
