#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCOMonologueSubsystem.generated.h"

class ULCOMonologueWidget;
class UWorld;

/**
 * 주인공 독백 표시 (화면 하단 자막 형태)
 * - 카메라 실패 문구, 감각 능력, 수첩 힌트 등 "주인공의 생각"은 전부 여기로 보냄
 * - 아이템 획득 같은 시스템 알림은 LCONotificationSubsystem 사용
 * - 위젯은 처음 독백을 띄울 때 자동 생성 (수첩보다 위에 표시)
 *
 * 사용 (C++)
 *   ULCOMonologueSubsystem::ShowMonologueByKey(this, TEXT("MONOLOGUE_SENSE_FAIL"));
 *   ULCOMonologueSubsystem::ShowMonologue(this, SomeText);
 * 사용 (BP) : Show Monologue / Show Monologue By Key 노드
 *
 * 설정: Config/DefaultGame.ini
 *   [/Script/LastCheckOut.LCOMonologueSubsystem]
 *   MonologueWidgetClass=/Game/.../WBP_Monologue.WBP_Monologue_C
 */
UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOMonologueSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// 독백 표시. Duration <= 0 이면 글자 수에 맞춰 자동 계산
	// 반환: 독백 UI로 보냈으면 true (위젯 클래스 미설정 등으로 못 띄우면 false → 호출한 쪽에서 기존 방식으로 표시 가능)
	UFUNCTION(BlueprintCallable, Category = "LCO|Monologue", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Duration"))
	static bool ShowMonologue(const UObject* WorldContextObject, const FText& Text, float Duration = -1.0f);

	// UI.csv 키로 독백 표시
	UFUNCTION(BlueprintCallable, Category = "LCO|Monologue", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Duration"))
	static bool ShowMonologueByKey(const UObject* WorldContextObject, FName TextKey, float Duration = -1.0f);

	// 표시 중인 독백과 대기 중인 독백 모두 지우기 (컷신 시작 등)
	UFUNCTION(BlueprintCallable, Category = "LCO|Monologue", meta = (WorldContext = "WorldContextObject"))
	static void ClearMonologues(const UObject* WorldContextObject);

	static ULCOMonologueSubsystem* Get(const UObject* WorldContextObject);

	bool Show(const FText& Text, float Duration = -1.0f);
	void Clear();

private:
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	ULCOMonologueWidget* GetOrCreateWidget();
	void CleanupWidget();

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Monologue")
	TSoftClassPtr<ULCOMonologueWidget> MonologueWidgetClass;

	// 수첩(6), 카메라 UI보다 위
	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Monologue")
	int32 ViewportZOrder = 50;

	// 같은 문장을 이 시간(초) 안에 다시 요청하면 무시
	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Monologue")
	float RepeatCooldown = 5.0f;

	UPROPERTY(Transient)
	TObjectPtr<ULCOMonologueWidget> MonologueWidget;

	// 문장별 마지막 표시 시각 (반복 방지)
	TMap<FString, double> LastShownTimes;
};
