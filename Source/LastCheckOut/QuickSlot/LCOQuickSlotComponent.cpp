#include "QuickSlot/LCOQuickSlotComponent.h"

#include "UI/LCOQuickSlotWidget.h"
#include "Inventory/InventoryComponent.h"
#include "Item/ItemData.h"
#include "Player/LCOPlayerController.h"
#include "Player/LCOPlayerCharacter.h"
#include "CameraCapture/LCOCameraCaptureComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "TimerManager.h"
#include "UI/LCOInventoryWidget.h"
#include "UI/LCODialogueWidget.h"

ULCOQuickSlotComponent::ULCOQuickSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SlotItemIDs.Init(NAME_None, NumSlots);
}

void ULCOQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	// BP에서 배열 크기를 바꿨어도 항상 4칸 유지
	SlotItemIDs.SetNum(NumSlots);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] Owner가 Pawn이 아님 - 플레이어 캐릭터에 붙여주세요"));
		return;
	}

	BindOwnerEvents();

	// 시작 시 이미 들고 있는 소비 아이템도 등록
	AutoAssignFromInventory();

	// 빙의가 BeginPlay보다 늦을 수 있으므로 컨트롤러 변경도 감시
	OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleControllerChanged);
	TrySetupForController(OwnerPawn->GetController());
}

void ULCOQuickSlotComponent::BindOwnerEvents()
{
	// 달리기 / 걷기 상태
	if (ALCOPlayerCharacter* Character = Cast<ALCOPlayerCharacter>(GetOwner()))
	{
		Character->OnSprintStarted.AddUniqueDynamic(this, &ThisClass::HandleSprintStarted);
		Character->OnWalkStarted.AddUniqueDynamic(this, &ThisClass::HandleWalkStarted);
		bIsSprinting = Character->IsSprinting();

		// 손전등이 켜지면 UV 라이트 끄기
		Character->OnFlashlightToggled.AddUniqueDynamic(this, &ThisClass::HandleFlashlightToggled);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] Owner가 LCOPlayerCharacter가 아님 - 달리기 상태 자동 연결 안 됨"));
	}

	// 인벤토리 변경
	if (UInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	}
}

void ULCOQuickSlotComponent::UnbindOwnerEvents()
{
	if (ALCOPlayerCharacter* Character = Cast<ALCOPlayerCharacter>(GetOwner()))
	{
		Character->OnSprintStarted.RemoveDynamic(this, &ThisClass::HandleSprintStarted);
		Character->OnWalkStarted.RemoveDynamic(this, &ThisClass::HandleWalkStarted);
		Character->OnFlashlightToggled.RemoveDynamic(this, &ThisClass::HandleFlashlightToggled);
	}

	if (UInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	}
}

void ULCOQuickSlotComponent::HandleSprintStarted()
{
	HandleSprintStateChanged(true);
}

void ULCOQuickSlotComponent::HandleWalkStarted()
{
	HandleSprintStateChanged(false);
}

void ULCOQuickSlotComponent::HandleInventoryChanged()
{
	// UV 라이트 아이템을 잃으면 끄기
	if (bUVLightOn)
	{
		const UInventoryComponent* Inventory = GetInventory();
		if (!Inventory || !Inventory->HasItem(UVLightItemID))
		{
			SetUVLightEquipped(false);
		}
	}

	// 슬롯 구성이 바뀌면 NotifySlotsChanged 안에서 위젯도 갱신됨
	if (AutoAssignFromInventory())
	{
		NotifySlotsChanged();
		return;
	}

	RefreshWidgets();
}


