#include "UI/LCOPhoneMenuWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interaction/LCOPhone.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Player/LCOPlayerController.h"
#include "Save/LCOSaveManager.h"
#include "UI/LCOInteractionPromptSubsystem.h"

namespace
{
	void SetPhoneButtonText(UWidget* Widget, const FText& Text)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget)) TextBlock->SetText(Text);
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				SetPhoneButtonText(Panel->GetChildAt(Index), Text);
			}
		}
	}
}

void ULCOPhoneMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	if (Btn_Call) Btn_Call->OnClicked.AddDynamic(this, &ThisClass::HandleCallClicked);
	if (Btn_Cancel) Btn_Cancel->OnClicked.AddDynamic(this, &ThisClass::HandleCancelClicked);
}

void ULCOPhoneMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ULCOLocalizationSubsystem* Localization = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		Localization->OnLanguageChanged.AddUniqueDynamic(this, &ThisClass::HandleLanguageChanged);
	}
	RefreshMenuTexts();
}

void ULCOPhoneMenuWidget::HandleLanguageChanged(FName Language)
{
	RefreshMenuTexts();
}

void ULCOPhoneMenuWidget::RefreshMenuTexts()
{
	SetPhoneButtonText(Btn_Call, ResolveFeedbackText(TEXT("PHONE_CALL_OFFICE"),
		NSLOCTEXT("LCOPhoneMenu", "CallOffice", "회사에 전화하기")));
	SetPhoneButtonText(Btn_Cancel, ResolveFeedbackText(TEXT("PHONE_HANG_UP"),
		NSLOCTEXT("LCOPhoneMenu", "HangUp", "수화기 내려놓기")));
	if (Btn_Save) Btn_Save->SetVisibility(ESlateVisibility::Collapsed);
}

bool ULCOPhoneMenuWidget::OpenPhoneMenu(ALCOPhone* InPhone)
{
	return OpenPhoneMenuInternal(InPhone, false);
}

bool ULCOPhoneMenuWidget::OpenPhoneMenuInternal(ALCOPhone* InPhone, bool bForReport)
{
	if (bMenuActive)
	{
		return Phone.Get() == InPhone;
	}

	APlayerController* PC = GetOwningPlayer();
	const ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC);
	if (!IsValid(InPhone) || (!bForReport && !InPhone->IsMenuOpen()) || !PC || !PC->IsLocalPlayerController()
		|| PC->bShowMouseCursor || (LCOPC && LCOPC->IsInUIMode()) || UGameplayStatics::IsGamePaused(this)
		|| !Btn_Call || !Btn_Cancel)
	{
		// 실패한 생성 요청의 전화기 잠금 해제는 BP 호출자가 담당한다.
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhoneMenu] Cannot open: check phone, owning player, buttons and active UI"));
		return false;
	}

	Phone = InPhone;
	ResetSaveFeedback();
	RefreshMenuTexts();
	if (Txt_Description)
	{
		OriginalDescription = Txt_Description->GetText();
	}
	OriginalFeedbackVisibility.Reset();
	UWidget* FeedbackWidgets[] = { Border_Choices, Border_PhoneChoices, Border_Dialogue, Btn_Call, Btn_Save, Btn_Cancel, Txt_MenuTitle, Txt_Description };
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

	// Keep unhandled menu keys (O) available to the controller's existing options binding.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetWidgetToFocus(TakeWidget());
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	SetKeyboardFocus();
	return true;
}

void ULCOPhoneMenuWidget::DismissPhoneMenu()
{
	// SaveAtPhone은 결과를 받기 전에 OnMenuClosed를 호출하므로 설명창을 유지한다.
	if (!bMenuActive || bWaitingForCall || IsSaveFeedbackActive())
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
	ClearCallTracking();
	ResetSaveFeedback();
	DismissPhoneMenu();
	if (ALCOPhone* PhoneActor = PhoneToClose.Get())
	{
		PhoneActor->CloseMenu();
	}
}

