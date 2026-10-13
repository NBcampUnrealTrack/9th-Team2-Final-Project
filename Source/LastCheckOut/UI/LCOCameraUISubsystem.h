#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "CameraCapture/LCOCameraCaptureComponent.h"
#include "LCOCameraUISubsystem.generated.h"

class APawn;
class APlayerController;
class ULCOCameraUIWidget;
class ULCOQuickSlotComponent;
class UTexture2D;
class UWorld;

/**
 * 카메라 촬영 UI 자동 생성 + 연결 (LCONotebookUISubsystem과 같은 방식)
 * - 게임 맵마다 WBP_CameraUI를 생성해서 화면에 올려둠 (평소에는 내용이 숨겨져 있음)
 * - 플레이어 캐릭터의 LCOCameraCaptureComponent에 자동 바인딩
 *   OnPhotoCaptured → 사진 팝업 / OnCaptureFailed → 실패 문구
 * - 퀵슬롯 OnCameraEquipChanged → 촬영 모드(뷰파인더) 표시
 * - 캐릭터가 바뀌어도(리스폰 등) 새 캐릭터에 다시 바인딩
 * 설정: Config/DefaultGame.ini
 *   [/Script/LastCheckOut.LCOCameraUISubsystem]
 *   CameraUIWidgetClass=/Game/.../WBP_CameraUI.WBP_CameraUI_C
 *   +ExcludedMapNames=MainMenu_Test
 */
UCLASS(Config = Game)
class LASTCHECKOUT_API ULCOCameraUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	UFUNCTION(BlueprintPure, Category = "LCO|CameraUI")
	ULCOCameraUIWidget* GetCameraUIWidget() const { return CameraUIWidget; }

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandlePhotoCaptured(FName CaptureID, UTexture2D* PhotoImage, FText PhotoTitle, FText PhotoDescription);

	UFUNCTION()
	void HandleCaptureFailed(ELCOCameraCaptureResult Result);

	UFUNCTION()
	void HandleCameraEquipChanged(bool bEquipped);

	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void TryCreateCameraUI(APlayerController* PlayerController);
	void CleanupCameraUI();

	void BindToPlayerController(APlayerController* PlayerController);
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromCameraComponent();

	bool ShouldCreateForWorld(const UWorld* World) const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|CameraUI")
	TSoftClassPtr<ULCOCameraUIWidget> CameraUIWidgetClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|CameraUI")
	TArray<FName> ExcludedMapNames;

	// 다른 HUD 위에 그려지도록 (수첩·일시정지 메뉴보다는 낮게)
	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|CameraUI")
	int32 ViewportZOrder = 5;

	UPROPERTY(Transient)
	TObjectPtr<ULCOCameraUIWidget> CameraUIWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> BoundPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOCameraCaptureComponent> BoundCameraComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<ULCOQuickSlotComponent> BoundQuickSlotComponent;
};
