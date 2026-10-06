#include "LCOInteractComponent.h"
#include "LCOInteractionReceiver.h"
#include "Engine/CollisionProfile.h"

ULCOInteractComponent::ULCOInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 플레이어 라인트레이스(Visibility 채널)에 감지되도록 충돌 설정
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	InteractPrompt = NSLOCTEXT("LCOInteraction", "DefaultInteractPrompt", "조사하기");
}

bool ULCOInteractComponent::Interact(AActor* Interactor)
{
	if (!CanInteract(Interactor))
	{
		return false;
	}

	AActor* Target = GetInteractionTarget();

	UE_LOG(LogTemp, Log, TEXT("[LCOInteract] %s -> %s (InputID: %s)"),
		*GetName(), *Target->GetName(), *InputID.ToString());

	OnInteracted.Broadcast(this, Interactor);

	// 대상에 상호작용 전달
	ILCOInteractionReceiver::Execute_HandleInteraction(Target, this, Interactor);
	return true;
}

bool ULCOInteractComponent::CanInteract(AActor* Interactor) const
{
	if (!bInteractionEnabled)
	{
		return false;
	}

	AActor* Target = GetInteractionTarget();
	if (!Target)
	{
		return false;
	}

	return !ILCOInteractionReceiver::Execute_IsInteractionLocked(Target, const_cast<ULCOInteractComponent*>(this), Interactor);
}

void ULCOInteractComponent::SetFocused(bool bFocused)
{
	// CustomDepth를 켜서 외곽선 포스트프로세스 머티리얼에서 사용
	SetRenderCustomDepth(bFocused);
}

void ULCOInteractComponent::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
}

AActor* ULCOInteractComponent::GetInteractionTarget() const
{
	AActor* Target = IsValid(LinkedTarget) ? LinkedTarget.Get() : GetOwner();

	// 대상이 인터페이스를 구현하지 않은 경우 (레벨 배치 시 연결 누락 등)
	if (!IsValid(Target) || !Target->Implements<ULCOInteractionReceiver>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOInteract] %s (%s): Target %s does not implement ILCOInteractionReceiver"),
			*GetName(), *GetNameSafe(GetOwner()), *GetNameSafe(Target));
		return nullptr;
	}

	return Target;
}
