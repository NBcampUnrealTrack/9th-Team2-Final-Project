#include "UI/LCOPauseMenuWidget.h"

#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/LCOGameInstance.h"

void ULCOPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// UI 전용 입력 모드에서 이 위젯이 포커스를 받을 수 있도록
	SetIsFocusable(true);

	if (Btn_Resume)   Btn_Resume->OnClicked.AddDynamic(this, &ThisClass::HandleResumeClicked);
	if (Btn_Settings) Btn_Settings->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
	if (Btn_MainMenu) Btn_MainMenu->OnClicked.AddDynamic(this, &ThisClass::HandleMainMenuClicked);
	if (Btn_Quit)     Btn_Quit->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);
}

void ULCOPauseMenuWidget::OpenPauseMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (!IsInViewport())
	{
		AddToViewport(5);
	}

	UGameplayStatics::SetGamePaused(this, true);

	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PC, this, EMouseLockMode::DoNotLock);
	PC->SetShowMouseCursor(true);
}

void ULCOPauseMenuWidget::ClosePauseMenu()
{
	// 설정 위젯이 떠 있으면 같이 닫음
	if (SettingsWidget && SettingsWidget->IsInViewport())
	{
		SettingsWidget->RemoveFromParent();
	}

	RemoveFromParent();

	UGameplayStatics::SetGamePaused(this, false);

	if (APlayerController* PC = GetOwningPlayer())
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
