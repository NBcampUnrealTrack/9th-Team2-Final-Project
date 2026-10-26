#include "Player/LCOFootstepComponent.h"
#include "Player/LCOPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"
#include "DrawDebugHelpers.h"
#include "Player/LCOFootstepSurfaceVolume.h"

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

	// 발밑 바닥 종류에 맞는 소리 (카펫 / 타일 / 나무 등)
	if (USoundBase* Sound = FindSurfaceSound(FootLocation))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, FootLocation, Volume);
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

USoundBase* ULCOFootstepComponent::FindSurfaceSound(const FVector& FootLocation) const
{
	// 1. 발소리 볼륨 (맵 구역별 지정)이 있으면 우선
	if (const ALCOFootstepSurfaceVolume* Volume = FindSurfaceVolume())
	{
		if (const TObjectPtr<USoundBase>* Found = SurfaceSounds.Find(Volume->GetSurface()))
		{
			if (*Found)
			{
				return *Found;
			}
		}
	}

	// 2. 발밑 바닥의 Physical Material
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOFootstepSurface), false, GetOwner());
	Params.bReturnPhysicalMaterial = true;

	const FVector Start = FootLocation + FVector(0.f, 0.f, 10.f);
	const FVector End = FootLocation - FVector(0.f, 0.f, SurfaceTraceDistance);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const EPhysicalSurface Surface = UGameplayStatics::GetSurfaceType(Hit);
		if (const TObjectPtr<USoundBase>* Found = SurfaceSounds.Find(Surface))
		{
			if (*Found)
			{
				return *Found;
			}
		}
	}

	// 3. 지정 안 된 곳은 기본 발소리
	return FootstepSound;
}
const ALCOFootstepSurfaceVolume* ULCOFootstepComponent::FindSurfaceVolume() const
{
	TArray<AActor*> Overlapping;
	GetOwner()->GetOverlappingActors(Overlapping, ALCOFootstepSurfaceVolume::StaticClass());

	// 겹친 볼륨이 여러 개면 우선순위가 높은 것
	const ALCOFootstepSurfaceVolume* Best = nullptr;
	for (AActor* Actor : Overlapping)
	{
		const ALCOFootstepSurfaceVolume* Volume = Cast<ALCOFootstepSurfaceVolume>(Actor);
		if (Volume && (!Best || Volume->GetPriority() > Best->GetPriority()))
		{
			Best = Volume;
		}
	}

	return Best;
}
