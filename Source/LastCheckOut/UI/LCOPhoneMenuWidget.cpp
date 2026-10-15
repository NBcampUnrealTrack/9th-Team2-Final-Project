#include "UI/LCOPhoneMenuWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interaction/LCOPhone.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Player/LCOPlayerController.h"
#include "UI/LCOInteractionPromptSubsystem.h"

void ULCOPhoneMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	if (Btn_Call) Btn_Call->OnClicked.AddDynamic(this, &ThisClass::HandleCallClicked);
	if (Btn_Save) Btn_Save->OnClicked.AddDynamic(this, &ThisClass::HandleSaveClicked);
	if (Btn_Cancel) Btn_Cancel->OnClicked.AddDynamic(this, &ThisClass::HandleCancelClicked);
}

bool ULCOPhoneMenuWidget::OpenPhoneMenu(ALCOPhone* InPhone)
{
	if (bMenuActive)
	{
		return Phone.Get() == InPhone;
	}

	APlayerController* PC = GetOwningPlayer();
	const ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC);
	if (!IsValid(InPhone) || !InPhone->IsMenuOpen() || !PC || !PC->IsLocalPlayerController()
		|| PC->bShowMouseCursor || (LCOPC && LCOPC->IsInUIMode()) || UGameplayStatics::IsGamePaused(this)
		|| !Btn_Call || !Btn_Save || !Btn_Cancel)
	{
		// 실패한 생성 요청의 전화기 잠금 해제는 BP 호출자가 담당한다.
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhoneMenu] Cannot open: check phone, owning player, buttons and active UI"));
		return false;
	}

	Phone = InPhone;
	ResetSaveFeedback();
	if (Txt_Description)
	{
		OriginalDescription = Txt_Description->GetText();
	}
	OriginalFeedbackVisibility.Reset();
	UWidget* FeedbackWidgets[] = { Border_Choices, Border_Dialogue, Btn_Call, Btn_Save, Btn_Cancel, Txt_MenuTitle, Txt_Description };
	for (UWidget* Widget : FeedbackWidgets)
	{
		if (Widget) OriginalFeedbackVisibility.Emplace(Widget, Widget->GetVisibility());
	}
	bMenuActive = true;
	bPreviousShowMouseCursor = PC->bShowMouseCursor;
	if (!PC->IsMoveInputIgnored())
	{
		PC->SetIgnoreMoveInput(true);
		bMoveInputBlockedByThisWidget = true;
	}
	if (!PC->IsLookInputIgnored())
	{
		PC->SetIgnoreLookInput(true);
		bLookInputBlockedByThisWidget = true;
	}

	AddToViewport(ViewportZOrder);
	NotifyPromptVisibility(true);
	RefreshButtonStates();

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	SetKeyboardFocus();
	return true;
}

void ULCOPhoneMenuWidget::DismissPhoneMenu()
{
	// SaveAtPhone은 결과를 받기 전에 OnMenuClosed를 호출하므로 설명창을 유지한다.
	if (!bMenuActive || IsSaveFeedbackActive())
	{
		return;
	}

	// RemoveFromParent의 NativeDestruct에서 전화기 CloseMenu를 호출하지 않도록 먼저 해제한다.
	bMenuActive = false;
	Phone.Reset();
	RestoreInput();
	NotifyPromptVisibility(false);
	if (IsInViewport()) RemoveFromParent();
}

void ULCOPhoneMenuWidget::ClosePhoneMenu()
{
	TWeakObjectPtr<ALCOPhone> PhoneToClose = Phone;
	ResetSaveFeedback();
	DismissPhoneMenu();
	if (ALCOPhone* PhoneActor = PhoneToClose.Get())
	{
		PhoneActor->CloseMenu();
	}
}

void ULCOPhoneMenuWidget::NativeDestruct()
{
	// BP가 직접 RemoveFromParent한 경우에도 전화기의 메뉴 상태를 해제한다.
	ClosePhoneMenu();
	Super::NativeDestruct();
}

void ULCOPhoneMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bMenuActive)
	{
		// 외부 종료의 즉시 알림이 없으므로 표시 중 Tick에서 보완한다.
		if (!Phone.IsValid())
		{
			ClosePhoneMenu();
			return;
		}
		if (!Phone->IsMenuOpen() && !IsSaveFeedbackActive())
		{
			DismissPhoneMenu();
			return;
		}
		RefreshButtonStates();
	}
}

FReply ULCOPhoneMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bMenuActive && IsSaveFeedbackActive())
	{
		if (InKeyEvent.GetKey() == EKeys::F)
		{
			// 저장 중 입력과 길게 누른 키의 반복 입력은 결과 확인으로 사용하지 않는다.
			if (bShowingSaveResult && !InKeyEvent.IsRepeat()) FinishSaveFeedback();
			return FReply::Handled();
		}
		// 저장 결과는 F/클릭으로만 확인한다. 대기 중 Escape로 닫지 않는다.
		if (InKeyEvent.GetKey() == EKeys::Escape) return FReply::Handled();
	}
	if (bMenuActive && InKeyEvent.GetKey() == EKeys::Escape)
	{
		ClosePhoneMenu();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply ULCOPhoneMenuWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bMenuActive && IsSaveFeedbackActive() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (bShowingSaveResult) FinishSaveFeedback();
		// 저장 중에는 클릭을 소비한다. 저장 버튼을 선택한 MouseUp으로 결과를 닫지 않는다.
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void ULCOPhoneMenuWidget::RefreshButtonStates()
{
	const ALCOPhone* PhoneActor = Phone.Get();
	const bool bAvailable = bMenuActive && !IsSaveFeedbackActive() && PhoneActor && PhoneActor->IsMenuOpen() && !PhoneActor->IsInCall();
	if (Btn_Call) Btn_Call->SetIsEnabled(bAvailable && PhoneActor->CanCall());
	if (Btn_Save) Btn_Save->SetIsEnabled(bAvailable && PhoneActor->CanSave());
	if (Btn_Cancel) Btn_Cancel->SetIsEnabled(bMenuActive && !IsSaveFeedbackActive());
}

void ULCOPhoneMenuWidget::HandleCallClicked()
{
	TWeakObjectPtr<ALCOPhone> PhoneToCall = Phone;
	if (!bMenuActive || IsSaveFeedbackActive() || !PhoneToCall.IsValid() || !PhoneToCall->IsMenuOpen())
	{
		return;
	}
	// StartCall이 동기적으로 여는 대화 UI보다 먼저 입력과 커서를 복구한다.
	// CloseMenu는 호출하지 않는다: StartCall이 MenuUser를 가져와야 한다.
	DismissPhoneMenu();
	if (ALCOPhone* PhoneActor = PhoneToCall.Get()) PhoneActor->StartCall();
}

void ULCOPhoneMenuWidget::HandleSaveClicked()
{
	TWeakObjectPtr<ALCOPhone> PhoneToSave = Phone;
	UWorld* World = GetWorld();
	if (!bMenuActive || IsSaveFeedbackActive() || !PhoneToSave.IsValid() || !PhoneToSave->IsMenuOpen() || !World)
	{
		return;
	}

	// 동기 실패의 OnGameSaved(false)와 OnMenuClosed보다 먼저 대기 상태를 설정한다.
	bWaitingForSaveResult = true;
	bMinimumSaveWaitElapsed = false;
	bHasReceivedSaveResult = false;
	bPendingSaveSuccess = false;
	SavePendingAnimationIndex = 0;
	UE_LOG(LogTemp, Log, TEXT("[LCOPhoneMenu] Save requested: Widget=%s Phone=%s"), *GetPathName(), *GetNameSafe(PhoneToSave.Get()));
	if (!Border_Choices || !Border_Dialogue)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhoneMenu] Check WBP border names: Border_Choices=%s Border_Dialogue=%s. Button visibility fallback will be used"),
			*GetNameSafe(Border_Choices), *GetNameSafe(Border_Dialogue));
	}
	ApplySaveFeedbackLayout();
	RefreshButtonStates();
	if (Txt_Description)
	{
		Txt_Description->SetVisibility(ESlateVisibility::Visible);
		RefreshSavePendingText();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhoneMenu] Txt_Description is missing; check WBP binding"));
	}
	// 즉시 실패 결과도 최소 대기 시간을 거치도록 SaveAtPhone 호출 전에 등록한다.
	World->GetTimerManager().SetTimer(MinimumSaveWaitTimerHandle, this,
		&ThisClass::HandleMinimumSaveWaitElapsed, 3.f, false);
	World->GetTimerManager().SetTimer(SavePendingAnimationTimerHandle, this,
		&ThisClass::AdvanceSavePendingAnimation, 0.5f, true);
	if (ALCOPhone* PhoneActor = PhoneToSave.Get())
	{
		const bool bAccepted = PhoneActor->SaveAtPhone();
		if (!bAccepted && bWaitingForSaveResult && !bHasReceivedSaveResult)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOPhoneMenu] Save rejected but result not forwarded. Connect OnGameSaved to ShowSaveResult"));
		}
	}
}

