#include "Player/LCOFootstepComponent.h"
#include "Player/LCOPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"

ULCOFootstepComponent::ULCOFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULCOFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<ALCOPlayerCharacter>(GetOwner());
}

void ULCOFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Character)
	{
		return;
	}

	// 공중, 숨는 중(이동 불가)에는 발소리 없음
	if (!Character->GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}

	const float Speed = Character->GetVelocity().Size2D();
	if (Speed < MinStepSpeed)
	{
		// 멈추면 초기화 (다시 걷기 시작하면 한 걸음 거리 뒤에 첫 발소리)
		DistanceSinceLastStep = 0.f;
		return;
	}

	float StepDistance, Volume, Loudness, Range;
	GetCurrentSettings(StepDistance, Volume, Loudness, Range);

	DistanceSinceLastStep += Speed * DeltaTime;
	if (DistanceSinceLastStep >= StepDistance)
	{
		DistanceSinceLastStep = 0.f;
		PlayFootstep();
	}
}

void ULCOFootstepComponent::PlayFootstep()
{
	if (!Character)
	{
		return;
	}

	float StepDistance, Volume, Loudness, Range;
	GetCurrentSettings(StepDistance, Volume, Loudness, Range);

	// 발 위치 (캡슐 바닥)
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector FootLocation = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);

	if (FootstepSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FootstepSound, FootLocation, Volume);
	}

	// AI 청각 소음 (AI 쪽에서 청각 감지 설정이 있어야 반응함)
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), FootLocation, Loudness, Character, Range, NoiseTag);

	if (bDrawDebugNoise)
	{
		DrawDebugSphere(GetWorld(), FootLocation, Range, 16, FColor::Yellow, false, 0.5f);
	}

	OnFootstep.Broadcast(FootLocation, Loudness);
}

void ULCOFootstepComponent::GetCurrentSettings(float& OutStepDistance, float& OutVolume, float& OutLoudness, float& OutRange) const
{
	if (Character && Character->IsCrouching())
	{
		OutStepDistance = CrouchStepDistance;
		OutVolume = CrouchVolume;
		OutLoudness = CrouchLoudness;
		OutRange = CrouchNoiseRange;
	}
	else if (Character && Character->IsSprinting())
	{
		OutStepDistance = SprintStepDistance;
		OutVolume = SprintVolume;
		OutLoudness = SprintLoudness;
		OutRange = SprintNoiseRange;
	}
	else
	{
		OutStepDistance = WalkStepDistance;
		OutVolume = WalkVolume;
		OutLoudness = WalkLoudness;
		OutRange = WalkNoiseRange;
	}
}