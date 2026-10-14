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
	T01 UMETA(DisplayName = "T01 [교수] 별관은 과거 신관"),
	T02 UMETA(DisplayName = "T02 [교수] 화재 전 연결 통로 봉쇄"),
	T03 UMETA(DisplayName = "T03 [지배인] 실족사 조기 종결 희망"),
	T04 UMETA(DisplayName = "T04 [벨보이] 사망자 혼자 산책로로"),
	T05 UMETA(DisplayName = "T05 [관리인] 난간·조명 정기 관리"),
	T06 UMETA(DisplayName = "T06 [프론트] 투숙객이 사고 신고"),
	T07 UMETA(DisplayName = "T07 [청소부] 별관 특정 객실 출입 제한"),
	T08 UMETA(DisplayName = "T08 [장기 투숙객] 과거 사고 소문"),
	T09 UMETA(DisplayName = "T09 [관리인] 화재 후 건물 구조 변경"),
	T10 UMETA(DisplayName = "T10 [프론트] 과거 사고 기록 일부 없음"),
	T11 UMETA(DisplayName = "T11 [벨보이] 산책로 미끄러움 안내"),
	T12 UMETA(DisplayName = "T12 [관리인] 비 오면 사고 지점 미끄러움"),
	T13 UMETA(DisplayName = "T13 [지배인] 사고 후 출입 제한"),

	// 조합 키워드
	K01 UMETA(DisplayName = "K01 화재 당시 투숙객 이동 경로"),
	K02 UMETA(DisplayName = "K02 신관 건축 시기와 구조 변경"),
	K03 UMETA(DisplayName = "K03 과거 사망 사건의 공통점"),
	K04 UMETA(DisplayName = "K04 실족사 발생 경위"),
	K05 UMETA(DisplayName = "K05 1981년 화재 은폐 의혹"),
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
