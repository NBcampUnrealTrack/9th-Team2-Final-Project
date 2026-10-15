#include "UI/LCOPhoneMenuWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Interaction/LCOPhone.h"
#include "Kismet/GameplayStatics.h"
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
	if (!bMenuActive)
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
		if (!Phone.IsValid() || !Phone->IsMenuOpen())
		{
			DismissPhoneMenu();
			return;
		}
		RefreshButtonStates();
	}
}

FReply ULCOPhoneMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bMenuActive && InKeyEvent.GetKey() == EKeys::Escape)
	{
		ClosePhoneMenu();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void ULCOPhoneMenuWidget::RefreshButtonStates()
{
	const ALCOPhone* PhoneActor = Phone.Get();
	const bool bAvailable = bMenuActive && PhoneActor && PhoneActor->IsMenuOpen() && !PhoneActor->IsInCall();
	if (Btn_Call) Btn_Call->SetIsEnabled(bAvailable && PhoneActor->CanCall());
	if (Btn_Save) Btn_Save->SetIsEnabled(bAvailable && PhoneActor->CanSave());
	if (Btn_Cancel) Btn_Cancel->SetIsEnabled(bMenuActive);
}

void ULCOPhoneMenuWidget::HandleCallClicked()
{
	TWeakObjectPtr<ALCOPhone> PhoneToCall = Phone;
	if (!bMenuActive || !PhoneToCall.IsValid() || !PhoneToCall->IsMenuOpen())
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
	if (!bMenuActive || !PhoneToSave.IsValid() || !PhoneToSave->IsMenuOpen())
	{
		return;
	}
	DismissPhoneMenu();
	if (ALCOPhone* PhoneActor = PhoneToSave.Get()) PhoneActor->SaveAtPhone();
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
