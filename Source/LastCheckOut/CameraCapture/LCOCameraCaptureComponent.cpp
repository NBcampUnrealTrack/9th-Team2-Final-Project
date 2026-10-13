
#include "LCOCameraCaptureComponent.h"
#include "LCOCameraCapturePointComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Notebook/LCONotebookTypes.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"


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

    // 소유자의 플레이어 컨트롤러를 가져온다.
    const APawn* OwnerPawn = Cast<APawn>(Owner);

    const APlayerController* PC =
        OwnerPawn
            ? Cast<APlayerController>(OwnerPawn->GetController())
            : nullptr;

    // 컨트롤러가 없으면 탐색을 중단한다.
    if (!PC)
    {
        return nullptr;
    }

    // Line Trace를 실행할 월드 객체를 가져온다.
    UWorld* World = GetWorld();

    if (!World)
    {
        return nullptr;
    }

    // 현재 화면의 카메라 위치와 방향을 가져온다.
    FVector ViewLocation;
    FRotator ViewRotation;

    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

    // 카메라와 캐릭터 사이의 거리를 계산한다.
    const float CameraToPawn =
        FVector::Dist(
            ViewLocation,
            OwnerPawn->GetPawnViewLocation()
        );

    // 현재 화면의 카메라 위치에서 탐색을 시작한다.
    const FVector Start = ViewLocation;

    // 화면이 바라보는 방향으로 탐색한다.
    const FVector End =
        Start + ViewRotation.Vector()
            * (CameraToPawn + CaptureDistance);

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


ELCOCameraCaptureResult
ULCOCameraCaptureComponent::ProcessCapture(
    ULCOCameraCapturePointComponent* CapturePoint)
{
    // 1. 촬영 대상 확인
    if (!CapturePoint)
    {
        return ELCOCameraCaptureResult::NoTarget;
    }

    // 2. 이미 촬영한 대상인지 확인
    if (CapturePoint->bHasBeenCaptured)
    {
        return ELCOCameraCaptureResult::AlreadyCaptured;
    }

    // 3. 촬영 가능 상태 확인
    if (!CapturePoint->CanCapture())
    {
        return ELCOCameraCaptureResult::InvalidData;
    }

    // 4. 수첩 DataTable 확인
    if (!NotebookTable)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Camera] NotebookTable이 설정되지 않았습니다.")
        );

        return ELCOCameraCaptureResult::SystemError;
    }

    // 5. 촬영 ID 확인
    const FName EntryID = CapturePoint->CaptureID;

    if (EntryID.IsNone())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Camera] CaptureID가 설정되지 않았습니다.")
        );

        return ELCOCameraCaptureResult::InvalidData;
    }

    // 6. 촬영 ID에 해당하는 수첩 데이터 검색
    const FLCONotebookEntry* NotebookRow =
        NotebookTable->FindRow<FLCONotebookEntry>(
            EntryID,
            TEXT("CameraCapture")
        );

    if (!NotebookRow)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Camera] 수첩 데이터를 찾을 수 없습니다: %s"),
            *EntryID.ToString()
        );

        return ELCOCameraCaptureResult::InvalidData;
    }

    // 7. 사진 이미지 확인
    if (!NotebookRow->Image)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Camera] 사진 이미지가 설정되지 않았습니다: %s"),
            *EntryID.ToString()
        );

        return ELCOCameraCaptureResult::InvalidData;
    }

    // 8. GameInstance 가져오기
    UWorld* World = GetWorld();

    UGameInstance* GameInstance =
        World ? World->GetGameInstance() : nullptr;

    if (!GameInstance)
    {
        return ELCOCameraCaptureResult::SystemError;
    }

    // 9. 수첩 Subsystem 가져오기
    ULCONotebookSubsystem* NotebookSubsystem =
        GameInstance->GetSubsystem<ULCONotebookSubsystem>();

    if (!NotebookSubsystem)
    {
        return ELCOCameraCaptureResult::SystemError;
    }

    // 10. 이미 수첩에 등록된 사진인지 확인
    const bool bAlreadyRegistered =
        NotebookSubsystem->HasEntry(EntryID);

    // 11. 신규 사진의 EntryID 검증
    if (!bAlreadyRegistered &&
        !NotebookRow->EntryID.IsNone() &&
        NotebookRow->EntryID != EntryID)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Camera] EntryID가 일치하지 않습니다: %s"),
            *EntryID.ToString()
        );

        return ELCOCameraCaptureResult::InvalidData;
    }

    // 12. 필름 1개 소모
    if (!ConsumeFilm())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Camera] 필름이 부족하거나 소모에 실패했습니다.")
        );

        return ELCOCameraCaptureResult::NoFilm;
    }

    // 13. 신규 사진만 수첩에 등록
    if (!bAlreadyRegistered)
    {
        if (!NotebookSubsystem->AddEntryFromTable(
                NotebookTable,
                EntryID))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("[Camera] 수첩 등록에 실패했습니다: %s"),
                *EntryID.ToString()
            );

            return ELCOCameraCaptureResult::SystemError;
        }
    }

    // 14. 촬영 완료 상태 기록
    CapturePoint->MarkCaptured();

    // 15. UI에 사진 정보 전달
    OnPhotoCaptured.Broadcast(
        EntryID,
        NotebookRow->Image,
        NotebookRow->Title,
        NotebookRow->Description
    );

    // 16. 촬영 성공
    return ELCOCameraCaptureResult::Success;
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
        OnCaptureFailed.Broadcast(ELCOCameraCaptureResult::NoTarget);
        return false;
    }

    // 필름이 없으면 촬영을 진행하지 않는다.
    if (!HasCameraFilm())
    {
        OnCaptureFailed.Broadcast(ELCOCameraCaptureResult::NoFilm);
        return false;
    }

    // 대상의 촬영 가능 여부 확인, 필름 소모,
    // 촬영 완료 처리 및 이벤트 전달을 수행한다.
    const ELCOCameraCaptureResult Result =
        ProcessCapture(CapturePoint);

    if (Result != ELCOCameraCaptureResult::Success)
    {
        OnCaptureFailed.Broadcast(Result);
        return false;
    }
    
    return true;
}
