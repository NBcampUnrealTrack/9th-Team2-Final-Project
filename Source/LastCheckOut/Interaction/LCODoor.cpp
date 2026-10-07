#include "Interaction/LCODoor.h"
#include "Inventory/InventoryComponent.h"

ALCODoor::ALCODoor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALCODoor::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 설정한 시작 상태를 BP 연출에 반영
	OnDoorInitialized(bIsOpen);
}

void ALCODoor::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	if (bIsLocked)
	{
		// 열쇠가 없으면 잠김 피드백만 주고 종료
		if (!TryUnlockWithKey(Interactor))
		{
			UE_LOG(LogTemp, Log, TEXT("[LCODoor] %s is locked"), *GetName());
			OnDoorLockedTry(Interactor);
			return;
		}

		// 열쇠로 잠금 해제한 경우 바로 열기
		Open();
		return;
	}

	if (bIsOpen)
	{
		if (bCanClose)
		{
			Close();
		}
	}
	else
	{
		Open();
	}
}

void ALCODoor::Open()
{
	if (bIsOpen || bIsLocked)
	{
		return;
	}

	bIsOpen = true;

	UE_LOG(LogTemp, Log, TEXT("[LCODoor] %s Open"), *GetName());
	OnDoorOpened();
}

void ALCODoor::Close()
{
	if (!bIsOpen)
	{
		return;
	}

	bIsOpen = false;

	UE_LOG(LogTemp, Log, TEXT("[LCODoor] %s Close"), *GetName());
	OnDoorClosed();
}

void ALCODoor::Unlock()
{
	if (!bIsLocked)
	{
		return;
	}

	bIsLocked = false;

	UE_LOG(LogTemp, Log, TEXT("[LCODoor] %s Unlock"), *GetName());
	OnDoorUnlocked();
}

void ALCODoor::Lock()
{
	bIsLocked = true;

	UE_LOG(LogTemp, Log, TEXT("[LCODoor] %s Lock"), *GetName());
}

bool ALCODoor::TryUnlockWithKey(AActor* Interactor)
{
	// 열쇠가 지정되지 않은 문은 열쇠로 열 수 없음
	if (RequiredItemID.IsNone() || !IsValid(Interactor))
	{
		return false;
	}

	UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	if (!Inventory || !Inventory->HasItem(RequiredItemID))
	{
		return false;
	}

	if (bConsumeKeyItem)
	{
		Inventory->RemoveItem(RequiredItemID, 1);
	}

	Unlock();
	return true;
}
