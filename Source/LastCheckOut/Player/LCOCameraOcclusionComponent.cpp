#include "Player/LCOCameraOcclusionComponent.h"
#include "Components/MeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ULCOCameraOcclusionComponent::ULCOCameraOcclusionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 카메라 위치가 갱신된 뒤에 검사해야 한 프레임 늦게 숨겨지지 않음
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void ULCOCameraOcclusionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector PawnEyeLocation = Pawn->GetPawnViewLocation();

	UpdateSelf(CameraLocation, PawnEyeLocation);

	if (bHideOccludingPawns)
	{
		UpdateOccluders(CameraLocation, PawnEyeLocation);
	}
}

void ULCOCameraOcclusionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 숨긴 상태로 남지 않도록 전부 복구
	SetSelfHidden(false);
	RestoreAllOccluders();

	Super::EndPlay(EndPlayReason);
}

// ---------- 내 몸 ----------

void ULCOCameraOcclusionComponent::UpdateSelf(const FVector& CameraLocation, const FVector& PawnEyeLocation)
{
	const bool bShouldHide = FVector::DistSquared(CameraLocation, PawnEyeLocation) < FMath::Square(SelfHideDistance);
	SetSelfHidden(bShouldHide);
}

void ULCOCameraOcclusionComponent::SetSelfHidden(bool bHide)
{
	if (bSelfHidden == bHide || !GetOwner())
	{
		return;
	}

	bSelfHidden = bHide;

	// 캐릭터 메시 + 붙어 있는 메시(소품 등) 전부
	TArray<UMeshComponent*> Meshes;
	GetOwner()->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		Mesh->SetRenderInMainPass(!bHide);
	}
}

// ---------- NPC 등 다른 Pawn ----------

void ULCOCameraOcclusionComponent::UpdateOccluders(const FVector& CameraLocation, const FVector& PawnEyeLocation)
{
	// 카메라 → 캐릭터 눈 위치로 구체를 쓸어서 사이에 있는 Pawn 찾기
	// 카메라가 NPC 안에 들어가 있는 경우도 시작 지점 겹침으로 잡힘
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOCameraOcclusion), false, GetOwner());
	GetWorld()->SweepMultiByObjectType(
		Hits,
		CameraLocation,
		PawnEyeLocation,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(ProbeRadius),
		Params);

	// 이번 프레임에 숨겨야 할 메시 목록
	TArray<UPrimitiveComponent*> NewHidden;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || HitActor == GetOwner())
		{
			continue;
		}

		TArray<UMeshComponent*> Meshes;
		HitActor->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			NewHidden.AddUnique(Mesh);
		}
	}

	// 더 이상 가리지 않는 메시는 다시 보이게
	for (const TWeakObjectPtr<UPrimitiveComponent>& Weak : HiddenOccluders)
	{
		if (Weak.IsValid() && !NewHidden.Contains(Weak.Get()))
		{
			Weak->SetRenderInMainPass(true);
		}
	}

	// 새로 가리는 메시 숨김
	TArray<TWeakObjectPtr<UPrimitiveComponent>> NewHiddenWeak;
	for (UPrimitiveComponent* Comp : NewHidden)
	{
		Comp->SetRenderInMainPass(false);
		NewHiddenWeak.Add(Comp);
	}

	HiddenOccluders = MoveTemp(NewHiddenWeak);
}

void ULCOCameraOcclusionComponent::RestoreAllOccluders()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Weak : HiddenOccluders)
	{
		if (Weak.IsValid())
		{
			Weak->SetRenderInMainPass(true);
		}
	}

	HiddenOccluders.Empty();
}