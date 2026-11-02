#include "Item/LCOFieldItem.h"
#include "Interaction/LCOInteractComponent.h"
#include "Inventory/InventoryComponent.h" // FItemData 포함 (GetItemData 반환형)
#include "Engine/DataTable.h"

ALCOFieldItem::ALCOFieldItem()
{
	PrimaryActorTick.bCanEverTick = false;

	// 상호작용 컴포넌트를 루트로 사용 (ALCOOpenableBase는 Scene Root + BP 메시 구조지만, 필드 아이템은 메시 하나라 컴포넌트 자체를 루트로) (기본 Cube 메시, BlockAll 충돌, 외곽선 Self)
	InteractComponent = CreateDefaultSubobject<ULCOInteractComponent>(TEXT("InteractComponent"));
	SetRootComponent(InteractComponent);

	// 아이템 정보 DataTable 기본 경로 (ALCOContainer와 동일)
	ItemDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT/DT_ItemData.DT_ItemData")));

	PickupPromptFormat = NSLOCTEXT("LCOFieldItem", "PickupPrompt", "{ItemName} 줍기");
}

void ALCOFieldItem::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	TryPickUp(Interactor);
}

bool ALCOFieldItem::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 이미 주웠으면 상호작용 불가 (숨김 + 충돌 해제 전 한 프레임 사이의 중복 입력 방지)
	return bPickedUp;
}

FText ALCOFieldItem::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 비어 있으면 InteractComponent의 기본 문구(InteractPrompt)를 사용
	if (PickupPromptFormat.IsEmpty())
	{
		return FText::GetEmpty();
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("ItemName"), GetItemDisplayName());
	return FText::Format(PickupPromptFormat, Args);
}

FText ALCOFieldItem::GetItemDisplayName() const
{
	if (const UDataTable* Table = ItemDataTable.LoadSynchronous())
	{
		if (const FItemData* Data = Table->FindRow<FItemData>(ItemID, TEXT("LCOFieldItem"), false))
		{
			return Data->ItemName;
		}
	}

	return FText::FromName(ItemID);
}

bool ALCOFieldItem::TryPickUp(AActor* Interactor)
{
	if (bPickedUp || !IsValid(Interactor) || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOFieldItem] %s PickUp failed: %s has no InventoryComponent"),
			*GetName(), *GetNameSafe(Interactor));
		return false;
	}

	// ALCOContainer::TakeItem과 같은 이유로 먼저 확인
	// (AddItem은 DataTable에 없는 아이템을 무시하므로, 확인 없이 숨기면 아이템만 사라짐)
	if (!Inventory->GetItemData(ItemID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOFieldItem] %s PickUp failed: %s is not in inventory ItemDataTable (or ItemDataTable is not set)"),
			*GetName(), *ItemID.ToString());
		return false;
	}

	// 캐비닛과 같은 지급 경로 → 증거품도 캐비닛에서 얻을 때와 동일하게 처리된다
	Inventory->AddItem(ItemID, Quantity);

	bPickedUp = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOFieldItem] %s PickedUp: %s x%d by %s"),
		*GetName(), *ItemID.ToString(), Quantity, *GetNameSafe(Interactor));

	OnPickedUp(Interactor);
	OnPickedUpDelegate.Broadcast(this, Interactor, ItemID, Quantity);

	ApplyPickedUpState();
	return true;
}

void ALCOFieldItem::ApplyPickedUpState()
{
	if (InteractComponent)
	{
		InteractComponent->SetFocused(false);
		InteractComponent->SetInteractionEnabled(false);
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

FLCOActorSaveState ALCOFieldItem::SaveActorState_Implementation() const
{
	FLCOActorSaveState State;
	if (!bPickedUp)
	{
		FLCOSavedItem SavedItem;
		SavedItem.ItemID = ItemID;
		SavedItem.Quantity = Quantity;
		State.Items.Add(SavedItem);
	}
	return State;
}

void ALCOFieldItem::LoadActorState_Implementation(const FLCOActorSaveState& State)
{
	// 저장된 남은 아이템이 없으면 이미 주운 것
	bPickedUp = State.Items.IsEmpty();
	if (bPickedUp)
	{
		ApplyPickedUpState();
	}
}

TArray<FName> ALCOFieldItem::GetItemIDOptions() const
{
	if (const UDataTable* Table = ItemDataTable.LoadSynchronous())
	{
		return Table->GetRowNames();
	}

	return TArray<FName>();
}
