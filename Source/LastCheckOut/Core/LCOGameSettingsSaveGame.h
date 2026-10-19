#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LCOGameSettingsSaveGame.generated.h"

// 게임 설정 저장 파일 (슬롯 LCO_Settings) → LCOGameSettingsSubsystem 담당
// - 진행 세이브(LCO_Save_0)와 별도라 새 게임을 시작해도 유지됨
// - 해상도 / 화면 모드 / 그래픽 품질은 엔진이 GameUserSettings.ini에 따로 저장하므로 여기 없음
// - 사운드는 LCOAudioSettingsSubsystem이 따로 저장
UCLASS()
class LASTCHECKOUT_API ULCOGameSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 마우스 감도 (1.0 = 기본)
	UPROPERTY()
	float LookSensitivity = 1.f;

	// 마우스 Y축 반전
	UPROPERTY()
	bool bInvertLookY = false;

	// 밝기 = 엔진 감마 값 (2.2 = 엔진 기본)
	UPROPERTY()
	float Brightness = 2.2f;
};
