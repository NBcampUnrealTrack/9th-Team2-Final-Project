#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOStaminaComponent.generated.h"

// Percent: 0.0 ~ 1.0 (프로그레스바 Percent에 그대로 연결 가능)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStaminaChanged, int32, CurrentStamina, int32, MaxStamina, float, Percent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStaminaExhausted);

// 플레이어 스태미나 (정수)
// 달리는 동안 일정 간격마다 감소, 달리기를 멈추면 잠시 후 회복
// 0이 되면 탈진 상태가 되고, RecoverThreshold 이상 회복될 때까지 달리기 불가
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOStaminaComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 달리기 소모 시작 / 중지 (캐릭터가 달리기 상태에 맞춰 호출)
	void SetConsuming(bool bConsume);

	// ---------- 스태미나 조절 ----------

	// 현재 스태미나 직접 설정 (0 ~ Max로 자동 보정)
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void SetCurrentStamina(int32 NewStamina);

	// 회복 (캔커피 등 아이템)
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void RestoreStamina(int32 Amount);

	// 소모 (특수 행동 등). 소모 후 RegenDelay 동안 회복 멈춤
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void ConsumeStamina(int32 Amount);

	// 최대 스태미나 변경. bFillToMax가 true면 현재값도 최대로 채움
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void SetMaxStamina(int32 NewMaxStamina, bool bFillToMax = false);

	// ---------- 조회 ----------

	// 달리기 가능 여부 (탈진 상태가 아니고 스태미나가 남아 있을 때)
	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	bool CanSprint() const { return !bExhausted && CurrentStamina > 0; }

	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	int32 GetCurrentStamina() const { return CurrentStamina; }

	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	int32 GetMaxStamina() const { return MaxStamina; }

	// 0.0 ~ 1.0 (프로그레스바용)
	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	float GetStaminaPercent() const;

	// 0 ~ 100 (텍스트 "75%" 표시용)
	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	int32 GetStaminaPercentInt() const;

	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	bool IsExhausted() const { return bExhausted; }

	// ---------- 이벤트 ----------

	// 스태미나 값이 바뀔 때마다 호출 (UI 갱신용)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Stamina")
	FOnStaminaChanged OnStaminaChanged;

	// 스태미나가 0이 되어 탈진했을 때 (숨소리 연출 등)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Stamina")
	FOnStaminaExhausted OnStaminaExhausted;
	
	// -----------------------------------------------------------------------------------------------------------
	
	// 영구 스태미나 자연 회복 배율 설정
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void SetPermanentRegenMultiplier(float Multiplier);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "1"))
	int32 MaxStamina = 100;

	// 계산 간격(초). 이 간격마다 아래 수치만큼 증감
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.01"))
	float StaminaTickInterval = 0.1f;

	// 달리는 동안 간격마다 감소량 (기본: 초당 20 → 약 5초 달리기)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0"))
	int32 DrainPerTick = 2;

	// 회복 중 간격마다 회복량 (기본: 초당 10 → 10초에 완전 회복)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0"))
	int32 RegenPerTick = 1;

	// 소모를 멈춘 뒤 회복이 시작되기까지 대기 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0"))
	float RegenDelay = 1.f;

	// 탈진 후 다시 달릴 수 있게 되는 스태미나
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0"))
	int32 RecoverThreshold = 20;

private:
	// 값 보정 + 탈진 판정 + 이벤트 호출을 한 곳에서 처리
	void ApplyStamina(int32 NewStamina);
	void BroadcastStaminaChanged();

	int32 CurrentStamina = 0;
	bool bConsuming = false;
	bool bExhausted = false;
	float RegenDelayRemaining = 0.f;
	
	// ------------------------------------------------------------------------------------------------------------
	
	// 영구 스태미나 회복 배율 (기본 1.0 = 100%)
	float PermanentRegenMultiplier = 1.0f;

	// 소수점 회복량 누적
	float RegenAccumulator = 0.0f;
};