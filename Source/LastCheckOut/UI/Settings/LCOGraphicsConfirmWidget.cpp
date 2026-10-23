#include "UI/Settings/LCOGraphicsConfirmWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/LCOGameSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"

void ULCOGraphicsConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RemainingTime = GraphicsConfirmDuration;
	ShownSeconds = INDEX_NONE;
	bFinished = false;

	if (GraphicsConfirmText)
	{
		GraphicsConfirmText->SetText(GetUIText(TEXT("SETTINGS_GRAPHICS_CONFIRM")));
	}

	if (GraphicsKeepButtonText)
	{
		GraphicsKeepButtonText->SetText(GetUIText(TEXT("SETTINGS_GRAPHICS_KEEP")));
	}

	if (GraphicsRevertButtonText)
	{
		GraphicsRevertButtonText->SetText(GetUIText(TEXT("SETTINGS_GRAPHICS_REVERT")));
	}

	if (GraphicsKeepButton)
	{
		GraphicsKeepButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleKeepClicked);
		GraphicsKeepButton->OnClicked.AddDynamic(this, &ThisClass::HandleKeepClicked);
	}

	if (GraphicsRevertButton)
	{
		GraphicsRevertButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRevertClicked);
		GraphicsRevertButton->OnClicked.AddDynamic(this, &ThisClass::HandleRevertClicked);
	}

	RefreshCountdown();
	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsConfirm] Shown (%.1fs)"), GraphicsConfirmDuration);
}

void ULCOGraphicsConfirmWidget::NativeDestruct()
{
	if (!bFinished)
	{
		Finish(false);
	}

	if (GraphicsKeepButton)
	{
		GraphicsKeepButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleKeepClicked);
	}

	if (GraphicsRevertButton)
	{
		GraphicsRevertButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRevertClicked);
	}

	Super::NativeDestruct();
}

void ULCOGraphicsConfirmWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bFinished)
	{
		return;
	}
	
	RemainingTime -= InDeltaTime;
	if (RemainingTime <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsConfirm] Timed out -> revert"));
		HandleRevertClicked();
		return;
	}

	RefreshCountdown();
}

void ULCOGraphicsConfirmWidget::RevertAndClose()
{
	if (!bFinished)
	{
		Finish(false);
	}
	RemoveFromParent();
}

void ULCOGraphicsConfirmWidget::HandleKeepClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsConfirm] Keep clicked"));
	Finish(true);
	RemoveFromParent();
}

void ULCOGraphicsConfirmWidget::HandleRevertClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LCOGraphicsConfirm] Revert clicked"));
	Finish(false);
	RemoveFromParent();
}

void ULCOGraphicsConfirmWidget::Finish(bool bKeep)
{
	bFinished = true;

	UGameInstance* GameInstance = GetGameInstance();
	if (ULCOGameSettingsSubsystem* GameSettings = GameInstance ? GameInstance->
		GetSubsystem<ULCOGameSettingsSubsystem>() : nullptr)
	{
		if (bKeep)
		{
			GameSettings->ConfirmGraphicsSettings();
		}
		else
		{
			GameSettings->RevertAppliedGraphicsSettings();
		}
	}

	OnFinished.Broadcast(bKeep);
}

void ULCOGraphicsConfirmWidget::RefreshCountdown()
{
	if (!GraphicsConfirmCountdownText)
	{
		return;
	}
	
	const int32 Seconds = FMath::Max(1, FMath::CeilToInt(RemainingTime));
	if (Seconds == ShownSeconds)
	{
		return;
	}
	ShownSeconds = Seconds;
	
	GraphicsConfirmCountdownText->SetText(FText::Format(FTextFormat(GetUIText(
		TEXT("SETTINGS_GRAPHICS_CONFIRM_COUNTDOWN"))), FText::AsNumber(Seconds)));
}

FText ULCOGraphicsConfirmWidget::GetUIText(FName Key) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr;
	return LocalizationSubsystem ? LocalizationSubsystem->GetUIText(Key) : FText::FromName(Key);
}