bool ULCOQuickSlotComponent::AutoAssignFromInventory()
{
    const UInventoryComponent* Inventory = GetInventory();
    if (!Inventory)
    {
        return false;
    }

    bool bChanged = false;

    // 1. 수량이 없거나 퀵슬롯에 넣을 수 없는 아이템을 비운다.
    for (int32 i = 0; i < NumSlots; ++i)
    {
        if (IsPinnedSlot(i))
        {
            continue;
        }

        if (!SlotItemIDs[i].IsNone()
            && (Inventory->GetItemCount(SlotItemIDs[i]) <= 0
                || !CanAssignItem(SlotItemIDs[i])))
        {
            SlotItemIDs[i] = NAME_None;
            bChanged = true;
        }
    }

    // 2. 현재 인벤토리에 존재하는 아이템 목록을 갱신한다.
    TSet<FName> CurrentItems;

    for (const FInventorySlot& InvSlot : Inventory->InventorySlots)
    {
        if (InvSlot.ItemID.IsNone() || InvSlot.Quantity <= 0)
        {
            continue;
        }

        CurrentItems.Add(InvSlot.ItemID);
    }

    // 3. 자동 등록 기능이 켜져 있다면 빈 슬롯을 채운다.
    if (bAutoAssignConsumables)
    {
        for (const FInventorySlot& InvSlot : Inventory->InventorySlots)
        {
            if (InvSlot.ItemID.IsNone() || InvSlot.Quantity <= 0)
            {
                continue;
            }

            // 소비 아이템이 아니면 건너뛴다.
            if (!CanAssignItem(InvSlot.ItemID))
            {
                continue;
            }

            // 이미 다른 퀵슬롯에 등록된 아이템이면 건너뛴다.
            if (FindSlotOfItem(InvSlot.ItemID) != INDEX_NONE)
            {
                continue;
            }

            // 빈 슬롯을 찾는다.
            int32 EmptyIndex = INDEX_NONE;

            for (int32 i = 0; i < NumSlots; ++i)
            {
                if (!IsPinnedSlot(i) && SlotItemIDs[i].IsNone())
                {
                    EmptyIndex = i;
                    break;
                }
            }

            if (EmptyIndex == INDEX_NONE)
            {
                break;
            }

            SlotItemIDs[EmptyIndex] = InvSlot.ItemID;
            bChanged = true;

            UE_LOG(
                LogTemp,
                Log,
                TEXT("[LCOQuickSlot] 자동 등록: %d번 슬롯 = %s"),
                EmptyIndex + 1,
                *InvSlot.ItemID.ToString()
            );
        }
    }

    // 다음 인벤토리 변경을 비교하기 위한 목록을 저장한다.
    KnownInventoryItems = MoveTemp(CurrentItems);

    return bChanged;
}


void ULCOQuickSlotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 휠이 열린 채로 레벨이 끝나도 시간 배율이 남지 않도록
	if (bWheelOpen)
	{
		CloseWheel(false);
	}

	// UV 라이트 정리
	if (UVLightActor)
	{
		SetUVLightEquipped(false);
		UVLightActor->Destroy();
		UVLightActor = nullptr;
	}

	if (APlayerController* PC = CachedPC.Get())
	{
		if (QuickSlotMappingContext)
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(QuickSlotMappingContext);
			}
		}
	}

	UnbindOwnerEvents();
	RemoveWidgets();
	Super::EndPlay(EndPlayReason);
}

// ======================================================================
// 컨트롤러 / 입력 / 위젯 준비
// ======================================================================

void ULCOQuickSlotComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	TrySetupForController(NewController);
}

