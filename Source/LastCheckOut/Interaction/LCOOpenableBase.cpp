#include "Interaction/LCOOpenableBase.h"
#include "Inventory/InventoryComponent.h"

ALCOOpenableBase::ALCOOpenableBase()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	ClosePrompt = NSLOCTEXT("LCOInteraction", "OpenableClosePrompt", "닫기");
	LockedPrompt = NSLOCTEXT("LCOInteraction", "OpenableLockedPrompt", "잠겨 있다");
	UnlockWithKeyPrompt = NSLOCTEXT("LCOInteraction", "OpenableUnlockWithKeyPrompt", "열쇠로 열기");
}

void ALCOOpenableBase::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 설정한 시작 상태를 BP 연출에 반영
	OnInitialized(bIsOpen);
}

void ALCOOpenableBase::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	// 쿨타임 중이면 입력만 무시 (포커스 / 문구는 유지). 코드에서 직접 부르는 Open() / Close()에는 적용하지 않음
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (InteractionCooldown > 0.f && LastInteractionTime >= 0.f && CurrentTime - LastInteractionTime < InteractionCooldown)
	{
		return;
	}

	LastInteractionTime = CurrentTime;

	// 잠겨 있고 열쇠도 없으면 잠김 피드백만 주고 종료
	if (bIsLocked && !TryUnlockWithKey(Interactor))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOOpenable] %s is locked"), *GetName());
		OnLockedTry(Interactor);
		return;
	}

	HandleUnlockedInteraction(Interactor);
}

bool ALCOOpenableBase::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return bIsOpen && !bCanClose;
}

FText ALCOOpenableBase::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	if (bIsLocked)
	{
		return HasKeyItem(Interactor) ? UnlockWithKeyPrompt : LockedPrompt;
	}

	return bIsOpen ? ClosePrompt : OpenPrompt;
}

FLCOActorSaveState ALCOOpenableBase::SaveActorState_Implementation() const
{
	FLCOActorSaveState State;
	State.bIsOpen = bIsOpen;
	State.bIsLocked = bIsLocked;
	return State;
}

void ALCOOpenableBase::LoadActorState_Implementation(const FLCOActorSaveState& State)
{
	bIsOpen = State.bIsOpen;
	bIsLocked = State.bIsLocked;

	UE_LOG(LogTemp, Log, TEXT("[LCOOpenable] %s LoadActorState (Open: %d, Locked: %d)"), *GetName(), bIsOpen, bIsLocked);

	// 애니메이션 없이 열림 / 닫힘 모습으로 맞춤
	OnInitialized(bIsOpen);
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

bool ALCOOpenableBase::HasKeyItem(const AActor* Interactor) const
{
	// 열쇠가 지정되지 않았으면 열쇠로 열 수 없음
	if (RequiredItemID.IsNone() || !IsValid(Interactor))
	{
		return false;
	}

	const UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>();
	return Inventory && Inventory->HasItem(RequiredItemID);
}

bool ALCOOpenableBase::TryUnlockWithKey(AActor* Interactor)
{
	if (!HasKeyItem(Interactor))
	{
		return false;
	}

	if (bConsumeKeyItem)
	{
		Interactor->FindComponentByClass<UInventoryComponent>()->RemoveItem(RequiredItemID, 1);
	}

	Unlock();
	return true;
}
