
#include "LCOCameraCaptureComponent.h"
#include "LCOCameraCapturePointComponent.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
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

    // 12. 신규 사진만 수첩에 등록
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

    // 13. 촬영 완료 상태 기록
    CapturePoint->MarkCaptured();

    // 14. UI에 사진 정보 전달
    OnPhotoCaptured.Broadcast(
        EntryID,
        NotebookRow->Image,
        NotebookRow->Title,
        NotebookRow->Description
    );

    // 15. 촬영 성공
    return ELCOCameraCaptureResult::Success;
}


bool ULCOCameraCaptureComponent::Capture()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        OnCaptureFailed.Broadcast(
            ELCOCameraCaptureResult::SystemError
        );
        return false;
    }

    const float CurrentTime = World->GetTimeSeconds();

    // 쿨타임 중이면 촬영 불가
    if (CurrentTime < NextCaptureAllowedTime)
    {
        OnCaptureFailed.Broadcast(
            ELCOCameraCaptureResult::Cooldown
        );
        return false;
    }

    // 촬영 시도 즉시 쿨타임 적용
    NextCaptureAllowedTime = CurrentTime + CaptureCooldown;

    // 촬영 대상 탐색
    ULCOCameraCapturePointComponent* CapturePoint =
        FindCaptureTarget();

    if (!CapturePoint)
    {
        OnCaptureFailed.Broadcast(
            ELCOCameraCaptureResult::NoTarget
        );
        return false;
    }

    // 실제 촬영 처리
    const ELCOCameraCaptureResult Result =
        ProcessCapture(CapturePoint);

    if (Result != ELCOCameraCaptureResult::Success)
    {
        OnCaptureFailed.Broadcast(Result);
        return false;
    }

    return true;
}
