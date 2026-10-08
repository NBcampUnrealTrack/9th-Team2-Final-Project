#include "QuickSlot/LCOQuickSlotComponent.h"

#include "UI/LCOQuickSlotWidget.h"
#include "Inventory/InventoryComponent.h"
#include "Item/ItemData.h"
#include "Player/LCOPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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

	// 빙의가 BeginPlay보다 늦을 수 있으므로 컨트롤러 변경도 감시
	OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleControllerChanged);
	TrySetupForController(OwnerPawn->GetController());
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
	UpdateHUDVisibility();
}

void ULCOQuickSlotComponent::RemoveWidgets()
{
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

	if (ItemID.IsNone())
	{
		ClearSlot(SlotIndex);
		return true;
	}

	// DataTable에 없는 아이템은 등록 불가
	const UInventoryComponent* Inventory = GetInventory();
	if (!Inventory || !Inventory->GetItemData(ItemID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOQuickSlot] 등록 실패 - 존재하지 않는 아이템: %s"), *ItemID.ToString());
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
	if (!IsValidSlotIndex(SlotIndex) || SlotItemIDs[SlotIndex].IsNone())
	{
		return;
	}

	SlotItemIDs[SlotIndex] = NAME_None;
	NotifySlotsChanged();
}

void ULCOQuickSlotComponent::SwapSlots(int32 SlotA, int32 SlotB)
{
	if (!IsValidSlotIndex(SlotA) || !IsValidSlotIndex(SlotB) || SlotA == SlotB)
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
	const FName ItemID = GetSlotItem(SlotIndex);
	const UInventoryComponent* Inventory = GetInventory();
	return (Inventory && !ItemID.IsNone()) ? Inventory->GetItemCount(ItemID) : 0;
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

	UE_LOG(LogTemp, Log, TEXT("[LCOQuickSlot] %d번 슬롯 사용: %s"), SlotIndex + 1, *ItemID.ToString());
	OnQuickSlotItemUsed.Broadcast(SlotIndex, ItemID);

	// 사용 처리에서 수량이 바뀌었을 수 있으므로 갱신
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

	// 휠이 열려 있을 땐 휠이 대신 보이므로 HUD는 숨김
	if (bIsSprinting && !bWheelOpen)
	{
		HUDWidget->RefreshSlots(this);
		HUDWidget->ShowQuickSlot();
	}
	else
	{
		HUDWidget->HideQuickSlot();
	}
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

	UpdateHUDVisibility();
}