void ULCOQuickSlotComponent::TrySetupForController(AController* NewController)
{
	APlayerController* PC = Cast<APlayerController>(NewController);
	if (!PC || !PC->IsLocalController() || CachedPC.Get() == PC)
	{
		return;
	}

	CachedPC = PC;
	bInputBound = false;

	// 퀵슬롯 전용 IMC 등록
	if (QuickSlotMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(QuickSlotMappingContext, MappingContextPriority);
		}
	}

	// 컨트롤러의 InputComponent에 Q 바인딩 (PlayerController 파일 수정 없이)
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (EIC && QuickSlotAction)
	{
		EIC->BindAction(QuickSlotAction, ETriggerEvent::Started, this, &ThisClass::HandleWheelStarted);
		EIC->BindAction(QuickSlotAction, ETriggerEvent::Triggered, this, &ThisClass::HandleWheelTriggered);
		EIC->BindAction(QuickSlotAction, ETriggerEvent::Completed, this, &ThisClass::HandleWheelCompleted);
		EIC->BindAction(QuickSlotAction, ETriggerEvent::Canceled, this, &ThisClass::HandleWheelCanceled);
		bInputBound = true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] QuickSlotAction 미지정 또는 EnhancedInputComponent 없음 - Q 휠 비활성"));
	}

	// 카메라를 든 상태에서만 동작하는 촬영 키
	if (EIC && CaptureAction)
	{
		EIC->BindAction(CaptureAction, ETriggerEvent::Started, this, &ThisClass::HandleCapturePressed);
	}
	else if (bPinCameraToFirstSlot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] CaptureAction 미지정 - 카메라를 들어도 촬영 키 없음"));
	}
	
	// 숫자키 1: 카메라 장착
	if (EIC && CameraKeyAction)
	{
		EIC->BindAction(
			CameraKeyAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleCameraKeyPressed
		);
	}

	// 숫자키 2: 손전등 켜기 / 끄기
	if (EIC && FlashlightKeyAction)
	{
		EIC->BindAction(
			FlashlightKeyAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleFlashlightKeyPressed
		);
	}

	// 숫자키 3: UV 라이트 켜기 / 끄기
	if (EIC && UVLightKeyAction)
	{
		EIC->BindAction(
			UVLightKeyAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::HandleUVLightKeyPressed
		);
	}

	CreateWidgets(PC);
}

void ULCOQuickSlotComponent::CreateWidgets(APlayerController* PC)
{
	RemoveWidgets();
	
	// 일반 HUD 퀵슬롯 위젯 생성
	HUDWidget = CreateWidget<ULCOQuickSlotWidget>(PC, HUDWidgetClass);

	if (HUDWidget)
	{
		// HUD는 일반 퀵슬롯 표시 모드로 설정
		HUDWidget->SetWheelMode(false);

		HUDWidget->AddToViewport(HUDZOrder);
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] HUDWidgetClass가 비어 있음"));
	}
	
	// Q 입력 시 표시할 원형 퀵슬롯 휠 위젯 생성
	WheelWidget = CreateWidget<ULCOQuickSlotWidget>(PC, WheelWidgetClass);

	if (WheelWidget)
	{
		// Q 휠은 소비 아이템 전용 표시 모드로 설정
		WheelWidget->SetWheelMode(true);

		WheelWidget->AddToViewport(WheelZOrder);
		WheelWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] WheelWidgetClass가 비어 있음"));
	}

	RefreshWidgets();
	bHUDShown = false;
	UpdateHUDVisibility();

	// 인벤토리 / 대화창 열림은 따로 알려주는 이벤트가 없어서 주기적으로 확인
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HUDCheckTimerHandle, this, &ThisClass::UpdateHUDVisibility, 0.1f, true);
	}
}

void ULCOQuickSlotComponent::RemoveWidgets()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HUDCheckTimerHandle);
	}
	bHUDShown = false;

	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	if (WheelWidget)
	{
		WheelWidget->RemoveFromParent();
		WheelWidget = nullptr;
	}
}

// ======================================================================
// 슬롯 등록 / 조회
// ======================================================================

