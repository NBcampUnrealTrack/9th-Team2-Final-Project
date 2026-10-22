#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/GameUserSettings.h"
#include "LCOGameSettingsSubsystem.generated.h"

class ALCOPlayerCharacter;
class ULCOGameSettingsSaveGame;

// 설정 값이 바뀌었을 때 (설정창 표시 갱신용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOSettingsChanged);

// 그래픽 / 게임플레이 설정의 저장·불러오기·적용 담당 (GameInstance 단위라 맵을 이동해도 유지)
// - 설정창 위젯(LCOSettingsWidget)은 이 함수들만 호출
// - 그래픽(화면 모드 / 해상도 / 품질): 엔진 UGameUserSettings 사용 → Set 후 ApplyGraphicsSettings()로 적용 + GameUserSettings.ini 저장
// - 밝기 / 감도 / Y축 반전: Set 즉시 적용 + LCO_Settings 슬롯에 저장
// - 새 캐릭터 생성 시(새 게임 / 맵 이동 / 이어하기) LCOGameMode가 ApplyGameplaySettingsToPlayer() 호출
// - 사운드는 LCOAudioSettingsSubsystem 담당 (여기서 다루지 않음)
UCLASS()
class LASTCHECKOUT_API ULCOGameSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 게임 시작 시 저장된 설정을 불러와 밝기 적용
	// (해상도 / 화면 모드 / 품질은 엔진이 시작할 때 GameUserSettings.ini 값을 자동 적용)
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 그래픽 (Set 후 ApplyGraphicsSettings를 불러야 실제 적용)

	// 화면 모드: Fullscreen(전체 화면) / WindowedFullscreen(테두리 없는 창) / Windowed(창 모드)
	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Graphics")
	EWindowMode::Type GetWindowMode() const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void SetWindowMode(EWindowMode::Type Mode);

	// 모니터가 지원하는 해상도 목록 (해상도 콤보박스 채우기용)
	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Graphics")
	TArray<FIntPoint> GetSupportedResolutions() const;

	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Graphics")
	FIntPoint GetResolution() const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void SetResolution(FIntPoint Resolution);

	// 전체 그래픽 품질 0 낮음 / 1 중간 / 2 높음 / 3 최고 (그림자·텍스처·효과 등을 한 번에 변경)
	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Graphics")
	int32 GetOverallQuality() const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void SetOverallQuality(int32 Level);

	// 화면 모드 / 해상도 / 품질 변경을 실제로 적용하고 GameUserSettings.ini에 저장
	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void ApplyGraphicsSettings();

	// 아직 적용하지 않은 그래픽 변경을 취소하고 마지막 저장 값으로 되돌림 (확인창 취소 등)
	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void RevertGraphicsSettings();

	// 그래픽 확인 창
	// 1) ApplyGraphicsSettingsForConfirm: 화면에만 적용 (저장 안 함)
	// 2) 유지 → ConfirmGraphicsSettings: 저장 / 되돌리기 또는 시간 초과 → RevertAppliedGraphicsSettings: 직전 확정 값으로 복구

	// 화면 모드 / 해상도 / 품질을 화면에만 적용하고 확인 대기 (저장 안 함)
	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void ApplyGraphicsSettingsForConfirm();

	// 확인 대기 중인 그래픽 설정을 확정하고 저장 (유지 버튼)
	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void ConfirmGraphicsSettings();

	// 확인 대기 중인 그래픽 설정을 버리고 직전 확정 값으로 복구 (되돌리기 버튼 / 시간 초과 / 설정창 닫힘)
	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void RevertAppliedGraphicsSettings();

	// 확인 대기 중인지 (적용 후 유지 / 되돌리기 선택 전)
	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Graphics")
	bool IsWaitingGraphicsConfirm() const { return bWaitingGraphicsConfirm; }

	// 밝기 (즉시 적용 + 저장)

	// 엔진 감마 값. 기본 2.2, MinBrightness ~ MaxBrightness 범위로 제한
	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Graphics")
	float GetBrightness() const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Graphics")
	void SetBrightness(float Value);

	// 게임플레이 (즉시 적용 + 저장)

	// 마우스 감도. 1.0 기본, MinLookSensitivity ~ MaxLookSensitivity 범위로 제한
	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Gameplay")
	float GetLookSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Gameplay")
	void SetLookSensitivity(float Value);

	UFUNCTION(BlueprintPure, Category = "LCO|Settings|Gameplay")
	bool IsInvertLookY() const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Gameplay")
	void SetInvertLookY(bool bInvert);

	// 저장된 감도 / Y축 반전을 캐릭터에 적용 (LCOGameMode가 새 캐릭터 생성 후 호출)
	UFUNCTION(BlueprintCallable, Category = "LCO|Settings|Gameplay")
	void ApplyGameplaySettingsToPlayer(ALCOPlayerCharacter* Player) const;

	// 공통

	// 전체 기본값으로 되돌리고 저장·적용 (감도 1.0 / 반전 끔 / 밝기 2.2 / 그래픽은 엔진 기본값)
	UFUNCTION(BlueprintCallable, Category = "LCO|Settings")
	void ResetToDefaults();

	// 설정 값이 바뀌었을 때 (설정창이 열려 있으면 표시 갱신)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Settings")
	FOnLCOSettingsChanged OnSettingsChanged;

	// 슬라이더 범위 (설정창 슬라이더 Min / Max와 맞출 것)
	static constexpr float MinLookSensitivity = 0.1f;
	static constexpr float MaxLookSensitivity = 5.f;
	static constexpr float MinBrightness = 1.5f;
	static constexpr float MaxBrightness = 3.f;

private:
	// LCO_Settings 슬롯에서 불러오기 (없으면 기본값으로 생성)
	void LoadSettings();

	// LCO_Settings 슬롯에 저장
	void SaveSettings() const;

	// 엔진 감마에 밝기 적용
	void ApplyBrightness() const;

	// 지금 조종 중인 캐릭터 (메인 메뉴 등 캐릭터가 없으면 nullptr)
	ALCOPlayerCharacter* GetCurrentPlayer() const;

	// 엔진 그래픽 설정 객체
	static UGameUserSettings* GetEngineSettings();

	// 지금 화면에 적용된 그래픽 값을 "확정 값"으로 기억 (되돌리기 기준)
	void CaptureConfirmedGraphics();

	// 마지막으로 확정된 화면 모드 / 해상도 / 품질 (확인 창에서 되돌릴 때 사용)
	EWindowMode::Type ConfirmedWindowMode = EWindowMode::Windowed;
	FIntPoint ConfirmedResolution = FIntPoint::ZeroValue;
	int32 ConfirmedQuality = 3;

	// 적용 후 유지 / 되돌리기 선택을 기다리는 중
	bool bWaitingGraphicsConfirm = false;

	static const FString SettingsSlotName;
	static constexpr int32 SettingsUserIndex = 0;

	// 불러온 설정 (감도 / 반전 / 밝기)
	UPROPERTY()
	TObjectPtr<ULCOGameSettingsSaveGame> Settings;
};