void ULCOPhoneMenuWidget::NativeDestruct()
{
	if (ULCOLocalizationSubsystem* Localization = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		Localization->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
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
		if (!Phone->IsMenuOpen() && !bWaitingForCall && !IsSaveFeedbackActive())
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
	if (Btn_Save)
	{
		Btn_Save->SetIsEnabled(false);
		Btn_Save->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Btn_Cancel) Btn_Cancel->SetIsEnabled(bMenuActive && !IsSaveFeedbackActive());
}

void ULCOPhoneMenuWidget::HandleCallClicked()
{
	TWeakObjectPtr<ALCOPhone> PhoneToCall = Phone;
	if (!bMenuActive || bWaitingForCall || IsSaveFeedbackActive() || !PhoneToCall.IsValid() || !PhoneToCall->IsMenuOpen())
	{
		return;
	}
	// Keep this instance alive but invisible until its own call finishes.
	bWaitingForCall = true;
	bWorkSeniorCall = false;
	bReportQueued = false;
	PhoneToCall->OnCallStartedDelegate.AddUniqueDynamic(this, &ThisClass::HandleCallStarted);
	PhoneToCall->OnCallFinishedDelegate.AddUniqueDynamic(this, &ThisClass::HandleCallFinished);
	RestoreInput();
	NotifyPromptVisibility(false);
	SetVisibility(ESlateVisibility::Collapsed);
	if (!PhoneToCall->StartCall()) ClosePhoneMenu();
}

void ULCOPhoneMenuWidget::HandleCallStarted(ALCOPhone* InPhone, AActor* Caller, int32 EntryIndex, FName DialogueID, FName StartNodeID)
{
	if (bWaitingForCall && Phone.Get() == InPhone)
	{
		bWorkSeniorCall = DialogueID == FName(TEXT("WorkSenior"));
	}
}

void ULCOPhoneMenuWidget::HandleCallFinished(ALCOPhone* InPhone, AActor* Caller, bool bCompletedNormally)
{
	if (!bWaitingForCall || bReportQueued || Phone.Get() != InPhone) return;
	if (!bCompletedNormally || !bWorkSeniorCall || UGameplayStatics::IsGamePaused(this) || !GetWorld())
	{
		ClosePhoneMenu();
		return;
	}
	// Leave the dialogue-close/options broadcast stack before acquiring UI input again.
	bReportQueued = true;
	// Observe an automatic call save immediately, even if it finishes before the next tick.
	if (!InPhone->CanSave())
	{
		ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>();
		if (SaveManager && SaveManager->IsSaving())
		{
			bReuseCallSave = true;
			ObservedSaveManager = SaveManager;
			SaveManager->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleExistingSaveFinished);
		}
	}
	CompletedCallTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleCompletedCall);
}

void ULCOPhoneMenuWidget::ClearCallTracking()
{
	if (ALCOPhone* PhoneActor = Phone.Get())
	{
		PhoneActor->OnCallStartedDelegate.RemoveDynamic(this, &ThisClass::HandleCallStarted);
		PhoneActor->OnCallFinishedDelegate.RemoveDynamic(this, &ThisClass::HandleCallFinished);
	}
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CompletedCallTimerHandle);
	bWaitingForCall = false;
	bWorkSeniorCall = false;
	bReportQueued = false;
	bReuseCallSave = false;
	CompletedCallSaveResult.Reset();
}

void ULCOPhoneMenuWidget::HandleCompletedCall()
{
	if (!bWaitingForCall || !bReportQueued) return;
	TWeakObjectPtr<ALCOPhone> CompletedPhone = Phone;
	const bool bReuseSave = bReuseCallSave;
	const TOptional<bool> ExistingSaveResult = CompletedCallSaveResult;
	ClosePhoneMenu();
	if (!CompletedPhone.IsValid() || CompletedPhone->IsInCall() || UGameplayStatics::IsGamePaused(this)) return;
	if (OpenPhoneMenuInternal(CompletedPhone.Get(), true)) BeginReportSave(bReuseSave, ExistingSaveResult);
}

