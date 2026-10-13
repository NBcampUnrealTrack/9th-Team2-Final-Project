#include "UI/LCOLoadingScreenWidget.h"

#include "Components/TextBlock.h"
#include "Localization/LCOLocalizationSubsystem.h"

ULCOLoadingScreenWidget::ULCOLoadingScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LoadingTipKeys =
	{
		FName(TEXT("LOADING_TIP_001")),
		FName(TEXT("LOADING_TIP_002")),
		FName(TEXT("LOADING_TIP_003")),
		FName(TEXT("LOADING_TIP_004"))
	};
}

void ULCOLoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindLocalizationEvents();
	SelectRandomTip();
	RefreshTipText();
}

void ULCOLoadingScreenWidget::NativeDestruct()
{
	UnbindLocalizationEvents();

	Super::NativeDestruct();
}

void ULCOLoadingScreenWidget::HandleLanguageChanged(FName /*NewLanguage*/)
{
	RefreshTipText();
}

void ULCOLoadingScreenWidget::SelectRandomTip()
{
	if (LoadingTipKeys.IsEmpty())
	{
		CurrentTipKey = NAME_None;
		return;
	}

	const int32 TipIndex = FMath::RandRange(0, LoadingTipKeys.Num() - 1);
	CurrentTipKey = LoadingTipKeys[TipIndex];
}

void ULCOLoadingScreenWidget::RefreshTipText()
{
	if (!Text_Tip)
	{
		return;
	}

	if (CurrentTipKey.IsNone())
	{
		Text_Tip->SetText(FText::GetEmpty());
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance
		? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>()
		: nullptr;

	Text_Tip->SetText(LocalizationSubsystem
		? LocalizationSubsystem->GetUIText(CurrentTipKey)
		: FText::FromName(CurrentTipKey));
}

void ULCOLoadingScreenWidget::BindLocalizationEvents()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOLoadingScreenWidget::UnbindLocalizationEvents()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}
