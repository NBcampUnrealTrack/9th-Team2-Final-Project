#include "UI/LCOMonologueSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "UI/LCOMonologueWidget.h"

void ULCOMonologueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
}

void ULCOMonologueSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	CleanupWidget();
	Super::Deinitialize();
}

void ULCOMonologueSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	// 컨트롤러가 바뀌면 다음 독백 때 새로 생성
	if (MonologueWidget && MonologueWidget->GetOwningPlayer() != NewPlayerController)
	{
		CleanupWidget();
	}
}

void ULCOMonologueSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (MonologueWidget && MonologueWidget->GetWorld() == World)
	{
		CleanupWidget();
	}
}

ULCOMonologueSubsystem* ULCOMonologueSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
	return LocalPlayer ? LocalPlayer->GetSubsystem<ULCOMonologueSubsystem>() : nullptr;
}

bool ULCOMonologueSubsystem::ShowMonologue(const UObject* WorldContextObject, const FText& Text, float Duration)
{
	ULCOMonologueSubsystem* Subsystem = Get(WorldContextObject);
	return Subsystem ? Subsystem->Show(Text, Duration) : false;
}

bool ULCOMonologueSubsystem::ShowMonologueByKey(const UObject* WorldContextObject, FName TextKey, float Duration)
{
	if (TextKey.IsNone())
	{
		return false;
	}

	ULCOMonologueSubsystem* Subsystem = Get(WorldContextObject);
	if (!Subsystem)
	{
		return false;
	}

	FText Text = FText::FromName(TextKey);
	const ULocalPlayer* LocalPlayer = Subsystem->GetLocalPlayer();
	if (const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr)
	{
		if (const ULCOLocalizationSubsystem* Localization = GameInstance->GetSubsystem<ULCOLocalizationSubsystem>())
		{
			Text = Localization->GetUIText(TextKey);
		}
	}

	return Subsystem->Show(Text, Duration);
}

void ULCOMonologueSubsystem::ClearMonologues(const UObject* WorldContextObject)
{
	if (ULCOMonologueSubsystem* Subsystem = Get(WorldContextObject))
	{
		Subsystem->Clear();
	}
}

bool ULCOMonologueSubsystem::Show(const FText& Text, float Duration)
{
	if (Text.IsEmpty())
	{
		return false;
	}

	ULCOMonologueWidget* Widget = GetOrCreateWidget();
	if (!Widget)
	{
		return false;
	}

	// 같은 문장 반복 방지 (표시 / 대기 중이거나 최근에 보여준 문장)
	const FString Key = Text.ToString();
	const double Now = FPlatformTime::Seconds();
	if (Widget->IsShowingOrQueued(Text))
	{
		return true;
	}
	if (const double* LastTime = LastShownTimes.Find(Key))
	{
		if (Now - *LastTime < RepeatCooldown)
		{
			return true;
		}
	}
	LastShownTimes.Add(Key, Now);

	Widget->EnqueueMonologue(Text, Duration);
	return true;
}

void ULCOMonologueSubsystem::Clear()
{
	if (MonologueWidget)
	{
		MonologueWidget->ClearAll();
	}
}

ULCOMonologueWidget* ULCOMonologueSubsystem::GetOrCreateWidget()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController = LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
	if (!PlayerController)
	{
		return nullptr;
	}

	if (MonologueWidget && MonologueWidget->GetOwningPlayer() == PlayerController)
	{
		if (!MonologueWidget->IsInViewport())
		{
			MonologueWidget->AddToViewport(ViewportZOrder);
		}
		return MonologueWidget;
	}

	CleanupWidget();

	if (MonologueWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOMonologueSubsystem] MonologueWidgetClass 미설정 - DefaultGame.ini 확인"));
		return nullptr;
	}

	TSubclassOf<ULCOMonologueWidget> LoadedClass = MonologueWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOMonologueSubsystem] MonologueWidgetClass 로드 실패: %s"), *MonologueWidgetClass.ToString());
		return nullptr;
	}

	MonologueWidget = CreateWidget<ULCOMonologueWidget>(PlayerController, LoadedClass);
	if (MonologueWidget)
	{
		MonologueWidget->AddToViewport(ViewportZOrder);
	}
	return MonologueWidget;
}

void ULCOMonologueSubsystem::CleanupWidget()
{
	if (MonologueWidget)
	{
		MonologueWidget->ClearAll();
		MonologueWidget->RemoveFromParent();
		MonologueWidget = nullptr;
	}
}
