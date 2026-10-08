#include "Interaction/LCOContainer.h"
#include "Inventory/InventoryComponent.h"
#include "Engine/DataTable.h"

ALCOContainer::ALCOContainer()
{
	// 아이템 정보 DataTable 기본 경로 (Content/Data/DT_ItemData)
	ItemDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_ItemData/DT_ItemData.DT_ItemData")));
}

bool ALCOContainer::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 열려 있는 동안(UI 사용 중)에는 상호작용 불가
	return bBlockInteractionWhileOpen && IsOpen();
}

FText ALCOContainer::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	if (!IsLocked() && IsOpen())
	{
		return OpenPrompt;
	}

	return Super::GetInteractionPrompt_Implementation(SourceComponent, Interactor);
}

void ALCOContainer::HandleUnlockedInteraction(AActor* Interactor)
{
	if (!IsOpen())
	{
		Open();
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOContainer] %s Opened by %s (Items: %d)"),
		*GetName(), *GetNameSafe(Interactor), ContainedItems.Num());

	OnContainerOpened(Interactor);
	OnContainerOpenedDelegate.Broadcast(this, Interactor);
}

bool ALCOContainer::TakeItem(AActor* Interactor, FName ItemID, int32 Quantity)
{
	if (!IsValid(Interactor) || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	// 보관 목록에서 해당 아이템 항목 찾기
	const int32 ItemIndex = ContainedItems.IndexOfByPredicate([ItemID](const FLCOContainerItem& Item)
	{
		return Item.ItemID == ItemID;
	});

	if (ItemIndex == INDEX_NONE || ContainedItems[ItemIndex].Quantity < Quantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOContainer] %s TakeItem failed: not enough %s"), *GetName(), *ItemID.ToString());
		return false;
	}

	UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOContainer] %s TakeItem failed: %s has no InventoryComponent"),
			*GetName(), *GetNameSafe(Interactor));
		return false;
	}

	// AddItem은 DataTable에 없는 아이템을 무시하므로, 창고에서 빼기 전에 먼저 확인
	// (확인하지 않으면 인벤토리에는 안 들어가고 창고에서만 사라짐)
	if (!Inventory->GetItemData(ItemID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOContainer] %s TakeItem failed: %s is not in inventory ItemDataTable (or ItemDataTable is not set)"),
			*GetName(), *ItemID.ToString());
		return false;
	}

	Inventory->AddItem(ItemID, Quantity);

	// 보관 수량 감소, 0이 되면 항목 삭제
	ContainedItems[ItemIndex].Quantity -= Quantity;
	if (ContainedItems[ItemIndex].Quantity <= 0)
	{
		ContainedItems.RemoveAt(ItemIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOContainer] %s ItemTaken: %s x%d"), *GetName(), *ItemID.ToString(), Quantity);

	OnItemTaken(Interactor, ItemID, Quantity);
	OnItemTakenDelegate.Broadcast(this, Interactor, ItemID, Quantity);
	return true;
}

TArray<FName> ALCOContainer::GetItemIDOptions() const
{
	if (const UDataTable* Table = ItemDataTable.LoadSynchronous())
	{
		return Table->GetRowNames();
	}

	return TArray<FName>();
}
