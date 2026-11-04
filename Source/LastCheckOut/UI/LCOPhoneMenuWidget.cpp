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
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Interaction/LCOPhone.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Player/LCOPlayerController.h"
#include "Save/LCOSaveManager.h"
#include "UI/LCOInteractionPromptSubsystem.h"
#include "UI/LCODialogueSubsystem.h"

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
	if (Btn_Save) Btn_Save->OnClicked.AddDynamic(this, &ThisClass::HandleReportClicked);
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
	if (Txt_MenuTitle)
	{
		Txt_MenuTitle->SetText(ResolveFeedbackText(TEXT("PHONE_TITLE"),
			NSLOCTEXT("LCOPhoneMenu", "Title", "전화기")));
	}
	SetPhoneButtonText(Btn_Call, ResolveFeedbackText(TEXT("PHONE_CALL_SENIOR"),
		NSLOCTEXT("LCOPhoneMenu", "CallSenior", "선배에게 전화한다.")));
	SetPhoneButtonText(Btn_Save, ResolveFeedbackText(TEXT("PHONE_REPORT_COMPANY"),
		NSLOCTEXT("LCOPhoneMenu", "ReportCompany", "진행 상황을 회사에 보고한다.")));
	SetPhoneButtonText(Btn_Cancel, ResolveFeedbackText(TEXT("PHONE_HANG_UP"),
		NSLOCTEXT("LCOPhoneMenu", "HangUp", "수화기 내려놓기")));
	OriginalDescription = ResolveFeedbackText(TEXT("PHONE_DESCRIPTION"),
		NSLOCTEXT("LCOPhoneMenu", "Description", "평범한 전화기다."));
	// Keep report/save/rest text intact when the language changes during that sequence.
	if (Txt_Description && !IsSaveFeedbackActive() && !bWaitingForCall)
	{
		Txt_Description->SetText(OriginalDescription);
	}
	if (Btn_Save && !IsSaveFeedbackActive()) Btn_Save->SetVisibility(ESlateVisibility::Visible);
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
		const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
		const ULCODialogueSubsystem* Dialogue = LocalPlayer ? LocalPlayer->GetSubsystem<ULCODialogueSubsystem>() : nullptr;
		Btn_Save->SetIsEnabled(bAvailable && Dialogue && Dialogue->HasCompletedWorkSeniorCall() && PhoneActor->CanSave());
		Btn_Save->SetVisibility(IsSaveFeedbackActive() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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
	bReportQueued = false;
	PhoneToCall->OnCallFinishedDelegate.AddUniqueDynamic(this, &ThisClass::HandleCallFinished);
	RestoreInput();
	NotifyPromptVisibility(false);
	SetVisibility(ESlateVisibility::Collapsed);
	if (!PhoneToCall->StartCall()) ClosePhoneMenu();
}

void ULCOPhoneMenuWidget::HandleCallFinished(ALCOPhone* InPhone, AActor* Caller, bool bCompletedNormally)
{
	// General calls retain phone/story completion, but never start a report or save.
	if (bWaitingForCall && !bWaitingForReport && Phone.Get() == InPhone) ClosePhoneMenu();
}

void ULCOPhoneMenuWidget::HandleReportClicked()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	ULCODialogueSubsystem* Dialogue = LocalPlayer ? LocalPlayer->GetSubsystem<ULCODialogueSubsystem>() : nullptr;
	if (!bMenuActive || bWaitingForCall || IsSaveFeedbackActive() || !Phone.IsValid()
		|| !Phone->IsMenuOpen() || Phone->IsInCall() || !Phone->CanSave()
		|| !Dialogue || !Dialogue->HasCompletedWorkSeniorCall()) return;
	bWaitingForCall = true;
	bWaitingForReport = true;
	ReportDialogueSubsystem = Dialogue;
	Dialogue->OnDialogueFinished.AddUniqueDynamic(this, &ThisClass::HandleReportFinished);
	// Keep the menu instance alive through the report and the subsequent save result.
	RestoreInput();
	NotifyPromptVisibility(false);
	SetVisibility(ESlateVisibility::Collapsed);
	if (!Dialogue->StartDialogue(TEXT("WorkSenior"), TEXT("REPORT_001"), GetOwningPlayerPawn())) ClosePhoneMenu();
}

