#include "Interaction/LCOCasterWheelComponent.h"
#include "GameFramework/Actor.h"

ULCOCasterWheelComponent::ULCOCasterWheelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 이동이 끝난 뒤의 위치로 계산하도록 물리 / 이동 처리 후에 Tick
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void ULCOCasterWheelComponent::BeginPlay()
{
	Super::BeginPlay();

	for (FLCOCasterWheel& Wheel : Wheels)
	{
		Wheel.Swivel = FindSceneComponent(Wheel.SwivelComponentName);
		Wheel.Spin = FindSceneComponent(Wheel.SpinComponentName);

		if (Wheel.Swivel.IsValid())
		{
			Wheel.SwivelRestRotation = Wheel.Swivel->GetRelativeRotation().Quaternion();
		}
		if (Wheel.Spin.IsValid())
		{
			Wheel.SpinRestRotation = Wheel.Spin->GetRelativeRotation().Quaternion();
		}
	}

	ResetWheels();
}

void ULCOCasterWheelComponent::ResetWheels()
{
	for (FLCOCasterWheel& Wheel : Wheels)
	{
		Wheel.SwivelYaw = 0.f;
		Wheel.SpinAngle = 0.f;
		ApplyWheel(Wheel);
	}

	bHasLastLocation = false;
	LocalVelocity = FVector::ZeroVector;
}

void ULCOCasterWheelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (!Owner || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector Location = Owner->GetActorLocation();
	if (!bHasLastLocation)
	{
		LastLocation = Location;
		bHasLastLocation = true;
		return;
	}

	FVector Delta = Location - LastLocation;
	LastLocation = Location;

	// 순간이동은 무시
	if (Delta.SizeSquared() > FMath::Square(TeleportDistance))
	{
		LocalVelocity = FVector::ZeroVector;
		return;
	}

	// 바닥 위 이동만 사용 (위아래 흔들림 제외), 액터 기준 방향으로 변환
	const FVector LocalDelta = Owner->GetActorTransform().InverseTransformVectorNoScale(Delta);
	const FVector PlanarDelta(LocalDelta.X, LocalDelta.Y, 0.f);
	LocalVelocity = PlanarDelta / DeltaTime;

	const float Speed = LocalVelocity.Size();
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(PlanarDelta.Y, PlanarDelta.X));

	for (FLCOCasterWheel& Wheel : Wheels)
	{
		// 캐스터: 이동 방향으로 서서히 회전 (가장 가까운 방향으로)
		if (Wheel.Swivel.IsValid() && Speed >= MinSwivelSpeed)
		{
			const float DeltaYaw = FMath::FindDeltaAngleDegrees(Wheel.SwivelYaw, TargetYaw);
			const float MaxStep = SwivelSpeed * DeltaTime;
			Wheel.SwivelYaw = FRotator::NormalizeAxis(Wheel.SwivelYaw + FMath::Clamp(DeltaYaw, -MaxStep, MaxStep));
		}

		// 굴러가기: 바퀴가 향한 방향으로 움직인 거리만큼
		if (Wheel.Spin.IsValid())
		{
			const float WheelYawRad = FMath::DegreesToRadians(Wheel.SwivelYaw);
			const FVector WheelForward(FMath::Cos(WheelYawRad), FMath::Sin(WheelYawRad), 0.f);
			const float RollDistance = FVector::DotProduct(PlanarDelta, WheelForward);
			Wheel.SpinAngle = FMath::Fmod(Wheel.SpinAngle + FMath::RadiansToDegrees(RollDistance / FMath::Max(Wheel.Radius, 0.1f)), 360.f);
		}

		ApplyWheel(Wheel);
	}
}

void ULCOCasterWheelComponent::ApplyWheel(FLCOCasterWheel& Wheel) const
{
	if (USceneComponent* Swivel = Wheel.Swivel.Get())
	{
		// 원래 자세에 세로축 회전만 더함
		const FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(Wheel.SwivelYaw));
		Swivel->SetRelativeRotation(YawQuat * Wheel.SwivelRestRotation);
	}

	if (USceneComponent* Spin = Wheel.Spin.Get())
	{
		// 원래 자세에 바퀴 축 회전만 더함
		const FVector Axis = SpinAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0.f, 1.f, 0.f));
		const FQuat SpinQuat(Axis, FMath::DegreesToRadians(Wheel.SpinAngle));
		Spin->SetRelativeRotation(Wheel.SpinRestRotation * SpinQuat);
	}
}

USceneComponent* ULCOCasterWheelComponent::FindSceneComponent(FName ComponentName) const
{
	if (ComponentName.IsNone() || !GetOwner())
	{
		return nullptr;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(GetOwner());
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component->GetFName() == ComponentName)
		{
			return Component;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCOCasterWheel] %s: component '%s' not found"), *GetNameSafe(GetOwner()), *ComponentName.ToString());
	return nullptr;
}
