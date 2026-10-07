#include "UI/LCOMainMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/LCOGameInstance.h"

void ULCOMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 위젯 생성 시 1회만 호출 → 중복 바인딩 없음
	if (Btn_Start) Btn_Start->OnClicked.AddDynamic(this, &ThisClass::HandleStartClicked);
	if (Btn_Quit)  Btn_Quit->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);
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

void ULCOMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
