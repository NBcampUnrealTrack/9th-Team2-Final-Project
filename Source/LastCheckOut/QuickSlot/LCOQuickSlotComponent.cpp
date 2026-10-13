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

	// 1번 칸 카메라 고정
	ApplyPinnedSlot();

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

	// 다 써서 수량이 0이 된 슬롯, 퀵슬롯에 넣을 수 없는 아이템이 든 슬롯은 비움
	for (int32 i = 0; i < NumSlots; ++i)
	{
		// 고정 칸(카메라)은 인벤토리 수량과 무관
		if (IsPinnedSlot(i))
		{
			continue;
		}

		if (!SlotItemIDs[i].IsNone() && (Inventory->GetItemCount(SlotItemIDs[i]) <= 0 || !CanAssignItem(SlotItemIDs[i])))
		{
			SlotItemIDs[i] = NAME_None;
			bChanged = true;
		}
	}

	TSet<FName> CurrentItems;
	for (const FInventorySlot& InvSlot : Inventory->InventorySlots)
	{
		if (InvSlot.ItemID.IsNone() || InvSlot.Quantity <= 0)
		{
			continue;
		}

		CurrentItems.Add(InvSlot.ItemID);

		// 이번에 새로 들어온 소비 아이템만 자동 등록 (직접 뺀 아이템은 다시 채우지 않음)
		if (!bAutoAssignConsumables
			|| KnownInventoryItems.Contains(InvSlot.ItemID)
			|| FindSlotOfItem(InvSlot.ItemID) != INDEX_NONE
			|| !CanAssignItem(InvSlot.ItemID))
		{
			continue;
		}

		const int32 EmptyIndex = SlotItemIDs.IndexOfByKey(NAME_None);
		if (EmptyIndex == INDEX_NONE)
		{
			continue; // 4칸 모두 사용 중 (인벤토리에서 직접 교체 가능)
		}

		SlotItemIDs[EmptyIndex] = InvSlot.ItemID;
		bChanged = true;

		UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] 자동 등록: %d번 슬롯 = %s"), EmptyIndex + 1, *InvSlot.ItemID.ToString());
	}

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

	CreateWidgets(PC);
}

void ULCOQuickSlotComponent::CreateWidgets(APlayerController* PC)
{
	RemoveWidgets();

	if (HUDWidgetClass)
	{
		HUDWidget = CreateWidget<ULCOQuickSlotWidget>(PC, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(HUDZOrder);
			HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] HUDWidgetClass가 비어 있음"));
	}

	if (WheelWidgetClass)
	{
		WheelWidget = CreateWidget<ULCOQuickSlotWidget>(PC, WheelWidgetClass);
		if (WheelWidget)
		{
			WheelWidget->AddToViewport(WheelZOrder);
			WheelWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
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
	return IsValidSlotIndex(SlotIndex) && SlotItemIDs.IsValidIndex(SlotIndex) ? SlotItemIDs[SlotIndex] : NAME_None;
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
	// 카메라는 인벤토리에 없지만 항상 사용 가능
	if (IsPinnedSlot(SlotIndex))
	{
		return 1;
	}

	const FName ItemID = GetSlotItem(SlotIndex);
	const UInventoryComponent* Inventory = GetInventory();
	return (Inventory && !ItemID.IsNone()) ? Inventory->GetItemCount(ItemID) : 0;
}

bool ULCOQuickSlotComponent::CanAssignItem(FName ItemID) const
{
	if (ItemID.IsNone())
	{
		return false;
	}

	const UInventoryComponent* Inventory = GetInventory();
	const FItemData* ItemData = Inventory ? Inventory->GetItemData(ItemID) : nullptr;

	// 직접 사용할 수 있는 소비 아이템만 (BuffType이 None인 카메라 필름 등은 제외)
	return ItemData
		&& ItemData->ItemType == EItemType::Consumable
		&& !ItemData->BuffEffects.IsEmpty();
}

bool ULCOQuickSlotComponent::IsPinnedSlot(int32 SlotIndex) const
{
	return bPinCameraToFirstSlot && SlotIndex == 0 && !CameraItemID.IsNone();
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
	// 1번 고정 칸: 카메라 들기 (이미 들고 있으면 그대로)
	if (IsPinnedSlot(SlotIndex))
	{
		SetCameraEquipped(true);
		return true;
	}

	const FName ItemID = GetSlotItem(SlotIndex);
	if (ItemID.IsNone())
	{
		return false;
	}

	if (GetSlotItemCount(SlotIndex) <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] %d번 슬롯 아이템(%s) 보유 수량 없음"), SlotIndex + 1, *ItemID.ToString());
		return false;
	}

	// 실제 효과 / 소모는 인벤토리가 처리 (증거품, 열쇠 등 사용 불가 아이템은 false)
	UInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Inventory->UseItem(ItemID))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] %d번 슬롯 아이템(%s) 사용 불가"), SlotIndex + 1, *ItemID.ToString());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] %d번 슬롯 사용: %s"), SlotIndex + 1, *ItemID.ToString());
	OnQuickSlotItemUsed.Broadcast(SlotIndex, ItemID);

	// 수량 변화는 OnInventoryChanged로 갱신되지만, 사용 불가 → 가능 등 상태 반영을 위해 한 번 더
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
		// 위쪽 = 0도, 시계방향으로 증가 (오른쪽 90, 아래 180, 왼쪽 270)
		float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(WheelCursor.X, WheelCursor.Y));
		if (AngleDeg < 0.f)
		{
			AngleDeg += 360.f;
		}

		const float SectorSize = 360.f / NumSlots;
		NewIndex = FMath::RoundToInt(AngleDeg / SectorSize) % NumSlots;
	}

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
