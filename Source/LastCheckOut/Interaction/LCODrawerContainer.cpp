#include "Interaction/LCODrawerContainer.h"
#include "Interaction/LCOInteractComponent.h"

ALCODrawerContainer::ALCODrawerContainer()
{
	DrawerOpenPrompt = NSLOCTEXT("LCOInteraction", "DrawerOpenPrompt", "서랍 열기");
	DrawerClosePrompt = NSLOCTEXT("LCOInteraction", "DrawerClosePrompt", "서랍 닫기");
	ContentsPrompt = NSLOCTEXT("LCOInteraction", "DrawerContentsPrompt", "살펴보기");
}

void ALCODrawerContainer::BeginPlay()
{
	Super::BeginPlay();

	OnDrawerInitialized(bDrawerOpen);
}

void ALCODrawerContainer::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	bHandlingContents = IsContentsComponent(SourceComponent);

	// 서랍이 닫혀 있으면 물건은 IsInteractionLocked에서 막히지만, 직접 호출되는 경우 대비
	if (bHandlingContents && !bDrawerOpen)
	{
		bHandlingContents = false;
		return;
	}

	Super::HandleInteraction_Implementation(SourceComponent, Interactor);

	bHandlingContents = false;
}

void ALCODrawerContainer::HandleUnlockedInteraction(AActor* Interactor)
{
	if (bHandlingContents)
	{
		// 창고 UI 열기 (OnContainerOpened)
		Super::HandleUnlockedInteraction(Interactor);
		return;
	}

	if (bDrawerOpen)
	{
		if (bCanClose)
		{
			CloseDrawer();
		}
	}
	else
	{
		OpenDrawer();
	}
}

bool ALCODrawerContainer::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	// 창고 UI 사용 중
	if (Super::IsInteractionLocked_Implementation(SourceComponent, Interactor))
	{
		return true;
	}

	if (IsContentsComponent(SourceComponent))
	{
		return !bDrawerOpen;
	}

	return bDrawerOpen && !bCanClose;
}

FText ALCODrawerContainer::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	if (IsContentsComponent(SourceComponent))
	{
		return ContentsPrompt;
	}

	if (IsLocked())
	{
		return Super::GetInteractionPrompt_Implementation(SourceComponent, Interactor);
	}

	return bDrawerOpen ? DrawerClosePrompt : DrawerOpenPrompt;
}

FLCOActorSaveState ALCODrawerContainer::SaveActorState_Implementation() const
{
	FLCOActorSaveState State = Super::SaveActorState_Implementation();
	State.bIsOpen = bDrawerOpen;
	return State;
}

void ALCODrawerContainer::LoadActorState_Implementation(const FLCOActorSaveState& State)
{
	// 부모는 bIsOpen을 창고 UI 상태로 보고 항상 닫힘으로 복원 → 서랍 상태는 먼저 꺼내 둠
	bDrawerOpen = State.bIsOpen;

	Super::LoadActorState_Implementation(State);

	UE_LOG(LogTemp, Log, TEXT("[LCODrawerContainer] %s LoadActorState (DrawerOpen: %d)"), *GetName(), bDrawerOpen);
	OnDrawerInitialized(bDrawerOpen);
}

void ALCODrawerContainer::OpenDrawer()
{
	if (bDrawerOpen || IsLocked())
	{
		return;
	}

	bDrawerOpen = true;

	UE_LOG(LogTemp, Log, TEXT("[LCODrawerContainer] %s OpenDrawer"), *GetName());
	OnDrawerOpened();
}

void ALCODrawerContainer::CloseDrawer()
{
	// 창고 UI 사용 중에는 닫지 않음
	if (!bDrawerOpen || IsOpen())
	{
		return;
	}

	bDrawerOpen = false;

	UE_LOG(LogTemp, Log, TEXT("[LCODrawerContainer] %s CloseDrawer"), *GetName());
	OnDrawerClosed();
}

bool ALCODrawerContainer::IsContentsComponent(const ULCOInteractComponent* Component) const
{
	return Component && !ContentsInputID.IsNone() && Component->GetInputID() == ContentsInputID;
}
