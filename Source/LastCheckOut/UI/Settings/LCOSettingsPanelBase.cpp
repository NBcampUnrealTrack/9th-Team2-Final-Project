#include "UI/Settings/LCOSettingsPanelBase.h"

#include "Components/TextBlock.h"
#include "Core/LCOGameSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"

void ULCOSettingsPanelBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOSettingsPanelBase::NativeDestruct()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}

	Super::NativeDestruct();
}

ULCOLocalizationSubsystem* ULCOSettingsPanelBase::GetLocalizationSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr;
}

ULCOGameSettingsSubsystem* ULCOSettingsPanelBase::GetGameSettingsSubsystem() const
{
	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<ULCOGameSettingsSubsystem>() : nullptr;
}

FText ULCOSettingsPanelBase::GetUIText(FName Key) const
{
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem();
	return LocalizationSubsystem ? LocalizationSubsystem->GetUIText(Key) : FText::FromName(Key);
}

void ULCOSettingsPanelBase::SetUIText(UTextBlock* TextBlock, FName Key) const
{
	if (TextBlock)
	{
		TextBlock->SetText(GetUIText(Key));
	}
}

void ULCOSettingsPanelBase::HandleLanguageChanged(FName NewLanguage)
{
	RefreshLocalizedTexts();
}
