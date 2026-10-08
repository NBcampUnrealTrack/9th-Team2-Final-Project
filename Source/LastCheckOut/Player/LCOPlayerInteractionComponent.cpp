#include "Player/LCOPlayerInteractionComponent.h"
#include "Interaction/LCOInteractComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ULCOPlayerInteractionComponent::ULCOPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULCOPlayerInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFocus();
}

void ULCOPlayerInteractionComponent::UpdateFocus()
{
	ULCOInteractComponent* Hit = TraceForInteractable();

	// 상호작용 불가 대상(해결된 퍼즐, 비활성화 등)은 포커스하지 않음
	if (Hit && !Hit->CanInteract(GetOwner()))
	{
		Hit = nullptr;
	}

	SetFocus(Hit);
}

void ULCOPlayerInteractionComponent::SetFocus(ULCOInteractComponent* NewFocus)
{
	if (NewFocus == FocusedComponent)
	{
		return;
	}

	if (IsValid(FocusedComponent))
	{
		FocusedComponent->SetFocused(false);
	}

	FocusedComponent = NewFocus;

	if (IsValid(FocusedComponent))
	{
		FocusedComponent->SetFocused(true);
	}

	OnFocusChanged.Broadcast(FocusedComponent);
}

bool ULCOPlayerInteractionComponent::TryInteract()
{
	if (!IsValid(FocusedComponent))
	{
		return false;
	}

	const bool bSuccess = FocusedComponent->Interact(GetOwner());
	
	if (bSuccess)
	{
		RefreshFocus();
	}

	return bSuccess;
}

void ULCOPlayerInteractionComponent::RefreshFocus()
{
	OnFocusChanged.Broadcast(IsValid(FocusedComponent) ? FocusedComponent.Get() : nullptr);
}

ULCOInteractComponent* ULCOPlayerInteractionComponent::TraceForInteractable() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// 3인칭이면 카메라가 캐릭터 뒤에 있으므로, 카메라~캐릭터 거리만큼 더 쏜다
	const FVector PawnEye = Pawn->GetPawnViewLocation();
	const float CameraToPawn = FVector::Dist(ViewLocation, PawnEye);
	const FVector ViewDir = ViewRotation.Vector();
	const FVector End = ViewLocation + ViewDir * (CameraToPawn + InteractDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOPlayerInteractTrace), false, Pawn);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		return nullptr;
	}

	ULCOInteractComponent* InteractComp = Cast<ULCOInteractComponent>(Hit.GetComponent());
	if (!InteractComp)
	{
		return nullptr;
	}

	// 캐릭터 기준 거리 검사 + 캐릭터 뒤쪽 물체 제외 (3인칭 대응)
	const FVector ToHit = Hit.ImpactPoint - PawnEye;
	if (ToHit.SizeSquared() > FMath::Square(InteractDistance) || FVector::DotProduct(ToHit, ViewDir) < 0.f)
	{
		return nullptr;
	}

	return InteractComp;
}