bool ULCOQuickSlotComponent::AssignSlot(int32 SlotIndex, FName ItemID)
{
	if (!IsValidSlotIndex(SlotIndex))
	{
		return false;
	}

	// 고정 칸(카메라)은 변경 불가
	if (IsPinnedSlot(SlotIndex))
	{
		return false;
	}

	if (ItemID.IsNone())
	{
		ClearSlot(SlotIndex);
		return true;
	}

	// 소비 아이템만 등록 가능 (열쇠, 증거품, DataTable에 없는 아이템은 불가)
	if (!CanAssignItem(ItemID))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] 등록 불가 - 소비 아이템이 아님: %s"), *ItemID.ToString());
		return false;
	}

	// 같은 아이템이 다른 슬롯에 있으면 비움 (한 아이템은 한 슬롯에만)
	const int32 ExistingIndex = FindSlotOfItem(ItemID);
	if (ExistingIndex != INDEX_NONE && ExistingIndex != SlotIndex)
	{
		SlotItemIDs[ExistingIndex] = NAME_None;
	}

	SlotItemIDs[SlotIndex] = ItemID;
	NotifySlotsChanged();
	return true;
}

void ULCOQuickSlotComponent::ClearSlot(int32 SlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex) || SlotItemIDs[SlotIndex].IsNone() || IsPinnedSlot(SlotIndex))
	{
		return;
	}

	SlotItemIDs[SlotIndex] = NAME_None;
	NotifySlotsChanged();
}

void ULCOQuickSlotComponent::SwapSlots(int32 SlotA, int32 SlotB)
{
	if (!IsValidSlotIndex(SlotA) || !IsValidSlotIndex(SlotB) || SlotA == SlotB || IsPinnedSlot(SlotA) || IsPinnedSlot(SlotB))
	{
		return;
	}

	SlotItemIDs.Swap(SlotA, SlotB);
	NotifySlotsChanged();
}

FName ULCOQuickSlotComponent::GetSlotItem(int32 SlotIndex) const
{
	// 슬롯 번호가 유효하지 않으면 빈 아이템 ID를 반환한다.
	if (SlotIndex < 0 || SlotIndex >= SlotItemIDs.Num())
	{
		return NAME_None;
	}

	// 해당 슬롯의 아이템 ID를 반환한다.
	return SlotItemIDs[SlotIndex];
}

int32 ULCOQuickSlotComponent::FindSlotOfItem(FName ItemID) const
{
	if (ItemID.IsNone())
	{
		return INDEX_NONE;
	}

	return SlotItemIDs.IndexOfByKey(ItemID);
}

int32 ULCOQuickSlotComponent::GetSlotItemCount(int32 SlotIndex) const
{
	// 슬롯 번호가 유효하지 않으면 수량 0을 반환한다.
	if (SlotIndex < 0 || SlotIndex >= SlotItemIDs.Num())
	{
		return 0;
	}

	// 슬롯에 아이템이 없으면 수량 0을 반환한다.
	const FName ItemID = SlotItemIDs[SlotIndex];

	if (ItemID.IsNone())
	{
		return 0;
	}

	// 인벤토리에서 실제 아이템 보유 수량을 조회한다.
	const UInventoryComponent* Inventory = GetInventory();

	if (!Inventory)
	{
		return 0;
	}

	return Inventory->GetItemCount(ItemID);
}

bool ULCOQuickSlotComponent::CanAssignItem(FName ItemID) const
{
	if (ItemID.IsNone())
	{
		return false;
	}

	const UInventoryComponent* Inventory = GetInventory();
	const FItemData* ItemData =
		Inventory ? Inventory->GetItemData(ItemID) : nullptr;

	if (!ItemData)
	{
		return false;
	}

	// 효과가 있는 소비 아이템만 퀵슬롯 등록 허용
	return ItemData->ItemType == EItemType::Consumable
		&& !ItemData->BuffEffects.IsEmpty();
}

bool ULCOQuickSlotComponent::IsPinnedSlot(int32 SlotIndex) const
{
	// 카메라는 더 이상 소비 아이템 퀵슬롯에 고정하지 않는다.
	return false;
}
void ULCOQuickSlotComponent::ApplyPinnedSlot()
{
	if (!bPinCameraToFirstSlot || CameraItemID.IsNone())
	{
		return;
	}

	// 다른 칸에 카메라가 들어 있으면 비우고, 1번 칸에 고정 (1번 칸에 있던 소비 아이템은 자동 등록으로 다시 채워짐)
	for (int32 i = 1; i < NumSlots; ++i)
	{
		if (SlotItemIDs[i] == CameraItemID)
		{
			SlotItemIDs[i] = NAME_None;
		}
	}

	SlotItemIDs[0] = CameraItemID;
}

