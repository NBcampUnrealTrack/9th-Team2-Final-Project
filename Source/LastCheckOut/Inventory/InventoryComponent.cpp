#include "InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

// 아이템을 인벤토리에 추가
void UInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
	// 유효하지 않은 아이템 ID이거나 추가하려는 수량이 0 이하라면 종료
	if (ItemID.IsNone() || Quantity <= 0)
	{
		return;
	}

	// 기존 인벤토리에 같은 아이템이 있는지 확인
	for (FInventorySlot& Slot : InventorySlots)
	{
		if (Slot.ItemID == ItemID)
		{
			// 같은 아이템이 있다면 기존 수량에 추가
			Slot.Quantity += Quantity;
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

			// 아이템 제거 성공
			return true;
		}
	}

	return false;
}