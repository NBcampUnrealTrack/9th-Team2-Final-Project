#include "Interaction/Reveal/LCORevealLightComponent.h"
#include "Interaction/Reveal/LCORevealLightSubsystem.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"

ULCORevealLightComponent::ULCORevealLightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULCORevealLightComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetSpotLight())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCORevealLight] %s (%s): no SpotLightComponent found"), *GetName(), *GetNameSafe(GetOwner()));
	}

	if (ULCORevealLightSubsystem* Subsystem = GetWorld()->GetSubsystem<ULCORevealLightSubsystem>())
	{
		Subsystem->RegisterRevealLight(this);
	}
}

void ULCORevealLightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		if (ULCORevealLightSubsystem* Subsystem = World->GetSubsystem<ULCORevealLightSubsystem>())
		{
			Subsystem->UnregisterRevealLight(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ULCORevealLightComponent::SetRevealEnabled(bool bEnabled)
{
	bRevealEnabled = bEnabled;
}

bool ULCORevealLightComponent::IsRevealActive() const
{
	const USpotLightComponent* SpotLight = GetSpotLight();
	return bRevealEnabled && SpotLight && SpotLight->IsVisible() && SpotLight->Intensity > 0.f;
}

USpotLightComponent* ULCORevealLightComponent::GetSpotLight() const
{
	if (IsValid(CachedSpotLight))
	{
		return CachedSpotLight;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (!SpotLightTag.IsNone())
	{
		TArray<UActorComponent*> Tagged = Owner->GetComponentsByTag(USpotLightComponent::StaticClass(), SpotLightTag);
		CachedSpotLight = Tagged.IsEmpty() ? nullptr : Cast<USpotLightComponent>(Tagged[0]);
	}
	else
	{
		CachedSpotLight = Owner->FindComponentByClass<USpotLightComponent>();
	}

	return CachedSpotLight;
}