UInventoryComponent* ULCOQuickSlotComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
}

const FItemData* ULCOQuickSlotComponent::GetSlotItemData(int32 SlotIndex) const
{
	const FName ItemID = GetSlotItem(SlotIndex);
	const UInventoryComponent* Inventory = GetInventory();
	return (Inventory && !ItemID.IsNone()) ? Inventory->GetItemData(ItemID) : nullptr;
}

void ULCOQuickSlotComponent::NotifySlotsChanged()
{
	RefreshWidgets();
	OnQuickSlotsChanged.Broadcast();
}

void ULCOQuickSlotComponent::RefreshWidgets()
{
	if (HUDWidget)
	{
		HUDWidget->RefreshSlots(this);
	}

	if (WheelWidget)
	{
		WheelWidget->RefreshSlots(this);
	}
}

// ======================================================================
// 사용
// ======================================================================


bool ULCOQuickSlotComponent::UseSlot(int32 SlotIndex)
{
	// 슬롯 번호가 올바른지 확인한다.
	if (!IsValidSlotIndex(SlotIndex))
	{
		return false;
	}

	// Q 휠 슬롯에서 소비 아이템 ID를 가져온다.
	const FName ItemID = GetSlotItem(SlotIndex);

	// 비어 있는 슬롯이면 사용하지 않는다.
	if (ItemID.IsNone())
	{
		return false;
	}

	// 인벤토리에서 해당 아이템의 보유 수량을 확인한다.
	if (GetSlotItemCount(SlotIndex) <= 0)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[LCOQuickSlot] %d번 슬롯 아이템(%s) 보유 수량 없음"),
			SlotIndex + 1,
			*ItemID.ToString()
		);

		return false;
	}

	// 실제 아이템 효과 적용과 소모는 인벤토리에서 처리한다.
	UInventoryComponent* Inventory = GetInventory();

	if (!Inventory || !Inventory->UseItem(ItemID))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[LCOQuickSlot] %d번 슬롯 아이템(%s) 사용 불가"),
			SlotIndex + 1,
			*ItemID.ToString()
		);

		return false;
	}

	// 아이템 사용 성공을 알린다.
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[LCOQuickSlot] %d번 슬롯 사용: %s"),
		SlotIndex + 1,
		*ItemID.ToString()
	);

	OnQuickSlotItemUsed.Broadcast(SlotIndex, ItemID);

	// UI를 갱신한다.
	RefreshWidgets();

	return true;
}


// ======================================================================
// 달리기 상태 → HUD 표시
// ======================================================================

void ULCOQuickSlotComponent::HandleSprintStateChanged(bool bInIsSprinting)
{
	bIsSprinting = bInIsSprinting;
	UpdateHUDVisibility();
}

void ULCOQuickSlotComponent::UpdateHUDVisibility()
{
	if (!HUDWidget)
	{
		return;
	}

	// 평소엔 HUD 표시, 아래 경우엔 숨김
	// - 달리는 동안
	// - 휠이 열려 있을 때 (휠이 대신 보임)
	// - 인벤토리 / 대화창이 열려 있을 때
	// 카메라를 든 상태(촬영 모드)에서도 숨김 - 뷰파인더 화면을 가리지 않도록
	const bool bShouldShow = !bIsSprinting && !bWheelOpen && !bCameraEquipped && !IsHUDBlockingMenuOpen();

	// 상태가 바뀔 때만 처리 (타이머로 자주 불리므로)
	if (bShouldShow == bHUDShown)
	{
		return;
	}

	bHUDShown = bShouldShow;

	if (bShouldShow)
	{
		HUDWidget->RefreshSlots(this);
		HUDWidget->ShowQuickSlot();
	}
	else
	{
		HUDWidget->HideQuickSlot();
	}
}

