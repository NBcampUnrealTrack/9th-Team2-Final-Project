#include "Player/LCOPlayerAnimInstance.h"
#include "Player/LCOPlayerCharacter.h"
#include "Interaction/LCOHidingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void ULCOPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<ALCOPlayerCharacter>(TryGetPawnOwner());
	Movement = Character ? Character->GetCharacterMovement() : nullptr;
}

void ULCOPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 에디터 미리보기 등 캐릭터가 없을 때
	if (!Character || !Movement)
	{
		return;
	}

	const FVector Velocity = Movement->Velocity;
	const FVector Velocity2D(Velocity.X, Velocity.Y, 0.f);

	GroundSpeed = Velocity2D.Size();

	// 이동 입력이 있고 속도가 있을 때만 이동 중 (입력을 뗀 뒤 감속 중에는 대기로 넘어감)
	const bool bHasInput = !Movement->GetCurrentAcceleration().IsNearlyZero();
	bShouldMove = bHasInput && GroundSpeed > MoveThreshold;

	// 몸이 바라보는 방향 기준 이동 방향 (배그식 3인칭 / 1인칭 모두 몸이 카메라를 따라 돌기 때문에 뒷걸음, 게걸음 판정 가능)
	if (GroundSpeed > MoveThreshold)
	{
		Direction = FRotator::NormalizeAxis(Velocity2D.Rotation().Yaw - Character->GetActorRotation().Yaw);
	}

	bIsFalling = Movement->IsFalling();
	bIsCrouching = Character->IsCrouching();
	bIsSprinting = Character->IsSprinting();
	bCameraEquipped = Character->IsCameraEquipped();
	bIsHiding = Character->GetHiding() && Character->GetHiding()->IsHiding();
	bFlashlightOn = Character->IsHoldingLight();
	FlashlightArmAlpha = FMath::FInterpTo(FlashlightArmAlpha, bFlashlightOn ? 1.f : 0.f, DeltaSeconds, ArmBlendSpeed);
	CameraArmAlpha = FMath::FInterpTo(CameraArmAlpha, bCameraEquipped ? 1.f : 0.f, DeltaSeconds, ArmBlendSpeed);
}