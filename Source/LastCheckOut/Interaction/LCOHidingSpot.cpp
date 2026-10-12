#include "Interaction/LCOHidingSpot.h"
#include "Interaction/LCOHidingComponent.h"
#include "Camera/CameraComponent.h"

ALCOHidingSpot::ALCOHidingSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	HidePoint = CreateDefaultSubobject<USceneComponent>(TEXT("HidePoint"));
	HidePoint->SetupAttachment(RootComponent);

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(RootComponent);
	ExitPoint->SetRelativeLocation(FVector(100.f, 0.f, 0.f));

	HidingCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("HidingCamera"));
	HidingCamera->SetupAttachment(HidePoint);

	HidePrompt = NSLOCTEXT("LCOInteraction", "HidingSpotHidePrompt", "숨기");
	ExitPrompt = NSLOCTEXT("LCOInteraction", "HidingSpotExitPrompt", "나가기");
}

void ALCOHidingSpot::HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor)
{
	ULCOHidingComponent* HidingComp = FindHidingComponent(Interactor);
	if (!HidingComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOHidingSpot] %s: %s has no LCOHidingComponent"), *GetName(), *GetNameSafe(Interactor));
		return;
	}

	// 안에 숨어 있는 본인이 상호작용하면 나오기
	if (HiddenActor.Get() == Interactor)
	{
		HidingComp->ExitHiding();
		return;
	}

	if (IsOccupied())
	{
		return;
	}

	HidingComp->EnterHiding(this);
}

bool ALCOHidingSpot::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	if (!FindHidingComponent(Interactor))
	{
		return true;
	}

	return IsOccupied() && HiddenActor.Get() != Interactor;
}

FText ALCOHidingSpot::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	return (IsOccupied() && HiddenActor.Get() == Interactor) ? ExitPrompt : HidePrompt;
}

void ALCOHidingSpot::ForceEject()
{
	if (ULCOHidingComponent* HidingComp = FindHidingComponent(HiddenActor.Get()))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOHidingSpot] %s ForceEject %s"), *GetName(), *GetNameSafe(HiddenActor.Get()));
		HidingComp->ExitHiding(true);
	}
}

FTransform ALCOHidingSpot::GetHideTransform() const
{
	return HidePoint->GetComponentTransform();
}

FTransform ALCOHidingSpot::GetExitTransform() const
{
	return ExitPoint->GetComponentTransform();
}

void ALCOHidingSpot::NotifyEntered(AActor* Hider)
{
	HiddenActor = Hider;

	UE_LOG(LogTemp, Log, TEXT("[LCOHidingSpot] %s Entered by %s"), *GetName(), *GetNameSafe(Hider));

	OnHiderEntered(Hider);
	OnEnteredDelegate.Broadcast(this, Hider);
}

void ALCOHidingSpot::NotifyExited(AActor* Hider, bool bForced)
{
	HiddenActor.Reset();

	UE_LOG(LogTemp, Log, TEXT("[LCOHidingSpot] %s Exited by %s (Forced: %d)"), *GetName(), *GetNameSafe(Hider), bForced);

	OnHiderExited(Hider, bForced);
	OnExitedDelegate.Broadcast(this, Hider, bForced);
}

void ALCOHidingSpot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 숨어 있는 상태로 옷장이 사라지면 (레벨 언로드 등) 액터를 밖으로 빼서 갇히지 않게 한다
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		ForceEject();
	}

	Super::EndPlay(EndPlayReason);
}

ULCOHidingComponent* ALCOHidingSpot::FindHidingComponent(const AActor* Actor) const
{
	return IsValid(Actor) ? Actor->FindComponentByClass<ULCOHidingComponent>() : nullptr;
}
