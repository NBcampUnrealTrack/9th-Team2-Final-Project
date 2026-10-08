#include "Player/LCOStaminaComponent.h"

ULCOStaminaComponent::ULCOStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULCOStaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	// 매 프레임이 아니라 정해진 간격마다 계산
	SetComponentTickInterval(StaminaTickInterval);

	CurrentStamina = MaxStamina;
	BroadcastStaminaChanged();
}

void ULCOStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 달리는 중: 감소
	if (bConsuming)
	{
		ApplyStamina(CurrentStamina - DrainPerTick);
		RegenDelayRemaining = RegenDelay;
		return;
	}

	// 소모를 멈춘 직후: 대기
	if (RegenDelayRemaining > 0.f)
	{
		RegenDelayRemaining -= DeltaTime;
		return;
	}

	// 회복
	if (CurrentStamina < MaxStamina)
	{
		ApplyStamina(CurrentStamina + RegenPerTick);
	}
}

void ULCOStaminaComponent::SetConsuming(bool bConsume)
{
	bConsuming = bConsume;
}

void ULCOStaminaComponent::SetCurrentStamina(int32 NewStamina)
{
	ApplyStamina(NewStamina);
}

void ULCOStaminaComponent::RestoreStamina(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	ApplyStamina(CurrentStamina + Amount);
}

void ULCOStaminaComponent::ConsumeStamina(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	ApplyStamina(CurrentStamina - Amount);
	RegenDelayRemaining = RegenDelay;
}

void ULCOStaminaComponent::SetMaxStamina(int32 NewMaxStamina, bool bFillToMax)
{
	MaxStamina = FMath::Max(1, NewMaxStamina);
	CurrentStamina = bFillToMax ? MaxStamina : FMath::Min(CurrentStamina, MaxStamina);

	if (bExhausted && CurrentStamina >= RecoverThreshold)
	{
		bExhausted = false;
	}

	// 최대치가 바뀌면 현재값이 같아도 백분율이 바뀌므로 항상 알림
	BroadcastStaminaChanged();
}

float ULCOStaminaComponent::GetStaminaPercent() const
{
	return MaxStamina > 0 ? static_cast<float>(CurrentStamina) / MaxStamina : 0.f;
}

int32 ULCOStaminaComponent::GetStaminaPercentInt() const
{
	return FMath::RoundToInt(GetStaminaPercent() * 100.f);
}

void ULCOStaminaComponent::ApplyStamina(int32 NewStamina)
{
	const int32 Clamped = FMath::Clamp(NewStamina, 0, MaxStamina);
	if (Clamped == CurrentStamina)
	{
		return;
	}

	CurrentStamina = Clamped;
	BroadcastStaminaChanged();

	// 0이 되면 탈진
	if (CurrentStamina <= 0 && !bExhausted)
	{
		bExhausted = true;
		UE_LOG(LogTemp, Log, TEXT("[LCOStamina] Exhausted"));
		OnStaminaExhausted.Broadcast();
	}
	// 탈진 후 일정량 이상 회복되면 해제
	else if (bExhausted && CurrentStamina >= RecoverThreshold)
	{
		bExhausted = false;
	}
}

void ULCOStaminaComponent::BroadcastStaminaChanged()
{
	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina, GetStaminaPercent());
}