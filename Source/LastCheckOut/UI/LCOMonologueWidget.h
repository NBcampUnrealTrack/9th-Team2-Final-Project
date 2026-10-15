#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOMonologueWidget.generated.h"

class UTextBlock;
class UWidget;

USTRUCT()
struct FLCOMonologueRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FText Text;

	UPROPERTY()
	float Duration = -1.0f;
};

/**
 * 독백 위젯 베이스 (WBP_Monologue의 부모)
 * - 생성 / 표시는 ULCOMonologueSubsystem이 처리
 * - 페이드 인 → 유지 → 페이드 아웃, 여러 개가 오면 순서대로 표시
 *
 * [디자이너 배치 - 이름 그대로]
 *   Txt_Monologue   (필수) : 독백 문장 (Auto Wrap Text 권장)
 *   MonologuePanel  (선택) : 배경 패널. 있으면 이 패널 전체를 페이드, 없으면 위젯 전체를 페이드
 * 위젯 전체는 클릭을 막지 않도록 Not Hit-Testable 로 둘 것
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOMonologueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void EnqueueMonologue(const FText& Text, float Duration);
	void ClearAll();
	bool IsShowingOrQueued(const FText& Text) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// BP 연출용 (효과음, 애니메이션 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Monologue")
	void OnMonologueShown(const FText& Text);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Monologue")
	void OnMonologueHidden();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_Monologue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MonologuePanel;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Monologue", meta = (ClampMin = "0.0"))
	float FadeInTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Monologue", meta = (ClampMin = "0.0"))
	float FadeOutTime = 0.4f;

	// 자동 표시 시간 = BaseDuration + 글자 수 × PerCharacterDuration (Min~Max로 제한)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Monologue", meta = (ClampMin = "0.0"))
	float BaseDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Monologue", meta = (ClampMin = "0.0"))
	float PerCharacterDuration = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Monologue", meta = (ClampMin = "0.0"))
	float MinDuration = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Monologue", meta = (ClampMin = "0.0"))
	float MaxDuration = 6.0f;

	// 대기 중인 독백이 이 개수를 넘으면 오래된 것부터 버림
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Monologue", meta = (ClampMin = "1"))
	int32 MaxQueueSize = 3;

private:
	enum class EPhase : uint8 { Idle, FadeIn, Hold, FadeOut };

	void StartNext();
	void HideImmediately();
	void SetFadeOpacity(float Opacity);
	float CalcDuration(const FLCOMonologueRequest& Request) const;

	TArray<FLCOMonologueRequest> Queue;
	FLCOMonologueRequest Current;
	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.0f;
	float HoldDuration = 0.0f;
};
