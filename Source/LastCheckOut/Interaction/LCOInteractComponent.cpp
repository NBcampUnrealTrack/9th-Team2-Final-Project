#include "LCOInteractComponent.h"
#include "LCOInteractionReceiver.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ULCOInteractComponent::ULCOInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 플레이어 라인트레이스(Visibility 채널)에 감지되도록 충돌 설정
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// 추가하자마자 보이도록 기본 메시를 Cube로 설정 (BP에서 다른 메시로 변경 가능)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		SetStaticMesh(DefaultMesh.Object);
	}

	InteractPrompt = NSLOCTEXT("LCOInteraction", "DefaultInteractPrompt", "조사하기");
}

bool ULCOInteractComponent::Interact(AActor* Interactor)
{
	// 대상 연결 누락은 실제 상호작용 시점에만 경고 (CanInteract는 매 프레임 호출되므로 로그 생략)
	if (!GetInteractionTarget())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOInteract] %s (%s): Target %s does not implement ILCOInteractionReceiver"),
			*GetName(), *GetNameSafe(GetOwner()), *GetNameSafe(IsValid(LinkedTarget) ? LinkedTarget.Get() : GetOwner()));
		return false;
	}

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

FText ULCOInteractComponent::GetInteractPrompt(AActor* Interactor) const
{
	// 대상이 상태별 문구를 주면 우선 사용
	if (AActor* Target = GetInteractionTarget())
	{
		const FText TargetPrompt = ILCOInteractionReceiver::Execute_GetInteractionPrompt(Target, const_cast<ULCOInteractComponent*>(this), Interactor);
		if (!TargetPrompt.IsEmpty())
		{
			return TargetPrompt;
		}
	}

	return InteractPrompt;
}

void ULCOInteractComponent::SetFocused(bool bFocused)
{
	// CustomDepth를 켜서 외곽선 포스트프로세스 머티리얼에서 사용
	if (HighlightTarget == ELCOHighlightTarget::Self || !GetOwner())
	{
		SetRenderCustomDepth(bFocused);
		return;
	}

	// 소유 액터의 모든 메시 (숨겨진 컴포넌트나 충돌용 컴포넌트는 그려지지 않으므로 같이 켜도 영향 없음)
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(GetOwner());
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		Primitive->SetRenderCustomDepth(bFocused);
	}
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
		return nullptr;
	}

	return Target;
}