void ULCOPhoneMenuWidget::ShowSaveResult(bool bSuccess)
{
	// 중복 결과와 취소/파괴 이후 도착한 결과는 무시한다.
	if (!bMenuActive || !bWaitingForSaveResult || bHasReceivedSaveResult || !Phone.IsValid() || !IsInViewport())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhoneMenu] Ignored save result: Widget=%s Active=%d Waiting=%d. Forward OnGameSaved to the instance that requested this save"),
			*GetPathName(), bMenuActive, bWaitingForSaveResult);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[LCOPhoneMenu] Save result: Widget=%s Phone=%s Success=%d"), *GetPathName(), *GetNameSafe(Phone.Get()), bSuccess);
	bHasReceivedSaveResult = true;
	bPendingSaveSuccess = bSuccess;
	TryShowSaveResult();
}

void ULCOPhoneMenuWidget::HandleMinimumSaveWaitElapsed()
{
	if (!bMenuActive || !bWaitingForSaveResult) return;
	bMinimumSaveWaitElapsed = true;
	TryShowSaveResult();
}

void ULCOPhoneMenuWidget::TryShowSaveResult()
{
	if (!bMenuActive || !bWaitingForSaveResult || !bMinimumSaveWaitElapsed || !bHasReceivedSaveResult
		|| !Phone.IsValid() || !IsInViewport()) return;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SavePendingAnimationTimerHandle);
	bWaitingForSaveResult = false;
	bShowingSaveResult = true;
	ApplySaveFeedbackLayout();
	if (Txt_Description)
	{
		Txt_Description->SetVisibility(ESlateVisibility::Visible);
		Txt_Description->SetText(bPendingSaveSuccess
			? ResolveFeedbackText(TEXT("PHONE_SAVE_SUCCESS"), NSLOCTEXT("LCOPhoneMenu", "SaveSuccess", "진행 상황을 기록했다."))
			: ResolveFeedbackText(TEXT("PHONE_SAVE_FAILED"), NSLOCTEXT("LCOPhoneMenu", "SaveFailed", "기록하지 못했다. 다시 시도해야겠다.")));
	}
	RefreshButtonStates();
}

void ULCOPhoneMenuWidget::FinishSaveFeedback()
{
	if (!bShowingSaveResult) return;
	ResetSaveFeedback();
	DismissPhoneMenu();
}

void ULCOPhoneMenuWidget::ResetSaveFeedback()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MinimumSaveWaitTimerHandle);
		World->GetTimerManager().ClearTimer(SavePendingAnimationTimerHandle);
	}
	if (IsSaveFeedbackActive())
	{
		if (Txt_Description) Txt_Description->SetText(OriginalDescription);
		for (const auto& Entry : OriginalFeedbackVisibility)
		{
			if (UWidget* Widget = Entry.Key.Get()) Widget->SetVisibility(Entry.Value);
		}
	}
	bWaitingForSaveResult = false;
	bShowingSaveResult = false;
	bMinimumSaveWaitElapsed = false;
	bHasReceivedSaveResult = false;
	bPendingSaveSuccess = false;
	SavePendingAnimationIndex = 0;
}

