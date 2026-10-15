#include "LCOMainMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LastCheckOut/Core/LCOGameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Save/LCOSaveManager.h"

void ULCOMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 버튼 클릭 바인딩 (NativeOnInitialized는 위젯 생성 시 1회만 호출 → 중복 바인딩 없음)
	if (Btn_NewGame)      Btn_NewGame->OnClicked.AddDynamic(this, &ThisClass::HandleNewGameClicked);
	if (Btn_Continue)     Btn_Continue->OnClicked.AddDynamic(this, &ThisClass::HandleContinueClicked);
	if (Btn_Settings)     Btn_Settings->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
	if (Btn_Quit)         Btn_Quit->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);
	if (Btn_SettingsBack) Btn_SettingsBack->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsBackClicked);
}

void ULCOMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsTransitioning = false;

	BindLocalizationEvents();
	RefreshLocalizedText();

	// 세이브가 없으면 이어하기 비활성화
	if (Btn_Continue)
	{
		Btn_Continue->SetIsEnabled(CanContinue());
	}

	// 처음엔 설정 패널 닫힌 상태
	CloseSettings();

	// 키보드/패드 포커스를 새 게임 버튼에
	if (Btn_NewGame)
	{
		Btn_NewGame->SetKeyboardFocus();
	}
}

void ULCOMainMenuWidget::NativeDestruct()
{
	UnbindLocalizationEvents();

	Super::NativeDestruct();
}

bool ULCOMainMenuWidget::CanContinue() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOSaveManager* SaveManager = GameInstance
		? GameInstance->GetSubsystem<ULCOSaveManager>()
		: nullptr;

	return SaveManager && SaveManager->HasSaveFile();
}

void ULCOMainMenuWidget::OpenSettings()
{
	if (MenuPanel)     MenuPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (SettingsPanel) SettingsPanel->SetVisibility(ESlateVisibility::Visible);

	if (Btn_SettingsBack)
	{
		Btn_SettingsBack->SetKeyboardFocus();
	}

	BP_OnSettingsOpened();
}

void ULCOMainMenuWidget::CloseSettings()
{
	if (SettingsPanel) SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (MenuPanel)     MenuPanel->SetVisibility(ESlateVisibility::Visible);

	BP_OnSettingsClosed();
}

void ULCOMainMenuWidget::HandleNewGameClicked()
{
	StartGameWithMode(ELCOGameStartMode::NewGame);
}

void ULCOMainMenuWidget::HandleContinueClicked()
{
	if (bIsTransitioning || !CanContinue())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	ULCOSaveManager* SaveManager = GameInstance
		? GameInstance->GetSubsystem<ULCOSaveManager>()
		: nullptr;
	if (!SaveManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOMainMenu] SaveManager not found"));
		return;
	}

	bIsTransitioning = true;
	BP_OnStartTransition();

	// 저장된 맵 선택과 이어하기 모드 설정은 SaveManager에서 처리한다.
	if (!SaveManager->StartContinue())
	{
		bIsTransitioning = false;
		UE_LOG(LogTemp, Warning, TEXT("[LCOMainMenu] StartContinue failed"));
	}
}

void ULCOMainMenuWidget::HandleSettingsClicked()
{
	OpenSettings();
}

void ULCOMainMenuWidget::HandleSettingsBackClicked()
{
	CloseSettings();
}

void ULCOMainMenuWidget::HandleLanguageChanged(FName /*NewLanguage*/)
{
	RefreshLocalizedText();
}

void ULCOMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void ULCOMainMenuWidget::BindLocalizationEvents()
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

void ULCOMainMenuWidget::UnbindLocalizationEvents()
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

void ULCOMainMenuWidget::RefreshLocalizedText()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance
		? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>()
		: nullptr;

	if (!LocalizationSubsystem)
	{
		return;
	}

	if (Txt_Start)    Txt_Start->SetText(LocalizationSubsystem->GetUIText(FName(TEXT("MENU_NEW_GAME"))));
	if (Txt_Continue) Txt_Continue->SetText(LocalizationSubsystem->GetUIText(FName(TEXT("MENU_CONTINUE"))));
	if (Txt_Settings) Txt_Settings->SetText(LocalizationSubsystem->GetUIText(FName(TEXT("MENU_SETTINGS"))));
	if (Txt_Quit)     Txt_Quit->SetText(LocalizationSubsystem->GetUIText(FName(TEXT("MENU_QUIT"))));
}

void ULCOMainMenuWidget::StartGameWithMode(ELCOGameStartMode InStartMode)
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

	ULCOGameInstance* LCOGameInstance = Cast<ULCOGameInstance>(GetGameInstance());
	if (!LCOGameInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOMainMenu] LCOGameInstance not found"));
		return;
	}

	bIsTransitioning = true;

	// GameMode::StartGame에서 이 값을 읽어 새 게임/이어하기 분기
	LCOGameInstance->SetStartMode(InStartMode);

	BP_OnStartTransition();

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, GameLevel);
}