void ULCOPhoneMenuWidget::BeginReportSave(bool bReuseSave, TOptional<bool> ExistingSaveResult)
{
	TWeakObjectPtr<ALCOPhone> PhoneToSave = Phone;
	UWorld* World = GetWorld();
	if (!bMenuActive || bWaitingForCall || IsSaveFeedbackActive() || !PhoneToSave.IsValid() || PhoneToSave->IsInCall() || !World)
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
	if (!Border_PhoneChoices || !Border_ReportFade || !Border_Dialogue)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPhoneMenu] Optional report borders missing: Choices=%s Fade=%s Dialogue=%s. Available widgets will be used"),
			*GetNameSafe(Border_PhoneChoices), *GetNameSafe(Border_ReportFade), *GetNameSafe(Border_Dialogue));
	}
	ApplySaveFeedbackLayout();
	StartReportFade(false);
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
		if (bReuseSave && ExistingSaveResult.IsSet())
		{
			ShowSaveResult(ExistingSaveResult.GetValue());
			return;
		}
		// Do not submit a second save if the phone already started an automatic save.
		if (bReuseSave || !PhoneActor->CanSave())
		{
			ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>();
			if (SaveManager && SaveManager->IsSaving())
			{
				ObservedSaveManager = SaveManager;
				SaveManager->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleExistingSaveFinished);
			}
			else ShowSaveResult(false);
			return;
		}
		// The existing BP result forwarding may also notify this instance; duplicates are ignored.
		if (ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>())
		{
			ObservedSaveManager = SaveManager;
			SaveManager->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleExistingSaveFinished);
		}
		const bool bAccepted = PhoneActor->SaveAtPhone();
		if (!bAccepted && bWaitingForSaveResult && !bHasReceivedSaveResult)
		{
			HandleExistingSaveFinished(false);
		}
	}
}

void ULCOPhoneMenuWidget::HandleExistingSaveFinished(bool bSuccess)
{
	if (ULCOSaveManager* SaveManager = ObservedSaveManager.Get())
	{
		SaveManager->OnSaveFinished.RemoveDynamic(this, &ThisClass::HandleExistingSaveFinished);
	}
	ObservedSaveManager.Reset();
	if (bWaitingForCall && bReportQueued)
	{
		CompletedCallSaveResult = bSuccess;
		return;
	}
	ShowSaveResult(bSuccess);
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
	if (!bMenuActive || !bWaitingForSaveResult || bReportFadeActive || !bMinimumSaveWaitElapsed || !bHasReceivedSaveResult
		|| !Phone.IsValid() || !IsInViewport()) return;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SavePendingAnimationTimerHandle);
	bWaitingForSaveResult = false;
	bShowingSaveResult = true;
	ApplySaveFeedbackLayout();
	if (Txt_Description)
	{
		Txt_Description->SetVisibility(ESlateVisibility::Visible);
		Txt_Description->SetText(bPendingSaveSuccess
			? ResolveFeedbackText(TEXT("PHONE_SAVE_SUCCESS"), NSLOCTEXT("LCOPhoneMenu", "SaveSuccess", "회사에 조사 상황을 보고했다."))
			: ResolveFeedbackText(TEXT("PHONE_SAVE_FAILED"), NSLOCTEXT("LCOPhoneMenu", "SaveFailed", "회사에 보고하지 못했다. 다시 시도해야겠다.")));
	}
	RefreshButtonStates();
}

void ULCOPhoneMenuWidget::FinishSaveFeedback()
{
	if (!bMenuActive || !bShowingSaveResult || bReportFadeActive) return;
	if (bPendingSaveSuccess && !bShowingRestMessage)
	{
		bShowingRestMessage = true;
		if (Txt_Description)
		{
			Txt_Description->SetText(ResolveFeedbackText(TEXT("PHONE_REPORT_REST"),
				NSLOCTEXT("LCOPhoneMenu", "ReportRest", "그리고 당신은 잠시 숨을 고르며 피로를 풀었다.")));
		}
		return;
	}
	StartReportFade(true);
}