void ULCOPhoneMenuWidget::HandleReportFinished(FName DialogueID, bool bCompletedNormally)
{
	if (!bWaitingForReport || bReportQueued || DialogueID != FName(TEXT("WorkSenior"))) return;
	if (!bCompletedNormally || !GetWorld() || UGameplayStatics::IsGamePaused(this))
	{
		ClosePhoneMenu();
		return;
	}
	bReportQueued = true;
	CompletedCallTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleCompletedCall);
}

void ULCOPhoneMenuWidget::ClearCallTracking()
{
	if (ULCODialogueSubsystem* Dialogue = ReportDialogueSubsystem.Get())
	{
		Dialogue->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleReportFinished);
	}
	ReportDialogueSubsystem.Reset();
	bWaitingForReport = false;
	if (ALCOPhone* PhoneActor = Phone.Get())
	{
		PhoneActor->OnCallFinishedDelegate.RemoveDynamic(this, &ThisClass::HandleCallFinished);
	}
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CompletedCallTimerHandle);
	bWaitingForCall = false;
	bReportQueued = false;
}

void ULCOPhoneMenuWidget::HandleCompletedCall()
{
	if (!bWaitingForCall || !bReportQueued) return;
	if (!Phone.IsValid() || Phone->IsInCall() || UGameplayStatics::IsGamePaused(this))
	{
		ClosePhoneMenu();
		return;
	}
	// Keep the existing collapsed instance. Reopening it flashes the menu/save layout.
	ClearCallTracking();
	BeginReportSave();
}

void ULCOPhoneMenuWidget::BeginReportSave()
{
	if (!bMenuActive || !Phone.IsValid() || Phone->IsInCall() || !GetWorld())
	{
		ClosePhoneMenu();
		return;
	}
	bWaitingForSaveResult = true;
	bHasReceivedSaveResult = false;
	bPendingSaveSuccess = false;
	SetVisibility(ESlateVisibility::Collapsed);
	// The dialogue released its locks after fade-in. Hold only movement/look until saving ends.
	if (APlayerController* PC = GetOwningPlayer())
	{
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
	}
	ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>();
	if (!SaveManager || !Phone->CanSave())
	{
		ShowSaveResult(false);
		return;
	}
	ObservedSaveManager = SaveManager;
	SaveManager->OnSaveFinished.AddUniqueDynamic(this, &ThisClass::HandleExistingSaveFinished);
	// true means accepted; only OnSaveFinished supplies actual success.
	if (!Phone->SaveAtPhone()) HandleExistingSaveFinished(false);
}

void ULCOPhoneMenuWidget::HandleExistingSaveFinished(bool bSuccess)
{
	if (ULCOSaveManager* SaveManager = ObservedSaveManager.Get())
	{
		SaveManager->OnSaveFinished.RemoveDynamic(this, &ThisClass::HandleExistingSaveFinished);
	}
	ObservedSaveManager.Reset();
	ShowSaveResult(bSuccess);
}

void ULCOPhoneMenuWidget::ShowSaveResult(bool bSuccess)
{
	if (bMenuActive && bWaitingForSaveResult && !Phone.IsValid())
	{
		ClosePhoneMenu();
		return;
	}
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

void ULCOPhoneMenuWidget::TryShowSaveResult()
{
	if (!bMenuActive || !bWaitingForSaveResult || !bHasReceivedSaveResult
		|| !Phone.IsValid() || !IsInViewport()) return;
	if (bPendingSaveSuccess)
	{
		// The async save finished successfully: release UI/input without another message.
		ClosePhoneMenu();
		return;
	}
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SavePendingAnimationTimerHandle);
	bWaitingForSaveResult = false;
	bShowingSaveResult = true;
	if (Txt_Description)
	{
		Txt_Description->SetVisibility(ESlateVisibility::Visible);
		Txt_Description->SetText(ResolveFeedbackText(TEXT("PHONE_SAVE_FAILED"),
			NSLOCTEXT("LCOPhoneMenu", "SaveFailed", "회사에 보고하지 못했다. 다시 시도해야겠다.")));
	}
	// Only failure is allowed to reveal the phone UI and acquire UI focus again.
	ApplySaveFeedbackLayout();
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetWidgetToFocus(TakeWidget());
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
	NotifyPromptVisibility(true);
	RefreshButtonStates();
}

void ULCOPhoneMenuWidget::FinishSaveFeedback()
{
	if (bMenuActive && bShowingSaveResult) ClosePhoneMenu();
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
		World->GetTimerManager().ClearTimer(SavePendingAnimationTimerHandle);
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