void ULCOPhoneMenuWidget::AdvanceSavePendingAnimation()
{
	if (!bMenuActive || !bWaitingForSaveResult || !Phone.IsValid() || !IsInViewport()) return;
	SavePendingAnimationIndex = (SavePendingAnimationIndex + 1) % 3;
	RefreshSavePendingText();
}

void ULCOPhoneMenuWidget::RefreshSavePendingText()
{
	if (!Txt_Description || !bWaitingForSaveResult) return;
	switch (SavePendingAnimationIndex)
	{
	case 1:
		Txt_Description->SetText(ResolveFeedbackText(TEXT("PHONE_SAVE_PENDING_TWO_DOTS"),
			NSLOCTEXT("LCOPhoneMenu", "SavePendingTwoDots", "기록 중..")));
		break;
	case 2:
		Txt_Description->SetText(ResolveFeedbackText(TEXT("PHONE_SAVE_PENDING"),
			NSLOCTEXT("LCOPhoneMenu", "SavePending", "기록 중...")));
		break;
	default:
		Txt_Description->SetText(ResolveFeedbackText(TEXT("PHONE_SAVE_PENDING_ONE_DOT"),
			NSLOCTEXT("LCOPhoneMenu", "SavePendingOneDot", "기록 중.")));
		break;
	}
}

void ULCOPhoneMenuWidget::ApplySaveFeedbackLayout()
{
	if (Border_Choices) Border_Choices->SetVisibility(ESlateVisibility::Collapsed);
	// Border가 아직 이름으로 바인딩되지 않은 WBP에서도 선택 버튼은 숨긴다.
	if (Btn_Call) Btn_Call->SetVisibility(ESlateVisibility::Collapsed);
	if (Btn_Save) Btn_Save->SetVisibility(ESlateVisibility::Collapsed);
	if (Btn_Cancel) Btn_Cancel->SetVisibility(ESlateVisibility::Collapsed);
	if (Border_Dialogue) Border_Dialogue->SetVisibility(ESlateVisibility::Visible);
	if (Txt_MenuTitle) Txt_MenuTitle->SetVisibility(ESlateVisibility::Visible);
	if (Txt_Description) Txt_Description->SetVisibility(ESlateVisibility::Visible);
	SetVisibility(ESlateVisibility::Visible);
	// 선택 버튼이 숨겨지면서 포커스를 잃어도 F/클릭을 이 위젯에서 받는다.
	SetKeyboardFocus();
}

FText ULCOPhoneMenuWidget::ResolveFeedbackText(FName Key, const FText& Fallback) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr;
	if (Localization)
	{
		const FText Text = Localization->GetUIText(Key);
		if (!Text.IsEmpty() && Text.ToString() != Key.ToString()) return Text;
	}
	return Fallback;
}

void ULCOPhoneMenuWidget::HandleCancelClicked()
{
	ClosePhoneMenu();
}

void ULCOPhoneMenuWidget::RestoreInput()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}
	if (bMoveInputBlockedByThisWidget)
	{
		PC->SetIgnoreMoveInput(false);
		bMoveInputBlockedByThisWidget = false;
	}
	if (bLookInputBlockedByThisWidget)
	{
		PC->SetIgnoreLookInput(false);
		bLookInputBlockedByThisWidget = false;
	}

	// 다른 UI가 입력을 가져갔다면 해당 입력 모드를 덮어쓰지 않는다.
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), true);
	const bool bAnotherWidgetHasFocus = Widgets.ContainsByPredicate([this, PC](const UUserWidget* Widget)
	{
		return Widget && Widget != this && Widget->GetOwningPlayer() == PC
			&& (Widget->HasUserFocus(PC) || Widget->HasUserFocusedDescendants(PC));
	});
	const ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC);
	if (!bAnotherWidgetHasFocus && !(LCOPC && LCOPC->IsInUIMode()) && !UGameplayStatics::IsGamePaused(this))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(bPreviousShowMouseCursor);
	}
}

void ULCOPhoneMenuWidget::NotifyPromptVisibility(bool bVisible)
{
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* Prompt = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			Prompt->NotifyBlockingWidgetVisibilityChanged(this, bVisible);
		}
	}
}