bool ULCOQuickSlotComponent::IsHUDBlockingMenuOpen()
{
	// TopLevelOnly = true: 화면(뷰포트)에 올라가 있는 위젯만 찾음 = 열려 있는 창
	TArray<UUserWidget*> FoundWidgets;

	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundWidgets, ULCOInventoryWidget::StaticClass(), true);
	if (FoundWidgets.Num() > 0)
	{
		return true;
	}

	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, FoundWidgets, ULCODialogueWidget::StaticClass(), true);
	return FoundWidgets.Num() > 0;
}

// ======================================================================
// Q 휠
// ======================================================================

void ULCOQuickSlotComponent::HandleWheelStarted()
{
	OpenWheel();
}


void ULCOQuickSlotComponent::HandleWheelTriggered()
{
	APlayerController* PC = CachedPC.Get();
	if (!bWheelOpen || !PC)
	{
		return;
	}

	// 시점 입력은 막혀 있지만 마우스 이동량은 그대로 읽을 수 있음
	float DeltaX = 0.f;
	float DeltaY = 0.f;
	PC->GetInputMouseDelta(DeltaX, DeltaY);

	WheelCursor += FVector2D(DeltaX, DeltaY);

	if (WheelCursor.Size() > WheelCursorMaxRadius)
	{
		WheelCursor = WheelCursor.GetSafeNormal() * WheelCursorMaxRadius;
	}

	int32 NewIndex = INDEX_NONE;

	if (WheelCursor.Size() >= SelectDeadZone)
	{
		// 위쪽 = 0도, 시계 방향으로 증가
		// 1번: 위, 2번: 오른쪽, 3번: 아래,
		// 4번: 왼쪽, 5번: 오른쪽 위
		float AngleDeg = FMath::RadiansToDegrees(
			FMath::Atan2(WheelCursor.X, WheelCursor.Y)
		);

		if (AngleDeg < 0.f)
		{
			AngleDeg += 360.f;
		}

		// 각 슬롯의 중심 각도
		const float SlotAngles[5] =
		{
			0.f,    // 1번: 위
			90.f,   // 2번: 오른쪽
			180.f,  // 3번: 아래
			270.f,  // 4번: 왼쪽
			45.f    // 5번: 오른쪽 위
		};

		// 마우스 방향과 가장 가까운 슬롯을 선택
		float BestDifference = TNumericLimits<float>::Max();

		for (int32 i = 0; i < NumSlots; ++i)
		{
			float Difference = FMath::Abs(AngleDeg - SlotAngles[i]);

			// 0도와 360도의 경계도 올바르게 계산
			Difference = FMath::Min(Difference, 360.f - Difference);

			if (Difference < BestDifference)
			{
				BestDifference = Difference;
				NewIndex = i;
			}
		}
	}

	// 선택이 변경되었을 때만 UI 갱신
	if (NewIndex != SelectedIndex)
	{
		SelectedIndex = NewIndex;

		if (WheelWidget)
		{
			WheelWidget->SetSelectedIndex(SelectedIndex);
		}
	}
}


void ULCOQuickSlotComponent::HandleWheelCompleted()
{
	CloseWheel(true);
}

void ULCOQuickSlotComponent::HandleWheelCanceled()
{
	CloseWheel(false);
}

