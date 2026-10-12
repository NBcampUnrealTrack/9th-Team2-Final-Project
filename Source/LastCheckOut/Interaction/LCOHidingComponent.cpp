#include "Interaction/LCOHidingComponent.h"
#include "Interaction/LCOHidingSpot.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

ULCOHidingComponent::ULCOHidingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool ULCOHidingComponent::EnterHiding(ALCOHidingSpot* HidingSpot)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !IsValid(HidingSpot) || IsHiding() || HidingSpot->IsOccupied())
	{
		return false;
	}

	CurrentHidingSpot = HidingSpot;

	// 이동 정지 + 이동 입력 무시 (MOVE_None 상태에서는 AddMovementInput이 적용되지 않음)
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	// 옷장 메시와 겹쳐서 밀려나지 않도록 충돌을 끄고, 틈새로 보이지 않도록 메시 숨김
	Character->SetActorEnableCollision(false);
	Character->SetActorHiddenInGame(true);

	// 숨는 위치로 순간이동, 바라볼 방향 설정
	const FTransform HideTransform = HidingSpot->GetHideTransform();
	Character->SetActorLocationAndRotation(HideTransform.GetLocation(), HideTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);

	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(HideTransform.Rotator());
	}

	// 내부 카메라 사용 시 시점 전환 (AActor는 기본적으로 자신의 CameraComponent를 뷰로 사용)
	if (HidingSpot->UsesHidingCamera())
	{
		if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
		{
			PC->SetViewTargetWithBlend(HidingSpot, HidingSpot->GetCameraBlendTime());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOHidingComponent] %s EnterHiding %s"), *GetNameSafe(Character), *HidingSpot->GetName());

	HidingSpot->NotifyEntered(Character);
	OnHidingChanged.Broadcast(true, HidingSpot);
	return true;
}

void ULCOHidingComponent::ExitHiding(bool bForced)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	ALCOHidingSpot* HidingSpot = CurrentHidingSpot.Get();
	if (!Character || !HidingSpot)
	{
		CurrentHidingSpot.Reset();
		return;
	}

	CurrentHidingSpot.Reset();

	// 충돌을 먼저 켜야 TeleportTo가 겹치지 않는 위치를 찾아준다
	Character->SetActorEnableCollision(true);
	Character->SetActorHiddenInGame(false);

	const FTransform ExitTransform = HidingSpot->GetExitTransform();
	if (!Character->TeleportTo(ExitTransform.GetLocation(), ExitTransform.Rotator()))
	{
		// 나갈 위치가 막혀 있으면 (AI가 서 있는 등) 그대로 이동
		UE_LOG(LogTemp, Warning, TEXT("[LCOHidingComponent] %s: ExitPoint of %s is blocked"), *GetNameSafe(Character), *HidingSpot->GetName());
		Character->SetActorLocationAndRotation(ExitTransform.GetLocation(), ExitTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(ExitTransform.Rotator());

		if (HidingSpot->UsesHidingCamera())
		{
			if (APlayerController* PC = Cast<APlayerController>(Controller))
			{
				PC->SetViewTargetWithBlend(Character, HidingSpot->GetCameraBlendTime());
			}
		}
	}

	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	UE_LOG(LogTemp, Log, TEXT("[LCOHidingComponent] %s ExitHiding %s (Forced: %d)"), *GetNameSafe(Character), *HidingSpot->GetName(), bForced);

	HidingSpot->NotifyExited(Character, bForced);
	OnHidingChanged.Broadcast(false, HidingSpot);
}

void ULCOHidingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 숨은 상태로 캐릭터가 사라지면 옷장 점유 상태를 풀어준다
	if (ALCOHidingSpot* HidingSpot = CurrentHidingSpot.Get())
	{
		CurrentHidingSpot.Reset();
		HidingSpot->NotifyExited(GetOwner(), true);
	}

	Super::EndPlay(EndPlayReason);
}
