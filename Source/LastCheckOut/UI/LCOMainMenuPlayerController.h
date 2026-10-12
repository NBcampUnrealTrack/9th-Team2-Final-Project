#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LCOMainMenuPlayerController.generated.h"

class ULCOMainMenuWidget;

/**
 * 메인 메뉴 전용 PlayerController
 * - 메인 메뉴 위젯 생성 + UI 전용 입력 + 마우스 커서 표시
 * - 레벨에 "MainMenuCamera" 태그가 붙은 CameraActor가 있으면 그 시점으로 전환 (3D 배경용)
 */
UCLASS()
class LASTCHECKOUT_API ALCOMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	// BP_MainMenuPlayerController에서 WBP_MainMenu 지정
	UPROPERTY(EditDefaultsOnly, Category = "LCO|UI")
	TSubclassOf<ULCOMainMenuWidget> MainMenuWidgetClass;

	// 배경 카메라를 찾을 때 쓰는 액터 태그
	UPROPERTY(EditDefaultsOnly, Category = "LCO|UI")
	FName MenuCameraTag = TEXT("MainMenuCamera");

private:
	void SetupMenuCamera();

	UPROPERTY()
	TObjectPtr<ULCOMainMenuWidget> MainMenuWidget;
};
