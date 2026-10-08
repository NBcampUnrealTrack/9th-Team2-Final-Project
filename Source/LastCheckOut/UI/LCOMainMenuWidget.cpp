#include "UI/LCOMainMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/LCOGameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"

void ULCOMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 위젯 생성 시 1회만 호출 → 중복 바인딩 없음
	if (Btn_Start)    Btn_Start->OnClicked.AddDynamic(this, &ThisClass::HandleStartClicked);
	if (Btn_Settings) Btn_Settings->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
	if (Btn_Quit)     Btn_Quit->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);
}

void ULCOMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindLocalizationEvents();
	RefreshLocalizedTexts();
}

void ULCOMainMenuWidget::NativeDestruct()
{
	UnbindLocalizationEvents();

	Super::NativeDestruct();
}

void ULCOMainMenuWidget::HandleStartClicked()
{
	if (bIsTransitioning)
	{
		return;
	}

	if (GameLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOMainMenu] GameLevel이 비어 있음 - WBP 클래스 디폴트에서 지정 필요"));
		return;
	}

	// GameMode::StartGame에서 읽을 시작 방식 저장
	if (ULCOGameInstance* LCOGameInstance = Cast<ULCOGameInstance>(GetGameInstance()))
	{
		LCOGameInstance->SetStartMode(ELCOGameStartMode::NewGame);
	}

	bIsTransitioning = true;
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, GameLevel);
}

void ULCOMainMenuWidget::HandleSettingsClicked()
{
	if (!SettingsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOMainMenu] SettingsWidgetClass가 비어 있음 - 설정 위젯 완성 후 지정 필요"));
		return;
	}

	// 이미 떠 있으면 다시 만들지 않음
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		return;
	}

	SettingsWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), SettingsWidgetClass);
	if (SettingsWidget)
	{
		// 메인 메뉴보다 위에 표시 (닫기는 설정 위젯이 RemoveFromParent로 처리)
		SettingsWidget->AddToViewport(10);
	}
}

void ULCOMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void ULCOMainMenuWidget::HandleLanguageChanged(FName NewLanguage)
{
	RefreshLocalizedTexts();
}

void ULCOMainMenuWidget::BindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOMainMenuWidget::UnbindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOMainMenuWidget::RefreshLocalizedTexts()
{
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();
	if (!LocalizationSubsystem)
	{
		return;
	}

	if (Txt_Start)
	{
		Txt_Start->SetText(LocalizationSubsystem->GetUIText(TEXT("MENU_NEW_GAME")));
	}

	if (Txt_Settings)
	{
		Txt_Settings->SetText(LocalizationSubsystem->GetUIText(TEXT("MENU_SETTINGS")));
	}

	if (Txt_Quit)
	{
		Txt_Quit->SetText(LocalizationSubsystem->GetUIText(TEXT("MENU_QUIT")));
	}
}
