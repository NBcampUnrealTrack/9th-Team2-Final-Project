#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspicionComponent.generated.h"

UENUM(BlueprintType)
enum class ELCOSuspicionLevel : uint8
{
	Calm,       // 0~19
	Suspicious, // 20~49
	Alert,      // 50~69
	Danger,     // 70~99
	Max         // 100 (체이싱 시작)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSuspicionChanged, float, NewValue, ELCOSuspicionLevel, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSuspicionMax);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LASTCHECKOUT_API USuspicionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USuspicionComponent();
	
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void AddSuspicion(float Amount);
	
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void ReduceSuspicion(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void ResetSuspicion();

	UFUNCTION(BlueprintPure, Category = "Suspicion")
	float GetSuspicion() const { return Suspicion; }

	UFUNCTION(BlueprintPure, Category = "Suspicion")
	ELCOSuspicionLevel GetSuspicionLevel() const;

	UFUNCTION(BlueprintPure, Category = "Suspicion")
	bool IsMaxReached() const { return bMaxReached; }

	UPROPERTY(BlueprintAssignable, Category = "Suspicion")
	FOnSuspicionChanged OnSuspicionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Suspicion")
	FOnSuspicionMax OnSuspicionMax;
	
	// 대화 중 등에 의심도 증가와 자연 감소를 모두 멈춘다.
	UFUNCTION(BlueprintCallable, Category = "Suspicion")
	void SetFrozen(bool bNewFrozen) { bFrozen = bNewFrozen; }

	UFUNCTION(BlueprintPure, Category = "Suspicion")
	bool IsFrozen() const { return bFrozen; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, Category = "Suspicion")
	float MaxSuspicion = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Suspicion")
	float DecayDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Suspicion")
	float DecayRate = 5.f;

private:
	void SetSuspicion(float NewValue);

	bool bFrozen = false;
	float Suspicion = 0.f;
	float TimeSinceLastChange = 0.f;
	bool bMaxReached = false;
};