void ULCOPhoneMenuWidget::StartReportFade(bool bFadeOut)
{
	if (!bMenuActive || bReportFadeActive) return;
	UWorld* World = GetWorld();
	if (!Border_ReportFade || !World)
	{
		// Missing optional artwork must not leave input blocked or prevent closing.
		if (bFadeOut) ClosePhoneMenu();
		return;
	}
	// Both fade directions belong to the same uninterrupted report sequence.
	// Acquire the lock before changing opacity or starting the fade timer.
	bBlockPauseForReport = true;
	bReportFadeActive = true;
	bReportFadingOut = bFadeOut;
	ReportFadeStartTime = World->GetTimeSeconds();
	Border_ReportFade->SetVisibility(ESlateVisibility::HitTestInvisible);
	Border_ReportFade->SetRenderOpacity(bFadeOut ? 1.f : 0.f);
	World->GetTimerManager().SetTimer(ReportFadeTimerHandle, this, &ThisClass::UpdateReportFade, 0.02f, true);
}

void ULCOPhoneMenuWidget::UpdateReportFade()
{
	UWorld* World = GetWorld();
	if (!bMenuActive || !Phone.IsValid() || !IsInViewport() || !World || !Border_ReportFade)
	{
		ClosePhoneMenu();
		return;
	}
	const float Progress = FMath::Clamp(World->GetTimeSeconds() - ReportFadeStartTime, 0.f, 1.f);
	Border_ReportFade->SetRenderOpacity(bReportFadingOut ? 1.f - Progress : Progress);
	if (Progress < 1.f) return;
	World->GetTimerManager().ClearTimer(ReportFadeTimerHandle);
	bReportFadeActive = false;
	if (bReportFadingOut)
	{
		ClosePhoneMenu();
	}
	else
	{
		TryShowSaveResult();
	}
}

void ULCOPhoneMenuWidget::ResetSaveFeedback()
{
	if (ULCOSaveManager* SaveManager = ObservedSaveManager.Get())
	{
		SaveManager->OnSaveFinished.RemoveDynamic(this, &ThisClass::HandleExistingSaveFinished);
	}
	ObservedSaveManager.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MinimumSaveWaitTimerHandle);
		World->GetTimerManager().ClearTimer(SavePendingAnimationTimerHandle);
		World->GetTimerManager().ClearTimer(ReportFadeTimerHandle);
	}
	if (Border_ReportFade)
	{
		Border_ReportFade->SetRenderOpacity(0.f);
		Border_ReportFade->SetVisibility(ESlateVisibility::Collapsed);
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
	bShowingRestMessage = false;
	bReportFadeActive = false;
	bReportFadingOut = false;
	bBlockPauseForReport = false;
	ReportFadeStartTime = 0.f;
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
			NSLOCTEXT("LCOPhoneMenu", "SavePendingTwoDots", "보고 중..")));
		break;
	case 2:
		Txt_Description->SetText(ResolveFeedbackText(TEXT("PHONE_SAVE_PENDING"),
			NSLOCTEXT("LCOPhoneMenu", "SavePending", "보고 중...")));
		break;
	default:
		Txt_Description->SetText(ResolveFeedbackText(TEXT("PHONE_SAVE_PENDING_ONE_DOT"),
			NSLOCTEXT("LCOPhoneMenu", "SavePendingOneDot", "보고 중.")));
		break;
	}
}

void ULCOPhoneMenuWidget::ApplySaveFeedbackLayout()
{
	if (Border_Choices) Border_Choices->SetVisibility(ESlateVisibility::Collapsed);
	if (Border_PhoneChoices) Border_PhoneChoices->SetVisibility(ESlateVisibility::Collapsed);
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
