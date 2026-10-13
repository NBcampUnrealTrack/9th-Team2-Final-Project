#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CameraCapture/LCOCameraCaptureComponent.h"
#include "LCOCameraUIWidget.generated.h"

class UImage;
class UTextBlock;
class UWidget;
class UTexture2D;
class UInputAction;

/**
 * 카메라 촬영 UI 베이스 (사진 팝업 + 실패 문구)
 * - 디자인은 WBP_CameraUI(부모 클래스 = 이 클래스)에서 진행
 * - 생성/바인딩은 LCOCameraUISubsystem이 자동으로 처리 (DefaultGame.ini에 클래스 경로 지정)
 * - 시간 정지 없음, 마우스 커서 없음 (팝업이 떠 있어도 이동 가능)
 * - 팝업 닫기: PhotoDisplayTime 후 자동 / ClosePhotoAction 키 입력
 * - 촬영 모드(퀵슬롯 1번 카메라를 든 상태)일 때 CameraModePanel(뷰파인더) 표시
 * - 촬영 모드 중 인벤토리 / 수첩 / 대화 / 상자 / 일시정지 등 UI가 열리면 카메라를 자동으로 내림
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOCameraUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULCOCameraUIWidget(const FObjectInitializer& ObjectInitializer);

	// ---------- 사진 팝업 ----------

	// 촬영 성공 시 호출 (이미 떠 있으면 내용 교체 + 타이머 재시작)
	UFUNCTION(BlueprintCallable, Category = "LCO|CameraUI")
	void ShowPhoto(FName CaptureID, UTexture2D* PhotoImage, const FText& Title, const FText& Description);

	UFUNCTION(BlueprintCallable, Category = "LCO|CameraUI")
	void HidePhoto();

	UFUNCTION(BlueprintPure, Category = "LCO|CameraUI")
	bool IsPhotoVisible() const { return bPhotoVisible; }

	// ---------- 실패 문구 ----------

	// 촬영 실패 시 호출. FailMessageKeys에서 UI.csv 키를 찾아 표시 (키가 None이면 표시 안 함)
	UFUNCTION(BlueprintCallable, Category = "LCO|CameraUI")
	void ShowCaptureFailed(ELCOCameraCaptureResult Result);

	// 임의 문구 표시 (MessageDisplayTime 후 자동으로 사라짐)
	UFUNCTION(BlueprintCallable, Category = "LCO|CameraUI")
	void ShowMessage(const FText& Message);

	UFUNCTION(BlueprintCallable, Category = "LCO|CameraUI")
	void HideMessage();

	// ---------- 촬영 모드 (카메라 들기 / 맨손) ----------

	UFUNCTION(BlueprintCallable, Category = "LCO|CameraUI")
	void SetCameraMode(bool bEquipped);

	UFUNCTION(BlueprintPure, Category = "LCO|CameraUI")
	bool IsCameraMode() const { return bCameraMode; }

	// 서브시스템 정리 시 호출 (닫기 키 바인딩 해제)
	void UnbindCloseInput();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// 촬영 모드 중 UI가 열렸는지 감시 → 열리면 카메라 내리기
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ---------- BP 연출용 이벤트 (선택 구현: 페이드 애니메이션 등) ----------

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|CameraUI")
	void BP_OnPhotoShown(FName CaptureID);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|CameraUI")
	void BP_OnPhotoHidden();

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|CameraUI")
	void BP_OnMessageShown(const FText& Message);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|CameraUI")
	void BP_OnMessageHidden();

	// 카메라 들기 / 내리기 (뷰파인더 페이드, 셔터 사운드 준비 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|CameraUI")
	void BP_OnCameraModeChanged(bool bEquipped);

	// ---------- 설정값 (WBP 클래스 디폴트에서 조절) ----------

	// 사진 팝업 자동 닫힘 시간(초). 0이면 자동으로 닫히지 않음
	UPROPERTY(EditDefaultsOnly, Category = "LCO|CameraUI", meta = (ClampMin = "0.0"))
	float PhotoDisplayTime = 4.f;

	// 실패 문구 표시 시간(초)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|CameraUI", meta = (ClampMin = "0.1"))
	float MessageDisplayTime = 2.f;

	// 사진 팝업 닫기 키 (IMC에 키 매핑 필요). 비워두면 자동 닫힘만 사용
	UPROPERTY(EditDefaultsOnly, Category = "LCO|CameraUI")
	TObjectPtr<UInputAction> ClosePhotoAction;

	// 실패 사유 → UI.csv 키. 값을 None으로 두면 그 사유는 문구 없이 무시
	UPROPERTY(EditDefaultsOnly, Category = "LCO|CameraUI")
	TMap<ELCOCameraCaptureResult, FName> FailMessageKeys;

	// ---------- 바인딩 위젯 (WBP에서 같은 이름으로 생성) ----------

	// 사진 팝업 전체를 감싸는 패널 (보이기/숨기기 대상)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PhotoPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_Photo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_PhotoTitle;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_PhotoDescription;

	// 실패 문구 텍스트
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_CaptureMessage;

	// 실패 문구 배경 패널 - 없으면 Txt_CaptureMessage만 보이기/숨기기
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MessagePanel;

	// 닫기 키 안내 텍스트 (예: "[E] 닫기") - 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_CloseHint;

	// 촬영 모드 뷰파인더 (프레임 테두리, 조준점 등) - 없어도 동작
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CameraModePanel;

private:
	void BindCloseInput();
	void HandleClosePressed();
	void SetPhotoPanelVisible(bool bVisible);
	void SetMessagePanelVisible(bool bVisible);
	FText GetUIText(FName Key) const;

	FTimerHandle PhotoTimerHandle;
	FTimerHandle MessageTimerHandle;

	bool bPhotoVisible = false;
	bool bCameraMode = false;
	bool bCloseInputBound = false;
	uint32 CloseBindingHandle = 0;
};
