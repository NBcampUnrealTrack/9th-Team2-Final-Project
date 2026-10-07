#include "Interaction/LCOOpenableBase.h"
#include "Inventory/InventoryComponent.h"

ALCOOpenableBase::ALCOOpenableBase()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ALCOOpenableBase::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 설정한 시작 상태를 BP 연출에 반영
	OnInitialized(bIsOpen);
}

void ALCOOpenableBase::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	// 잠겨 있고 열쇠도 없으면 잠김 피드백만 주고 종료
	if (bIsLocked && !TryUnlockWithKey(Interactor))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOOpenable] %s is locked"), *GetName());
		OnLockedTry(Interactor);
		return;
	}

	HandleUnlockedInteraction(Interactor);
}

void ALCOOpenableBase::HandleUnlockedInteraction(AActor* Interactor)
{
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

void ALCOOpenableBase::Open()
{
	if (bIsOpen || bIsLocked)
	{
		return;
	}

	bIsOpen = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOOpenable] %s Open"), *GetName());
	OnOpened();
}

void ALCOOpenableBase::Close()
{
	if (!bIsOpen)
	{
		return;
	}

	bIsOpen = false;

	UE_LOG(LogTemp, Log, TEXT("[LCOOpenable] %s Close"), *GetName());
	OnClosed();
}

void ALCOOpenableBase::Unlock()
{
	if (!bIsLocked)
	{
		return;
	}

	bIsLocked = false;

	UE_LOG(LogTemp, Log, TEXT("[LCOOpenable] %s Unlock"), *GetName());
	OnUnlocked();
}

void ALCOOpenableBase::Lock()
{
	bIsLocked = true;

	UE_LOG(LogTemp, Log, TEXT("[LCOOpenable] %s Lock"), *GetName());
}

bool ALCOOpenableBase::TryUnlockWithKey(AActor* Interactor)
{
	// 열쇠가 지정되지 않았으면 열쇠로 열 수 없음
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
