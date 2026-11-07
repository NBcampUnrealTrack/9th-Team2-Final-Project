#pragma once

#include "CoreMinimal.h"

// 스토리 진행에 쓰는 ID 모음 (챕터 / 목표 / 상태 플래그 / 엔딩)
// - 문자열을 직접 쓰지 말고 여기 상수를 사용 (오타가 컴파일 오류로 잡힘)
// - 세이브에 이름 그대로 저장되므로 기존 값은 바꾸지 말 것
namespace LCOStoryIDs
{
	// 챕터
	inline const FName Chapter1(TEXT("Chapter1"));
	inline const FName Chapter2(TEXT("Chapter2"));
	inline const FName Chapter3(TEXT("Chapter3"));

	// 새 게임 첫 챕터
	inline const FName FirstChapter(TEXT("Chapter1"));

	// 목표
	inline const FName FollowCaretakerObjective(TEXT("C2_FollowCaretaker"));

	// 상태 플래그
	inline const FName NotebookConfiscatedFlag(TEXT("C2_NotebookConfiscated"));

	// 엔딩
	inline const FName End3(TEXT("End3"));

	// 출시 범위 끝 (2막 클리어 → "To be continued")
	inline const FName ToBeContinued(TEXT("ToBeContinued"));

	// 추리 키워드 플래그 이름 앞글자 (ELCOStoryFlag의 K01 ~)
	inline const TCHAR* const DeductionFlagPrefix = TEXT("K");

	// 추리 후 새 목표 수첩 행 이름 뒤에 붙는 글자 (예: K01_OBJ)
	inline const TCHAR* const ObjectiveRowSuffix = TEXT("_OBJ");
}
