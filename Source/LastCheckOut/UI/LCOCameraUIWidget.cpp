#include "UI/LCOCameraUIWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "TimerManager.h"

ULCOCameraUIWidget::ULCOCameraUIWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 기본 키 (WBP 클래스 디폴트에서 변경 가능)
	FailMessageKeys.Add(ELCOCameraCaptureResult::NoTarget,        TEXT("CAMERA_FAIL_NO_TARGET"));
	FailMessageKeys.Add(ELCOCameraCaptureResult::Cooldown,        TEXT("CAMERA_FAIL_COOLDOWN"));
	FailMessageKeys.Add(ELCOCameraCaptureResult::AlreadyCaptured, TEXT("CAMERA_FAIL_ALREADY_CAPTURED"));
	FailMessageKeys.Add(ELCOCameraCaptureResult::InvalidData,     TEXT("CAMERA_FAIL_ERROR"));
	FailMessageKeys.Add(ELCOCameraCaptureResult::SystemError,     TEXT("CAMERA_FAIL_ERROR"));
}

void ULCOCameraUIWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 시작 시 전부 숨김
	SetPhotoPanelVisible(false);
	SetMessagePanelVisible(false);

	if (CameraModePanel)
	{
		CameraModePanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 위젯 자체는 클릭을 막지 않음 (게임 조작 유지)
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (Txt_CloseHint)
	{
		Txt_CloseHint->SetText(GetUIText(TEXT("CAMERA_PHOTO_CLOSE_HINT")));
	}

	BindCloseInput();
}

void ULCOCameraUIWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhotoTimerHandle);
		World->GetTimerManager().ClearTimer(MessageTimerHandle);
	}

	UnbindCloseInput();

	Super::NativeDestruct();
}

// ---------- 사진 팝업 ----------

void ULCOCameraUIWidget::ShowPhoto(FName CaptureID, UTexture2D* PhotoImage, const FText& Title, const FText& Description)
{
	if (Img_Photo)
	{
		Img_Photo->SetBrushFromTexture(PhotoImage, false);
	}

	if (Txt_PhotoTitle)
	{
		Txt_PhotoTitle->SetText(Title);
	}

	if (Txt_PhotoDescription)
	{
		Txt_PhotoDescription->SetText(Description);
	}

	// 성공 팝업이 뜨면 실패 문구는 정리
	HideMessage();

	SetPhotoPanelVisible(true);
	bPhotoVisible = true;

	// 자동 닫힘 타이머 (이미 떠 있던 경우에도 재시작)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhotoTimerHandle);

		if (PhotoDisplayTime > 0.f)
		{
			World->GetTimerManager().SetTimer(PhotoTimerHandle, this, &ThisClass::HidePhoto, PhotoDisplayTime, false);
		}
	}

	BP_OnPhotoShown(CaptureID);
}

void ULCOCameraUIWidget::HidePhoto()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhotoTimerHandle);
	}

	if (!bPhotoVisible)
	{
		return;
	}

	SetPhotoPanelVisible(false);
	bPhotoVisible = false;

	BP_OnPhotoHidden();
}

// ---------- 실패 문구 ----------

void ULCOCameraUIWidget::ShowCaptureFailed(ELCOCameraCaptureResult Result)
{
	const FName* Key = FailMessageKeys.Find(Result);
	if (!Key || Key->IsNone())
	{
		return;
	}

	ShowMessage(GetUIText(*Key));
}

void ULCOCameraUIWidget::ShowMessage(const FText& Message)
{
	if (Message.IsEmpty() || !Txt_CaptureMessage)
	{
		return;
	}

	Txt_CaptureMessage->SetText(Message);
	SetMessagePanelVisible(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MessageTimerHandle);
		World->GetTimerManager().SetTimer(MessageTimerHandle, this, &ThisClass::HideMessage, MessageDisplayTime, false);
	}

	BP_OnMessageShown(Message);
}

void ULCOCameraUIWidget::HideMessage()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MessageTimerHandle);
	}

	const UWidget* Target = MessagePanel ? MessagePanel.Get() : static_cast<UWidget*>(Txt_CaptureMessage.Get());
	if (!Target || Target->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	SetMessagePanelVisible(false);
	BP_OnMessageHidden();
}

// ---------- 촬영 모드 ----------

void ULCOCameraUIWidget::SetCameraMode(bool bEquipped)
{
	if (bCameraMode == bEquipped)
	{
		return;
	}

	bCameraMode = bEquipped;

	if (CameraModePanel)
	{
		CameraModePanel->SetVisibility(bCameraMode ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	BP_OnCameraModeChanged(bCameraMode);
}

// ---------- 닫기 키 ----------

void ULCOCameraUIWidget::BindCloseInput()
{
	if (bCloseInputBound || !ClosePhotoAction)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	UEnhancedInputComponent* EIC = PlayerController ? Cast<UEnhancedInputComponent>(PlayerController->InputComponent) : nullptr;
	if (!EIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOCameraUIWidget] EnhancedInputComponent not found - 닫기 키 바인딩 실패 (자동 닫힘만 동작)"));
		return;
	}

	CloseBindingHandle = EIC->BindAction(ClosePhotoAction, ETriggerEvent::Started, this, &ThisClass::HandleClosePressed).GetHandle();
	bCloseInputBound = true;
}

void ULCOCameraUIWidget::UnbindCloseInput()
{
	if (!bCloseInputBound)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (UEnhancedInputComponent* EIC = PlayerController ? Cast<UEnhancedInputComponent>(PlayerController->InputComponent) : nullptr)
	{
		EIC->RemoveBindingByHandle(CloseBindingHandle);
	}

	bCloseInputBound = false;
	CloseBindingHandle = 0;
}

void ULCOCameraUIWidget::HandleClosePressed()
{
	// 팝업이 없을 때는 아무것도 하지 않음 (같은 키를 다른 기능과 공유해도 안전)
	if (bPhotoVisible)
	{
		HidePhoto();
	}
}

// ---------- 내부 ----------

void ULCOCameraUIWidget::SetPhotoPanelVisible(bool bVisible)
{
	if (PhotoPanel)
	{
		PhotoPanel->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ULCOCameraUIWidget::SetMessagePanelVisible(bool bVisible)
{
	UWidget* Target = MessagePanel ? MessagePanel.Get() : static_cast<UWidget*>(Txt_CaptureMessage.Get());
	if (Target)
	{
		Target->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FText ULCOCameraUIWidget::GetUIText(FName Key) const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const ULCOLocalizationSubsystem* Localization = GameInstance->GetSubsystem<ULCOLocalizationSubsystem>())
		{
			return Localization->GetUIText(Key);
		}
	}

	return FText::FromName(Key);
}