void ULCOQuickSlotComponent::OpenWheel()
{
	APlayerController* PC = CachedPC.Get();
	if (bWheelOpen || !PC || !WheelWidget)
	{
		return;
	}

	// 메뉴(아이템/수첩/옵션)가 열려 있거나 일시정지 중이면 무시
	if (const ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
	{
		if (LCOPC->IsInUIMode())
		{
			return;
		}
	}

	if (UGameplayStatics::IsGamePaused(this))
	{
		return;
	}

	bWheelOpen = true;
	WheelCursor = FVector2D::ZeroVector;
	SelectedIndex = INDEX_NONE;

	// 휠이 열린 동안 시점 고정 (SetIgnoreLookInput은 카운트 방식이라 닫을 때 false로 한 번 되돌림)
	PC->SetIgnoreLookInput(true);

	// 슬로우 모션
	UGameplayStatics::SetGlobalTimeDilation(this, WheelTimeDilation);

	WheelWidget->RefreshSlots(this);
	WheelWidget->SetSelectedIndex(INDEX_NONE);
	WheelWidget->ShowQuickSlot();

	UpdateHUDVisibility();
}

void ULCOQuickSlotComponent::CloseWheel(bool bUseSelected)
{
	if (!bWheelOpen)
	{
		return;
	}

	bWheelOpen = false;

	if (APlayerController* PC = CachedPC.Get())
	{
		PC->SetIgnoreLookInput(false);
	}

	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);

	if (WheelWidget)
	{
		WheelWidget->HideQuickSlot();
	}

	const int32 IndexToUse = SelectedIndex;
	SelectedIndex = INDEX_NONE;
	WheelCursor = FVector2D::ZeroVector;

	if (bUseSelected && IndexToUse != INDEX_NONE)
	{
		UseSlot(IndexToUse);
	}
	else if (bUseSelected)
	{
		// 휠 가운데(선택 없음)에서 떼면 맨손으로
		SetCameraEquipped(false);
	}

	UpdateHUDVisibility();
}

// ======================================================================
// 카메라 (1번 고정 칸)
// ======================================================================

void ULCOQuickSlotComponent::SetCameraEquipped(bool bEquip)
{
	if (bEquip && !bPinCameraToFirstSlot)
	{
		return;
	}

	if (bCameraEquipped == bEquip)
	{
		return;
	}

	bCameraEquipped = bEquip;
	UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] %s"), bCameraEquipped ? TEXT("카메라 들기 (촬영 모드)") : TEXT("맨손"));

	OnCameraEquipChanged.Broadcast(bCameraEquipped);
	RefreshWidgets();
	UpdateHUDVisibility();
}

void ULCOQuickSlotComponent::HandleCameraKeyPressed()
{
	// Q 휠이 열려 있는 동안에는 장비 입력을 무시한다.
	if (bWheelOpen)
	{
		return;
	}

	// 카메라가 켜져 있으면 끄고, 꺼져 있으면 켜기
	SetCameraEquipped(!bCameraEquipped);
}

void ULCOQuickSlotComponent::HandleFlashlightKeyPressed()
{
	// Q 휠을 사용하는 동안에는 장비 입력을 무시한다.
	if (bWheelOpen)
	{
		return;
	}

	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}

	const FName FlashlightID(TEXT("Flashlight"));

	// 손전등을 획득한 경우에만 사용
	if (!Inventory->HasItem(FlashlightID))
	{
		return;
	}

	// 손전등은 소모하지 않고 켜기 / 끄기
	// (손전등이 켜지면 HandleFlashlightToggled에서 UV 라이트가 자동으로 꺼짐)
	Inventory->UseEquipment(FlashlightID);
}

// ======================================================================
// UV 라이트 (숫자키 3)
// ======================================================================

void ULCOQuickSlotComponent::HandleUVLightKeyPressed()
{
	// Q 휠을 사용하는 동안에는 장비 입력을 무시한다.
	if (bWheelOpen)
	{
		return;
	}

	SetUVLightEquipped(!bUVLightOn);
}

