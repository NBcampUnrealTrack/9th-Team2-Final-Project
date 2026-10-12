#include "UI/LCOPauseMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/LCOGameInstance.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Player/LCOPlayerController.h"
#include "UI/LCOInteractionPromptSubsystem.h"
#include "UI/LCOContainerWidget.h"
#include "UI/LCOInventoryWidget.h"
#include "UI/LCONotebookWidget.h"

void ULCOPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Btn_Resume)
	{
		Btn_Resume->OnClicked.RemoveDynamic(this, &ThisClass::HandleResumeClicked);
		Btn_Resume->OnClicked.AddDynamic(this, &ThisClass::HandleResumeClicked);
	}

	if (Btn_Settings)
	{
		Btn_Settings->OnClicked.RemoveDynamic(this, &ThisClass::HandleSettingsClicked);
		Btn_Settings->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
	}

	if (Btn_MainMenu)
	{
		Btn_MainMenu->OnClicked.RemoveDynamic(this, &ThisClass::HandleMainMenuClicked);
		Btn_MainMenu->OnClicked.AddDynamic(this, &ThisClass::HandleMainMenuClicked);
	}

	if (Btn_Quit)
	{
		Btn_Quit->OnClicked.RemoveDynamic(this, &ThisClass::HandleQuitClicked);
		Btn_Quit->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);
	}

	// O 키(옵션 입력) → 일시정지 메뉴 토글
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnOptionsRequested.RemoveDynamic(this, &ThisClass::HandleOptionsRequested);
		LCOPC->OnOptionsRequested.AddDynamic(this, &ThisClass::HandleOptionsRequested);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPauseMenu] Owning Player가 LCOPlayerController가 아님 - O 키 바인딩 안 됨"));
	}
}

void ULCOPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindLocalizationEvents();
	RefreshLocalizedTexts();
}

void ULCOPauseMenuWidget::NativeDestruct()
{
	UnbindLocalizationEvents();

	Super::NativeDestruct();
}

void ULCOPauseMenuWidget::UnbindFromOptionsInput()
{
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnOptionsRequested.RemoveDynamic(this, &ThisClass::HandleOptionsRequested);
	}
}

void ULCOPauseMenuWidget::OpenPauseMenu()
{
	if (IsInViewport())
	{
		return;
	}

	// 수첩 / 인벤토리 / 수납장이 열려 있으면 먼저 닫음 (ZOrder가 더 높아서 일시정지 메뉴 위에 남는 문제 방지)
	CloseOtherMenus();

	AddToViewport(5);
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, true);
		}
	}
	UGameplayStatics::SetGamePaused(this, true);
	SetPlayerUIMode(true);
}

void ULCOPauseMenuWidget::ClosePauseMenu()
{
	if (!IsInViewport())
	{
		return;
	}

	// 설정 위젯이 떠 있으면 같이 닫음
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		SettingsWidget->RemoveFromParent();
	}

	RemoveFromParent();
	UGameplayStatics::SetGamePaused(this, false);
	SetPlayerUIMode(false);
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, false);
		}
	}
}

void ULCOPauseMenuWidget::TogglePauseMenu()
{
	if (IsInViewport())
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void ULCOPauseMenuWidget::CloseOtherMenus()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// 각 메뉴의 Close 함수가 UI 모드 해제까지 처리 → 이후 일시정지 메뉴가 다시 UI 모드로 전환
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, FoundWidgets, ULCONotebookWidget::StaticClass(), false);
	for (UUserWidget* Widget : FoundWidgets)
	{
		ULCONotebookWidget* Notebook = Cast<ULCONotebookWidget>(Widget);
		if (Notebook && Notebook->GetOwningPlayer() == PC && Notebook->IsNotebookOpen())
		{
			Notebook->CloseNotebook();
		}
	}

	FoundWidgets.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, FoundWidgets, ULCOInventoryWidget::StaticClass(), false);
	for (UUserWidget* Widget : FoundWidgets)
	{
		ULCOInventoryWidget* Inventory = Cast<ULCOInventoryWidget>(Widget);
		if (Inventory && Inventory->GetOwningPlayer() == PC && Inventory->IsInViewport())
		{
			Inventory->CloseInventory();
		}
	}

	// 수납장 UI: CloseContainer가 수납장 액터 닫기 + 입력 모드 복구까지 처리
	FoundWidgets.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, FoundWidgets, ULCOContainerWidget::StaticClass(), false);
	for (UUserWidget* Widget : FoundWidgets)
	{
		ULCOContainerWidget* ContainerWidget = Cast<ULCOContainerWidget>(Widget);
		if (ContainerWidget && ContainerWidget->GetOwningPlayer() == PC && ContainerWidget->IsInViewport())
		{
			ContainerWidget->CloseContainer();
		}
	}
}

void ULCOPauseMenuWidget::HandleOptionsRequested()
{
	TogglePauseMenu();
}

void ULCOPauseMenuWidget::SetPlayerUIMode(bool bEnable)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// 플레이어 컨트롤러 쪽 UI 모드 처리 사용 (커서, 이동/시점 차단, 입력 모드)
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
	{
		LCOPC->SetUIMode(bEnable);
		return;
	}

	// LCOPlayerController가 아닐 때만 직접 처리
	if (bEnable)
	{
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC, this, EMouseLockMode::DoNotLock, false);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
		PC->SetShowMouseCursor(false);
	}
}

void ULCOPauseMenuWidget::HandleResumeClicked()
{
	ClosePauseMenu();
}

void ULCOPauseMenuWidget::HandleSettingsClicked()
{
	if (!SettingsWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPauseMenu] SettingsWidgetClass가 비어 있음 - 설정 위젯 완성 후 지정 필요"));
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
		// 일시정지 메뉴보다 위에 표시 (닫기는 설정 위젯이 RemoveFromParent로 처리)
		SettingsWidget->AddToViewport(10);
	}
}

void ULCOPauseMenuWidget::HandleMainMenuClicked()
{
	if (bIsTransitioning)
	{
		return;
	}

	if (MainMenuLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPauseMenu] MainMenuLevel이 비어 있음 - WBP 클래스 디폴트에서 지정 필요"));
		return;
	}

	bIsTransitioning = true;

	// 일시정지 상태로 레벨을 넘기지 않도록 해제
	UGameplayStatics::SetGamePaused(this, false);

	// 메인 메뉴 복귀 시 시작 방식 초기화
	if (ULCOGameInstance* LCOGameInstance = Cast<ULCOGameInstance>(GetGameInstance()))
	{
		LCOGameInstance->ResetStartMode();
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
}

void ULCOPauseMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void ULCOPauseMenuWidget::HandleLanguageChanged(FName NewLanguage)
{
	RefreshLocalizedTexts();
}

void ULCOPauseMenuWidget::BindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOPauseMenuWidget::UnbindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCOPauseMenuWidget::RefreshLocalizedTexts()
{
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>();
	if (!LocalizationSubsystem)
	{
		return;
	}

	if (Txt_Resume)
	{
		Txt_Resume->SetText(LocalizationSubsystem->GetUIText(TEXT("MENU_RESUME")));
	}

	if (Txt_Settings)
	{
		Txt_Settings->SetText(LocalizationSubsystem->GetUIText(TEXT("MENU_SETTINGS")));
	}

	if (Txt_MainMenu)
	{
		Txt_MainMenu->SetText(LocalizationSubsystem->GetUIText(TEXT("MENU_MAIN_MENU")));
	}

	if (Txt_Quit)
	{
		Txt_Quit->SetText(LocalizationSubsystem->GetUIText(TEXT("MENU_QUIT")));
	}
}
