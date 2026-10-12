#include "AI/SuspicionComponent.h"

USuspicionComponent::USuspicionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USuspicionComponent::AddSuspicion(float Amount)
{
	if (Amount <= 0.f || bMaxReached || bFrozen)
	{
		return;
	}

	TimeSinceLastChange = 0.f;
	SetSuspicion(Suspicion + Amount);
}

void USuspicionComponent::ReduceSuspicion(float Amount)
{
	if (Amount <= 0.f || bMaxReached || bFrozen)
	{
		return;
	}

	TimeSinceLastChange = 0.f;
	SetSuspicion(Suspicion - Amount);
}

void USuspicionComponent::ResetSuspicion()
{
	bMaxReached = false;
	TimeSinceLastChange = 0.f;
	SetSuspicion(0.f);
}

ELCOSuspicionLevel USuspicionComponent::GetSuspicionLevel() const
{
	if (Suspicion >= MaxSuspicion)
	{
		return ELCOSuspicionLevel::Chasing;
	}
	if (Suspicion >= 70.f)
	{
		return ELCOSuspicionLevel::Danger;
	}
	if (Suspicion >= 50.f)
	{
		return ELCOSuspicionLevel::Alert;
	}
	if (Suspicion >= 20.f)
	{
		return ELCOSuspicionLevel::Suspicious;
	}
	return ELCOSuspicionLevel::Calm;
}

void USuspicionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	/*if (Suspicion <= 0.f || bMaxReached || bFrozen)
	{
		return;
	}

	TimeSinceLastChange += DeltaTime;
	if (TimeSinceLastChange >= DecayDelay)
	{
		SetSuspicion(Suspicion - DecayRate * DeltaTime);
	}*/
}

void USuspicionComponent::SetSuspicion(float NewValue)
{
	
	NewValue = FMath::Clamp(NewValue, 0.f, MaxSuspicion);
	if (FMath::IsNearlyEqual(NewValue, Suspicion))
	{
		return;
	}

	Suspicion = NewValue;
	OnSuspicionChanged.Broadcast(Suspicion, GetSuspicionLevel());

	// 100 도달은 한 번만 알린다.
	if (Suspicion >= MaxSuspicion && !bMaxReached)
	{
		bMaxReached = true;
		OnSuspicionMax.Broadcast();
	}
}