void ULCOQuickSlotComponent::SetUVLightEquipped(bool bOn)
{
	if (bOn == bUVLightOn)
	{
		return;
	}

	if (bOn)
	{
		// UV 라이트 아이템을 획득한 경우에만 켤 수 있음
		const UInventoryComponent* Inventory = GetInventory();
		if (!Inventory || !Inventory->HasItem(UVLightItemID))
		{
			return;
		}

		AActor* UVLight = GetOrSpawnUVLight();
		if (!UVLight)
		{
			return;
		}

		bUVLightOn = true;

		// 일반 손전등은 끔 (손에는 하나만)
		if (ALCOPlayerCharacter* Character = Cast<ALCOPlayerCharacter>(GetOwner()))
		{
			Character->SetFlashlightOn(false);
		}

		// 모델 보이기 → 빛 켜기
		UVLight->SetActorHiddenInGame(false);
		CallUVLightSwitch(UVLight, true);
	}
	else
	{
		bUVLightOn = false;

		if (UVLightActor)
		{
			// 빛 끄기 → 모델까지 숨기기
			CallUVLightSwitch(UVLightActor, false);
			UVLightActor->SetActorHiddenInGame(true);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] UV 라이트 %s"), bUVLightOn ? TEXT("켜짐") : TEXT("꺼짐"));
	OnUVLightChanged.Broadcast(bUVLightOn);
}

void ULCOQuickSlotComponent::HandleFlashlightToggled(bool bFlashlightOn)
{
	// 일반 손전등이 켜지면 UV 라이트는 끔
	if (bFlashlightOn && bUVLightOn)
	{
		SetUVLightEquipped(false);
	}
}

AActor* ULCOQuickSlotComponent::GetOrSpawnUVLight()
{
	if (UVLightActor)
	{
		return UVLightActor;
	}

	if (!UVLightClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] UVLightClass(BP_UVFlashlight)가 비어 있음"));
		return nullptr;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Cast<APawn>(Owner);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UVLightActor = World->SpawnActor<AActor>(UVLightClass, Owner->GetActorTransform(), SpawnParams);
	if (!UVLightActor)
	{
		return nullptr;
	}

	UVLightActor->SetActorEnableCollision(false);

	// 캐릭터 손전등(Spot Light)과 같은 위치에 부착. 없으면 캐릭터 루트
	USceneComponent* AttachTarget = Owner->FindComponentByClass<USpotLightComponent>();
	if (!AttachTarget)
	{
		AttachTarget = Owner->GetRootComponent();
	}
	UVLightActor->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// 시작은 꺼진 상태 + 모델 숨김
	CallUVLightSwitch(UVLightActor, false);
	UVLightActor->SetActorHiddenInGame(true);
	return UVLightActor;
}

void ULCOQuickSlotComponent::CallUVLightSwitch(AActor* UVLight, bool bOn) const
{
	if (!UVLight)
	{
		return;
	}

	// BP 함수 SetUVLightOn(bool) 찾아서 호출
	UFunction* SwitchFunction = UVLight->FindFunction(UVLightSwitchFunctionName);
	if (!SwitchFunction || SwitchFunction->NumParms != 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] %s에 %s(bool) 함수가 없음"),
			*UVLight->GetName(), *UVLightSwitchFunctionName.ToString());
		return;
	}

	struct FSwitchParams
	{
		bool bOn;
	};

	FSwitchParams Params{ bOn };
	UVLight->ProcessEvent(SwitchFunction, &Params);
}

void ULCOQuickSlotComponent::HandleCapturePressed()
{
	// 맨손 / 휠 열림 / 메뉴 열림 / 일시정지 중에는 무시
	if (!bCameraEquipped || bWheelOpen)
	{
		return;
	}

	if (const ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(CachedPC.Get()))
	{
		if (LCOPC->IsInUIMode())
		{
			return;
		}
	}

	if (UGameplayStatics::IsGamePaused(this))
	{
		return;
	}

	// 결과(성공 팝업 / 실패 문구)는 카메라 컴포넌트 델리게이트 → LCOCameraUISubsystem이 처리
	if (ULCOCameraCaptureComponent* CameraCapture = GetOwner() ? GetOwner()->FindComponentByClass<ULCOCameraCaptureComponent>() : nullptr)
	{
		CameraCapture->Capture();
	}
}