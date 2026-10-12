
#include "LCOCameraCaptureComponent.h"
#include "LCOCameraCapturePointComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"


ULCOCameraCaptureComponent::ULCOCameraCaptureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// 플레이어가 바라보는 방향에서 촬영 가능한 대상을 찾는다.
ULCOCameraCapturePointComponent*
ULCOCameraCaptureComponent::FindCaptureTarget() const
{
    // 컴포넌트를 소유한 액터를 가져온다.
    // 일반적으로 플레이어 캐릭터가 소유자다.
    AActor* Owner = GetOwner();

    // 소유자가 없으면 대상을 탐색할 수 없다.
    if (!Owner)
    {
        return nullptr;
    }

    // 소유자에게 부착된 카메라 컴포넌트를 찾는다.
    // 카메라의 위치와 정면 방향을 기준으로 촬영 대상을 탐색한다.
    UCameraComponent* Camera =
        Owner->FindComponentByClass<UCameraComponent>();

    // 카메라가 없으면 탐색을 중단한다.
    if (!Camera)
    {
        return nullptr;
    }

    // Line Trace를 실행할 월드 객체를 가져온다.
    UWorld* World = GetWorld();

    if (!World)
    {
        return nullptr;
    }

    // 탐색 시작점은 카메라의 현재 위치다.
    const FVector Start = Camera->GetComponentLocation();

    // 카메라가 바라보는 정면 방향으로 CaptureDistance만큼 탐색한다.
    const FVector End =
        Start + Camera->GetForwardVector() * CaptureDistance;

    // 충돌한 대상의 정보가 저장될 변수다.
    FHitResult HitResult;

    // Line Trace가 플레이어 자신을 감지하지 않도록 제외한다.
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Owner);

    // 카메라 정면 방향으로 Visibility 채널 Line Trace를 실행한다.
    // 첫 번째로 감지된 충돌 대상의 정보를 HitResult에 저장한다.
    const bool bHit = World->LineTraceSingleByChannel(
        HitResult,
        Start,
        End,
        ECC_Visibility,
        QueryParams
    );

    // 아무것도 맞지 않았거나 충돌한 액터가 없다면 실패한다.
    if (!bHit || !HitResult.GetActor())
    {
        return nullptr;
    }

    // 충돌한 액터에 촬영 대상 컴포넌트가 있는지 확인한다.
    // 해당 컴포넌트가 없으면 일반 물체이므로 촬영할 수 없다.
    return HitResult.GetActor()
        ->FindComponentByClass<ULCOCameraCapturePointComponent>();
}

// 인벤토리에 카메라 필름이 남아 있는지 확인한다.
bool ULCOCameraCaptureComponent::HasCameraFilm() const
{
    AActor* Owner = GetOwner();

    // 소유자가 없으면 인벤토리에 접근할 수 없다.
    if (!Owner)
    {
        return false;
    }

    // 플레이어 캐릭터에 부착된 인벤토리 컴포넌트를 찾는다.
    UInventoryComponent* Inventory =
        Owner->FindComponentByClass<UInventoryComponent>();

    // 인벤토리를 찾지 못했다면 필름이 없는 것으로 처리한다.
    if (!Inventory)
    {
        return false;
    }

    // CameraFilmItemID에 해당하는 아이템의 보유 수량을 확인한다.
    // 수량이 1개 이상이면 촬영할 수 있다.
    return Inventory->GetItemCount(CameraFilmItemID) > 0;
}

// 인벤토리에서 카메라 필름 1개를 소모한다.
bool ULCOCameraCaptureComponent::ConsumeFilm()
{
    AActor* Owner = GetOwner();

    // 소유자가 없으면 소모할 수 없다.
    if (!Owner)
    {
        return false;
    }

    // 플레이어 캐릭터의 인벤토리 컴포넌트를 가져온다.
    UInventoryComponent* Inventory =
        Owner->FindComponentByClass<UInventoryComponent>();

    // 인벤토리가 없다면 소모에 실패한다.
    if (!Inventory)
    {
        return false;
    }

    // 필름 수량을 다시 확인한다.
    // 촬영 처리 시점에 필름이 부족한 경우를 방지한다.
    if (Inventory->GetItemCount(CameraFilmItemID) <= 0)
    {
        return false;
    }

    // 카메라 필름 1개를 제거한다.
    // RemoveItem이 성공 여부를 bool로 반환한다는 전제다.
    return Inventory->RemoveItem(CameraFilmItemID, 1);
}

// 지정된 촬영 대상의 촬영을 처리한다.
bool ULCOCameraCaptureComponent::ProcessCapture(
    ULCOCameraCapturePointComponent* CapturePoint)
{
    // 대상이 없거나 이미 촬영했거나 사진 이미지가 없다면 중단한다.
    if (!CapturePoint || !CapturePoint->CanCapture())
    {
        return false;
    }

    // 필름 소모에 성공한 경우에만 촬영을 진행한다.
    // 필름이 부족하면 사진을 획득할 수 없다.
    if (!ConsumeFilm())
    {
        return false;
    }

    // 촬영 성공 상태를 기록한다.
    // 동일한 대상의 중복 획득을 방지한다.
    CapturePoint->MarkCaptured();

    // 사진 획득 이벤트를 전달한다.
    // UI는 사진 표시, 수첩 시스템은 사진 등록에 활용할 수 있다.
    OnPhotoCaptured.Broadcast(
        CapturePoint->CaptureID,
        CapturePoint->PhotoImage,
        CapturePoint->PhotoTitle
    );

    // 촬영 처리가 정상적으로 완료되었음을 반환한다.
    return true;
}

// 플레이어가 촬영 버튼을 눌렀을 때 호출되는 진입 함수다.
bool ULCOCameraCaptureComponent::Capture()
{
    // 현재 카메라가 바라보는 방향에서 촬영 대상을 탐색한다.
    ULCOCameraCapturePointComponent* CapturePoint =
        FindCaptureTarget();

    // 촬영 가능한 대상이 없다면 촬영을 중단한다.
    if (!CapturePoint)
    {
        return false;
    }

    // 필름이 없으면 촬영을 진행하지 않는다.
    if (!HasCameraFilm())
    {
        return false;
    }

    // 대상의 촬영 가능 여부 확인, 필름 소모,
    // 촬영 완료 처리 및 이벤트 전달을 수행한다.
    return ProcessCapture(CapturePoint);
}
