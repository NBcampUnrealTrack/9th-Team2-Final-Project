#include "Player/LCOCameraOcclusionComponent.h"
#include "Components/MeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 메인 패스만 끄면 깊이는 계속 그려져서 뒤쪽이 가려진 검은 실루엣이 남음 → 깊이 패스도 같이 끔 (그림자 패스는 별개라 유지)
	void SetRenderHidden(UPrimitiveComponent* Comp, bool bHide)
	{
		Comp->SetRenderInMainPass(!bHide);
		Comp->SetRenderInDepthPass(!bHide);
	}
}

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

	UpdateSelf(CameraLocation, PawnEyeLocation, DeltaTime);

	if (bHideOccludingPawns)
	{
		UpdateOccluders(CameraLocation, PawnEyeLocation);
	}
}

void ULCOCameraOcclusionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 숨긴 상태로 남지 않도록 전부 복구
	SetSelfHidden(false);
	SetSelfOpacity(1.f);
	RestoreAllOccluders();

	Super::EndPlay(EndPlayReason);
}

// ---------- 내 몸 ----------

void ULCOCameraOcclusionComponent::UpdateSelf(const FVector& CameraLocation, const FVector& PawnEyeLocation, float DeltaTime)
{
	const float Distance = FVector::Dist(CameraLocation, PawnEyeLocation);

	// 카메라가 몸 안까지 들어오면 완전히 숨김
	SetSelfHidden(Distance < SelfHideDistance);

	// SelfFadeStartDistance → SelfHideDistance 구간에서 1 → SelfMinOpacity로 점점 투명
	const float FadeRange = FMath::Max(SelfFadeStartDistance - SelfHideDistance, 1.f);
	const float FadeAlpha = FMath::Clamp((Distance - SelfHideDistance) / FadeRange, 0.f, 1.f);
	const float TargetOpacity = FMath::Lerp(SelfMinOpacity, 1.f, FadeAlpha);

	// 목표에 거의 도달하면 딱 맞춰서 1.0에서 디더 구멍이 남지 않게
	float NewOpacity = FMath::FInterpTo(SelfOpacity, TargetOpacity, DeltaTime, SelfFadeSpeed);
	if (FMath::IsNearlyEqual(NewOpacity, TargetOpacity, 0.01f))
	{
		NewOpacity = TargetOpacity;
	}

	SetSelfOpacity(NewOpacity);
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
		// 1인칭 팔 / 손 카메라는 카메라 가림 처리에서 제외 (1인칭에서도 보여야 함)
		if (Mesh->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson)
		{
			continue;
		}
		SetRenderHidden(Mesh, bHide);
	}
}

void ULCOCameraOcclusionComponent::SetSelfOpacity(float Opacity)
{
	// 1인칭 팔 / 손 카메라는 카메라 가림 처리에서 제외 (1인칭에서도 보여야 함)
	if (SelfOpacity == Opacity || !GetOwner())
	{
		return;
	}

	SelfOpacity = Opacity;

	// 캐릭터 메시 + 붙어 있는 메시(손 카메라 등) 전부. CameraFade 파라미터가 없는 머티리얼은 영향 없음
	TArray<UMeshComponent*> Meshes;
	GetOwner()->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		// 1인칭 팔 / 손 카메라는 카메라 가림 처리에서 제외 (1인칭에서도 보여야 함)
		if (Mesh->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson)
		{
			continue;
		}
		Mesh->SetScalarParameterValueOnMaterials(FadeParamName, SelfOpacity);
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
			SetRenderHidden(Weak.Get(), false);
		}
	}

	// 새로 가리는 메시 숨김
	TArray<TWeakObjectPtr<UPrimitiveComponent>> NewHiddenWeak;
	for (UPrimitiveComponent* Comp : NewHidden)
	{
		SetRenderHidden(Comp, true);
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
			SetRenderHidden(Weak.Get(), false);
		}
	}

	HiddenOccluders.Empty();
}