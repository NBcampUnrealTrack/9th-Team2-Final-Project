#include "InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// DataTable + InventoryComponent가 제대로 연결됐는지 테스트용 로그 추후 캐릭터가 생기면 확인
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// DataTable 연결 확인
	if (ItemDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("DataTable 연결 성공!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DataTable이 연결되지 않았습니다!"));
	}

	// RoomKey 아이템 데이터 확인
	const FItemData* ItemData = GetItemData(FName("RoomKey"));

	if (ItemData)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("아이템 찾기 성공! 이름: %s"),
			*ItemData->ItemName.ToString()
		);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RoomKey 아이템을 찾지 못했습니다!"));
	}
}

// 아이템을 인벤토리에 추가
void UInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
	// 유효하지 않은 아이템 ID이거나 추가하려는 수량이 0 이하라면 종료
	if (ItemID.IsNone() || Quantity <= 0)
	{
		return;
	}
	
	// DataTable에 등록되지 않은 아이템이라면 종료
	if (!GetItemData(ItemID))
	{
		UE_LOG(LogTemp, Warning, TEXT("존재하지 않는 아이템입니다: %s"), *ItemID.ToString());
		return;
	}

	// 기존 인벤토리에 같은 아이템이 있는지 확인
	for (FInventorySlot& Slot : InventorySlots)
	{
		if (Slot.ItemID == ItemID)
		{
			// 같은 아이템이 있다면 기존 수량에 추가
			Slot.Quantity += Quantity;
			
			// 인벤토리 변경 이벤트 발생
			OnInventoryChanged.Broadcast();
			
			return;
		}
	}

	// 같은 아이템이 없다면 새로운 슬롯 생성
	FInventorySlot NewSlot;
	
	// 새 슬롯에 아이템 ID 저장
	NewSlot.ItemID = ItemID;
	
	// 새 슬롯에 아이템 수량 저장
	NewSlot.Quantity = Quantity;
	
	// 인벤토리에 새로운 슬롯 추가
	InventorySlots.Add(NewSlot);
	
	// 인벤토리 변경 이벤트 발생
	OnInventoryChanged.Broadcast();
}

// 특정 아이템을 가지고 있는지 확인
bool UInventoryComponent::HasItem(FName ItemID) const
{
	// 인벤토리의 모든 슬롯 확인
	for (const FInventorySlot& Slot : InventorySlots)
	{
		// 아이템 ID가 일치하고 수량이 1개 이상이면 보유 중
		if (Slot.ItemID == ItemID && Slot.Quantity > 0)
		{
			return true;
		}
	}

	// 해당 아이템을 찾지 못한 경우
	return false;
}

// 특정 아이템의 현재 보유 개수 확인
int32 UInventoryComponent::GetItemCount(FName ItemID) const
{
	// 인벤토리의 모든 슬롯 확인
	for (const FInventorySlot& Slot : InventorySlots)
	{
		// 찾는 아이템과 ID가 일치하면 현재 수량 반환
		if (Slot.ItemID == ItemID)
		{
			return Slot.Quantity;
		}
	}

	// 아이템이 없으면 0 반환
	return 0;
}

// 특정 아이템을 인벤토리에서 제거
bool UInventoryComponent::RemoveItem(FName ItemID, int32 Quantity)
{
	// 제거하려는 수량이 0 이하라면 종료
	if (Quantity <= 0)
	{
		return false;
	}

	// 인벤토리 슬롯을 순서대로 확인
	for (int32 i = 0; i < InventorySlots.Num(); ++i)
	{
		FInventorySlot& Slot = InventorySlots[i];

		// 찾는 아이템과 현재 슬롯의 아이템이 같은지 확인
		if (Slot.ItemID == ItemID)
		{
			// 현재 가지고 있는 수량보다 많이 제거하려는 경우
			if (Slot.Quantity < Quantity)
			{
				return false;
			}

			// 아이템 수량 감소
			Slot.Quantity -= Quantity;

			// 수량이 0이 되면 해당 슬롯 삭제
			if (Slot.Quantity <= 0)
			{
				InventorySlots.RemoveAt(i);
			}

			// 인벤토리 변경 이벤트 발생
			OnInventoryChanged.Broadcast();
			
			// 아이템 제거 성공
			return true;
		}
	}

	return false;
}

// DataTable에서 아이템 정보를 가져오기
const FItemData* UInventoryComponent::GetItemData(FName ItemID) const
{
	// DataTable이 설정되지 않았다면 종료
	if (!ItemDataTable)
	{
		return nullptr;
	}

	// ItemID에 해당하는 Row 찾기
	return ItemDataTable->FindRow<FItemData>(
		ItemID,
		TEXT("GetItemData")
	);
}

// 아이템 사용
bool UInventoryComponent::UseItem(FName ItemID)
{
	// 인벤토리에 해당 아이템이 없으면 사용할 수 없음
	if (!HasItem(ItemID))
	{
		return false;
	}

	// DataTable에서 아이템 정보 가져오기
	const FItemData* ItemData = GetItemData(ItemID);

	// 등록되지 않은 아이템이면 사용할 수 없음
	if (!ItemData)
	{
		return false;
	}

	// 아이템 종류에 따라 사용 방식 분기
	switch (ItemData->ItemType)
	{
	case EItemType::Consumable:

		if (ItemID == FName("CanCoffee"))
		{
			// 이동속도 증가 처리
			// 캐릭터 담당 코드와 연결
		}
		else if (ItemID == FName("RedGinsengCandy"))
		{
			// 최대 스태미나 증가 처리
			// 캐릭터 담당 코드와 연결
		}

		// 소비 아이템은 사용 후 1개 소모
		RemoveItem(ItemID, 1);
		return true;


	case EItemType::KeyItem:

		// 사용 시 지도 UI를 열며 아이템은 소모하지 않음
		if (ItemID == FName("HotelFloorPlan"))
		{
			// 지도 UI 열기
			return true;
		}

		// RoomKey는 일반 UseItem으로 사용하지 않음
		return false;


	case EItemType::Evidence:

		// 증거품은 사용하거나 소모하지 않음
		// 아이템 획득 시 Notebook에 등록하는 방식으로 처리
		return false;


	default:
		// 정의되지 않은 아이템 종류는 사용할 수 없음
		return false;
	}
}