#include "AI/Activity/LCOActivitySpot.h"

#include "Components/ArrowComponent.h"
#include "Components/TextRenderComponent.h"

ALCOActivitySpot::ALCOActivitySpot()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Root);
	Arrow->ArrowSize = 1.5f;

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(Root);
	NameLabel->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetWorldSize(30.f);
	NameLabel->SetTextRenderColor(FColor::Yellow);
	NameLabel->SetHiddenInGame(true);

	// 기본 동작: 슬롯으로 이동 -> 슬롯 방향으로 돌기 -> 3초 대기
	FLCOActivityStep MoveStep;
	MoveStep.Type = ELCOActivityStepType::MoveToSlot;
	Steps.Add(MoveStep);

	FLCOActivityStep FaceStep;
	FaceStep.Type = ELCOActivityStepType::FaceSlotDirection;
	Steps.Add(FaceStep);

	FLCOActivityStep WaitStep;
	WaitStep.Type = ELCOActivityStepType::Wait;
	WaitStep.Duration = 3.f;
	Steps.Add(WaitStep);
}

bool ALCOActivitySpot::TryReserve(AActor* User)
{
	if (Occupant.IsValid() && Occupant.Get() != User)
	{
		return false;
	}

	Occupant = User;
	return true;
}

void ALCOActivitySpot::Release(AActor* User)
{
	if (Occupant.Get() == User)
	{
		Occupant.Reset();
	}
}

FString ALCOActivitySpot::GetDisplayName() const
{
	return ActivityName.IsNone() ? GetName() : ActivityName.ToString();
}

void ALCOActivitySpot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (NameLabel)
	{
		NameLabel->SetText(FText::FromString(GetDisplayName()));
	}
}

#if WITH_EDITOR
void ALCOActivitySpot::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ALCOActivitySpot, ActivityName) && !ActivityName.IsNone())
	{
		SetActorLabel(ActivityName.ToString());
	}
}
#endif
