#include "InventoryComponent.h"
#include "Player/LCOPlayerCharacter.h"
#include "Notebook/LCONotebookSubsystem.h"

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

	// DataTable에서 아이템 정보 가져오기
	const FItemData* ItemData = GetItemData(ItemID);

	// DataTable에 등록되지 않은 아이템이라면 종료
	if (!ItemData)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("존재하지 않는 아이템입니다: %s"),
			*ItemID.ToString()
		);

		return;
	}

	// 기존 인벤토리에 같은 아이템이 있는지 확인
	for (FInventorySlot& Slot : InventorySlots)
	{
		if (Slot.ItemID == ItemID)
		{
			// 증거품과 열쇠 아이템은 중복 획득 방지
			if (ItemData->ItemType == EItemType::Evidence ||
				ItemData->ItemType == EItemType::KeyItem)
			{
				return;
			}

			// 소비 아이템은 기존 수량에 추가
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
	
	// 증거품이면 수첩에 기록
	// 같은 ID는 NotebookSubsystem에서 중복 등록되지 않음
	if (ItemData->ItemType == EItemType::Evidence && NotebookTable)
	{
		if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (ULCONotebookSubsystem* Notebook =
				GI->GetSubsystem<ULCONotebookSubsystem>())
			{
				Notebook->AddEntryFromTable(NotebookTable, ItemID);
			}
		}
	}
	
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
	// 인벤토리를 소유한 캐릭터 가져오기
	ALCOPlayerCharacter* Character = Cast<ALCOPlayerCharacter>(GetOwner());

	if (!Character)
	{
		return false;
	}
	
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
		{
			// 아이템에 등록된 버프 효과가 있는지 확인
			// 버프가 없는 소비 아이템은 사용할 수 없음
			if (ItemData->BuffEffects.IsEmpty())
			{
				return false;
			}

			// 아이템에 등록된 모든 버프 효과를 순회
			for (const FItemBuffEffect& Effect : ItemData->BuffEffects)
			{
				// 버프 종류에 따라 서로 다른 효과 적용
				switch (Effect.BuffType)
				{
				case EBuffType::MoveSpeed:

					// 이동속도 증가 버프 적용
					if (!Character->ApplyMoveSpeedBuff(
						Effect.BuffAmount,
						Effect.BuffDuration))
					{
						return false;
					}

					break;

				case EBuffType::MaxStamina:

					// 최대 스태미나 증가 버프 적용
					if (!Character->ApplyMaxStaminaBuff(
						static_cast<int32>(Effect.BuffAmount),
						Effect.BuffDuration))
					{
						return false;
					}

					break;
				
				case EBuffType::SenseCharge:
					{
						// 이미 직감 횟수가 최대라면 사용 불가
						if (Character->GetCurrentSenseCharges()
							>= Character->GetMaxSenseCharges())
						{
							return false;
						}

						// 회복량은 정수로 설정
						const int32 Amount =
							FMath::FloorToInt(Effect.BuffAmount);

						if (Amount <= 0)
						{
							return false;
						}

						// 직감 사용 횟수 회복
						Character->RestoreSenseCharges(Amount);

						break;
					}
				
				case EBuffType::None:
				default:

					// 유효하지 않은 버프가 등록되어 있으면 사용 중단
					return false;
				}
				
			}

			// 모든 버프 적용이 끝나면 아이템 1개 소모
			if (!RemoveItem(ItemID, 1))
			{
				return false;
			}

			return true;
		}



	case EItemType::KeyItem:

		// 호텔 평면도 사용
		if (ItemID == FName("HotelFloorPlan"))
		{
			// TODO: 지도 UI 열기
			return true;
		}

		// RoomKey는 일반 UseItem으로 사용하지 않음
		// 문과 상호작용할 때 HasItem()으로 보유 여부 확인
		return false;


	case EItemType::Evidence:

		// 증거품은 직접 사용하는 아이템이 아님
		// 획득 시 Notebook에 등록
		return false;


	default:

		// 정의되지 않은 아이템 종류는 사용할 수 없음
		return false;
	}
}