#include "UI/LCOSenseWidget.h"

#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Player/LCOPlayerCharacter.h"
#include "TimerManager.h"

ULCOSenseWidget::ULCOSenseWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULCOSenseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetChargesVisible(false);
	SetHoldVisible(false);
	UpdateHoldProgress(0.f);
}

void ULCOSenseWidget::NativeDestruct()
{
	UnbindFromCharacter();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargesVisibilityTimer);
	}

	Super::NativeDestruct();
}

void ULCOSenseWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bHoldVisible)
	{
		return;
	}

	ALCOPlayerCharacter* Character = BoundCharacter.Get();
	if (!Character || !Character->IsSenseHolding() || !Character->CanUseSense())
	{
		SetHoldVisible(false);
		UpdateHoldProgress(0.f);
		return;
	}

	UpdateHoldProgress(Character->GetSenseHoldProgress());
}

void ULCOSenseWidget::BindToCharacter(ALCOPlayerCharacter* Character)
{
	if (BoundCharacter.Get() == Character)
	{
		SyncChargesFromCharacter();
		RefreshHoldState();
		return;
	}

	UnbindFromCharacter();
	BoundCharacter = Character;

	if (Character)
	{
		Character->OnSenseChargesChanged.RemoveDynamic(this, &ThisClass::HandleSenseChargesChanged);
		Character->OnSenseChargesChanged.AddDynamic(this, &ThisClass::HandleSenseChargesChanged);

		Character->OnSenseUsed.RemoveDynamic(this, &ThisClass::HandleSenseUsed);
		Character->OnSenseUsed.AddDynamic(this, &ThisClass::HandleSenseUsed);
	}

	SyncChargesFromCharacter();
	RefreshHoldState();
}

void ULCOSenseWidget::UnbindFromCharacter()
{
	if (ALCOPlayerCharacter* Character = BoundCharacter.Get())
	{
		Character->OnSenseChargesChanged.RemoveDynamic(this, &ThisClass::HandleSenseChargesChanged);
		Character->OnSenseUsed.RemoveDynamic(this, &ThisClass::HandleSenseUsed);
	}

	BoundCharacter.Reset();
	SetHoldVisible(false);
	UpdateHoldProgress(0.f);
}

void ULCOSenseWidget::NotifySenseInputStarted()
{
	SyncChargesFromCharacter();
	SetChargesVisible(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargesVisibilityTimer);
		World->GetTimerManager().SetTimer(ChargesVisibilityTimer, this, &ThisClass::HideCharges, ChargesVisibleDuration, false);
	}
}

void ULCOSenseWidget::NotifySenseInputReleased()
{
	SetHoldVisible(false);
	UpdateHoldProgress(0.f);
}

void ULCOSenseWidget::RefreshHoldState()
{
	ALCOPlayerCharacter* Character = BoundCharacter.Get();
	const bool bShouldShow = Character && Character->CanUseSense() && Character->IsSenseHolding();

	SetHoldVisible(bShouldShow);
	UpdateHoldProgress(bShouldShow ? Character->GetSenseHoldProgress() : 0.f);
}

void ULCOSenseWidget::HandleSenseChargesChanged(int32 CurrentCharges, int32 MaxCharges)
{
	UpdateCharges(CurrentCharges, MaxCharges);
}

void ULCOSenseWidget::HandleSenseUsed(bool bSuccess)
{
	SetHoldVisible(false);
	UpdateHoldProgress(0.f);
	SyncChargesFromCharacter();
}

void ULCOSenseWidget::SetChargesVisible(bool bVisible)
{
	if (SenseChargesPanel)
	{
		SenseChargesPanel->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bChargesVisible != bVisible)
	{
		bChargesVisible = bVisible;
		BP_OnChargesVisibilityChanged(bChargesVisible);
	}
}

void ULCOSenseWidget::SetHoldVisible(bool bVisible)
{
	if (SenseHoldPanel)
	{
		SenseHoldPanel->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bHoldVisible != bVisible)
	{
		bHoldVisible = bVisible;
		BP_OnHoldVisibilityChanged(bHoldVisible);
	}
}

void ULCOSenseWidget::HideCharges()
{
	SetChargesVisible(false);
}

void ULCOSenseWidget::SyncChargesFromCharacter()
{
	if (const ALCOPlayerCharacter* Character = BoundCharacter.Get())
	{
		UpdateCharges(Character->GetCurrentSenseCharges(), Character->GetMaxSenseCharges());
	}
	else
	{
		UpdateCharges(0, 0);
	}
}

void ULCOSenseWidget::UpdateCharges(int32 CurrentCharges, int32 MaxCharges)
{
	CurrentCharges = FMath::Max(0, CurrentCharges);
	MaxCharges = FMath::Max(0, MaxCharges);

	if (Txt_SenseCharges)
	{
		Txt_SenseCharges->SetText(FText::Format(NSLOCTEXT("LCO", "SenseChargesFormat", "{0} / {1}"), CurrentCharges, MaxCharges));
	}

	if (ChargeIconContainer)
	{
		const int32 ChildCount = ChargeIconContainer->GetChildrenCount();
		for (int32 Index = 0; Index < ChildCount; ++Index)
		{
			if (UWidget* Child = ChargeIconContainer->GetChildAt(Index))
			{
				const bool bUsedByMax = Index < MaxCharges;
				Child->SetVisibility(bUsedByMax ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
				Child->SetRenderOpacity(Index < CurrentCharges ? ChargeActiveOpacity : ChargeInactiveOpacity);
			}
		}
	}

	BP_OnChargesChanged(CurrentCharges, MaxCharges);
}

void ULCOSenseWidget::UpdateHoldProgress(float Progress)
{
	const float ClampedProgress = FMath::Clamp(Progress, 0.f, 1.f);

	if (Progress_SenseHold)
	{
		Progress_SenseHold->SetPercent(ClampedProgress);
	}

	BP_OnHoldProgressChanged(ClampedProgress);
}
