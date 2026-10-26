#include "UI/LCOLoadingSubsystem.h"

#include "UI/LCOLoadingOverlayWidget.h"
#include "UI/SLCOLoadingScreen.h"
#include "Components/InputComponent.h"
#include "CoreGlobals.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "IAssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLCOLoading, Log, All);

namespace
{
constexpr int32 LoadingZOrder = MAX_int32;

// Consume hardware/UI events only in this game's window. Do not change input mode,
// focus, cursor policy or controller ignore-input counters owned by other UI systems.
class FLCOLoadingInputProcessor final : public IInputProcessor
{
public:
	explicit FLCOLoadingInputProcessor(UGameViewportClient* InViewport) : Viewport(InViewport) {}
	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& App, const FKeyEvent&) override { return Blocks(App); }
	virtual bool HandleKeyUpEvent(FSlateApplication& App, const FKeyEvent&) override { return Blocks(App); }
	virtual bool HandleAnalogInputEvent(FSlateApplication& App, const FAnalogInputEvent&) override { return Blocks(App); }
	virtual bool HandleMouseMoveEvent(FSlateApplication& App, const FPointerEvent&) override { return Blocks(App); }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent&) override { return Blocks(App); }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& App, const FPointerEvent&) override { return Blocks(App); }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& App, const FPointerEvent&) override { return Blocks(App); }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& App, const FPointerEvent&, const FPointerEvent*) override { return Blocks(App); }
	virtual bool HandleMotionDetectedEvent(FSlateApplication& App, const FMotionEvent&) override { return Blocks(App); }
	virtual const TCHAR* GetDebugName() const override { return TEXT("LCOLoading"); }

private:
	bool Blocks(FSlateApplication& App) const
	{
		const UGameViewportClient* Client = Viewport.Get();
		return Client && Client->GetWindow().IsValid() && Client->GetWindow() == App.GetActiveTopLevelWindow();
	}
	TWeakObjectPtr<UGameViewportClient> Viewport;
};
}

void ULCOLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Context-aware variant fires before PreLoadMap, where MoviePlayer may begin playback.
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	LoadingTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickLoading));
}

void ULCOLoadingSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(LoadingTicker);
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FinishLoading(false, TEXT("Subsystem shutdown"));
	Super::Deinitialize();
}

void ULCOLoadingSubsystem::RequestLevelTravel(FName LevelName)
{
	if (!LevelName.IsNone())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayStatics::OpenLevel(World, LevelName);
		}
	}
}

void ULCOLoadingSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	check(IsInGameThread());
	// Global map delegates can also belong to another PIE game instance.
	if (!GetGameInstance() || WorldContext.OwningGameInstance != GetGameInstance() || IsRunningDedicatedServer())
	{
		return;
	}
	const double TravelStartTime = FPlatformTime::Seconds();
	if (bLoading)
	{
		UE_LOG(LogLCOLoading, Warning, TEXT("Superseding unfinished loading with %s"), *MapName);
	}
	ReleasePlayerInput();
	RemoveScreens();
	ActiveWorld.Reset();
	bLoading = true;
	bLastLoadingFailed = false;
	ZeroShaderFrames = 0;
	LastCheckedFrame = MAX_uint64;
	RemainingShaders = 0;
	LoadingStartTime = TravelStartTime;
	PreparationStartTime = LoadingStartTime;
	RequiredLoadingCompletedTime = -1.0;
	ActiveTipText = SelectLoadingTipText();

	// This guard exists underneath MoviePlayer before LoadMap can present a new world.
	// PostLoadMap adds the UMG replacement FIRST, then removes this guard.
	if (!InstallTravelGuard())
	{
		bLastLoadingFailed = true;
		UE_LOG(LogLCOLoading, Error, TEXT("No viewport for travel guard (%s); seamless coverage cannot be guaranteed"), *MapName);
	}
	BlockPlayerInput(GetWorld());
	ShowLoadingScreen(ActiveTipText);
	UE_LOG(LogLCOLoading, Log, TEXT("Loading started: %s"), *MapName);
}

bool ULCOLoadingSubsystem::InstallTravelGuard()
{
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!Viewport || !Viewport->GetGameViewportWidget().IsValid())
	{
		return false;
	}
	ScreenViewport = Viewport;
	TravelGuard = SNew(SLCOLoadingScreen).TipText(ActiveTipText).StartTime(LoadingStartTime);
	Viewport->AddViewportWidgetContent(TravelGuard.ToSharedRef(), LoadingZOrder);
	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FLCOLoadingInputProcessor>(Viewport);
		if (!FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0))
		{
			bLastLoadingFailed = true;
			UE_LOG(LogLCOLoading, Error, TEXT("Could not register loading UI input blocker"));
		}
	}
	return TravelGuard->GetParentWidget().IsValid();
}

void ULCOLoadingSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	check(IsInGameThread());
	if (!bLoading)
	{
		return;
	}
	if (!LoadedWorld)
	{
		FinishLoading(false, TEXT("LoadMap returned no world"));
		return;
	}
	if (LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	ActiveWorld = LoadedWorld;
	PreparationStartTime = FPlatformTime::Seconds();
	RequiredLoadingCompletedTime = -1.0;
	ZeroShaderFrames = 0;
	BlockPlayerInput(LoadedWorld);
	if (!InstallOverlay(LoadedWorld))
	{
		bLastLoadingFailed = true;
		UE_LOG(LogLCOLoading, Error, TEXT("UMG handoff unavailable; retaining travel guard and retrying"));
	}
}

bool ULCOLoadingSubsystem::InstallOverlay(UWorld* World)
{
	// A replacement/retry must establish fresh readiness before starting its hold.
	RequiredLoadingCompletedTime = -1.0;
	ZeroShaderFrames = 0;
	LastCheckedFrame = MAX_uint64;
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!World || !Viewport || !Viewport->GetGameViewportWidget().IsValid())
	{
		return false;
	}
	// A replaced viewport needs a new input preprocessor as well.
	if (ScreenViewport.Get() != Viewport)
	{
		RemoveScreens();
		if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();
		InstallTravelGuard();
	}
	OverlayWidget = CreateWidget<ULCOLoadingOverlayWidget>(GetGameInstance(), ULCOLoadingOverlayWidget::StaticClass());
	if (!OverlayWidget)
	{
		return false;
	}
	OverlayWidget->InitializeLoadingScreen(ActiveTipText, LoadingStartTime);
	OverlayContent = OverlayWidget->TakeWidget();
	// Direct mounting intentionally uses the SAME pixel layout as MoviePlayer (no UMG DPI transform).
	Viewport->AddViewportWidgetContent(OverlayContent.ToSharedRef(), LoadingZOrder);
	ScreenViewport = Viewport;
	if (!OverlayContent->GetParentWidget().IsValid())
	{
		OverlayContent.Reset();
		OverlayWidget = nullptr;
		return false; // Keep the travel guard if the viewport did not accept the UMG host.
	}
	if (TravelGuard.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(TravelGuard.ToSharedRef());
		TravelGuard.Reset();
	}
	return true;
}

int32 ULCOLoadingSubsystem::GetRemainingShaders() const
{
	check(IsInGameThread());
#if WITH_EDITOR
	// Use the SAME interface and finalization-aware count as DrawMapWarnings.
	// The concrete FShaderCompilingManager override is private; do not call it directly.
	for (IAssetCompilingManager* Manager : FAssetCompilingManager::Get().GetRegisteredManagers())
	{
		if (Manager->GetAssetTypeName() == FShaderCompilingManager::GetStaticAssetTypeName())
		{
			return Manager->GetNumRemainingAssets();
		}
	}
#endif
	// Cooked games / absent compilers have no editor shader compilation to await.
	// This is not a PSO, texture streaming or level streaming readiness claim.
	return 0;
}

bool ULCOLoadingSubsystem::TickLoading(float /*DeltaTime*/)
{
	check(IsInGameThread());
	if (!bLoading)
	{
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - PreparationStartTime >= PreparationTimeoutSeconds)
	{
		UE_LOG(LogLCOLoading, Error, TEXT("LOADING TIMEOUT after %.0fs; remaining shaders=%d. Releasing cover/input as FAILURE, not ready."),
			PreparationTimeoutSeconds, RemainingShaders);
		FinishLoading(false, TEXT("Preparation timeout (fail-open)"));
		return true;
	}
	UWorld* World = ActiveWorld.Get();
	if (!World)
	{
		return true;
	}
	const bool bViewportChanged = ScreenViewport.Get() != GetGameInstance()->GetGameViewportClient();
	if (!OverlayContent.IsValid() || !OverlayContent->GetParentWidget().IsValid() || bViewportChanged)
	{
		// Detect external viewport clearing; this cannot retroactively guarantee no exposed frame.
		if (OverlayContent.IsValid())
		{
			bLastLoadingFailed = true;
			UE_LOG(LogLCOLoading, Error, TEXT("Loading cover detached externally; coverage was interrupted"));
			RemoveScreens();
		}
		InstallOverlay(World);
		return true;
	}
	BlockPlayerInput(World); // Also covers local controllers added after PostLoadMap.
	if (GetMoviePlayer()->IsMovieCurrentlyPlaying() || LastCheckedFrame == GFrameCounter)
	{
		return true;
	}
	LastCheckedFrame = GFrameCounter;
	const int32 NewRemainingShaders = GetRemainingShaders();
	if (NewRemainingShaders != RemainingShaders)
	{
		UE_LOG(LogLCOLoading, Verbose, TEXT("Shader preparation remaining: %d"), NewRemainingShaders);
	}
	RemainingShaders = NewRemainingShaders;
	if (RemainingShaders > 0)
	{
		if (RequiredLoadingCompletedTime >= 0.0)
		{
			UE_LOG(LogLCOLoading, Verbose, TEXT("New shader work invalidated readiness; restarting completion hold after preparation"));
		}
		RequiredLoadingCompletedTime = -1.0;
		ZeroShaderFrames = 0;
		return true;
	}

	// Count distinct normal engine frames. The post-completion hold replaces the old
	// extra two-frame render grace; world/render/async processing stays live throughout.
	ZeroShaderFrames = FMath::Min(ZeroShaderFrames + 1, RequiredZeroFrames);
	if (ZeroShaderFrames >= RequiredZeroFrames && RequiredLoadingCompletedTime < 0.0)
	{
		RequiredLoadingCompletedTime = Now;
		UE_LOG(LogLCOLoading, Verbose, TEXT("Required preparation complete at %.3fs; holding cover for at least %.1fs"),
			Now - LoadingStartTime, PostCompletionHoldSeconds);
	}
	if (RequiredLoadingCompletedTime >= 0.0)
	{
		// One total minimum, not separate MoviePlayer and UMG minimum durations.
		const double RevealTime = FMath::Max(LoadingStartTime + MinimumLoadingScreenTime,
			RequiredLoadingCompletedTime + PostCompletionHoldSeconds);
		if (Now >= RevealTime)
		{
			FinishLoading(!bLastLoadingFailed, TEXT("Total minimum and post-completion hold satisfied"));
		}
	}
	return true;
}

void ULCOLoadingSubsystem::BlockPlayerInput(UWorld* World)
{
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController())
		{
			continue;
		}
		const bool bAlreadyBlocked = InputBlockers.ContainsByPredicate([PC](const UInputComponent* Component)
		{
			return IsValid(Component) && Component->GetOwner() == PC;
		});
		if (!bAlreadyBlocked)
		{
			UInputComponent* Blocker = NewObject<UInputComponent>(PC);
			Blocker->Priority = MAX_int32;
			Blocker->bBlockInput = true;
			Blocker->RegisterComponent();
			PC->PushInputComponent(Blocker);
			InputBlockers.Add(Blocker);
			if (PC->PlayerInput)
			{
				PC->PlayerInput->FlushPressedKeys();
			}
		}
	}
}

void ULCOLoadingSubsystem::ReleasePlayerInput()
{
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	for (UInputComponent* Blocker : InputBlockers)
	{
		if (IsValid(Blocker))
		{
			if (APlayerController* PC = Cast<APlayerController>(Blocker->GetOwner()))
			{
				PC->PopInputComponent(Blocker);
				if (PC->PlayerInput)
				{
					PC->PlayerInput->FlushPressedKeys();
				}
			}
			Blocker->DestroyComponent();
		}
	}
	InputBlockers.Reset();
}

void ULCOLoadingSubsystem::RemoveScreens()
{
	if (UGameViewportClient* Viewport = ScreenViewport.Get())
	{
		if (OverlayContent.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(OverlayContent.ToSharedRef());
		}
		if (TravelGuard.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(TravelGuard.ToSharedRef());
		}
	}
	OverlayContent.Reset();
	OverlayWidget = nullptr;
	TravelGuard.Reset();
	ScreenViewport.Reset();
}

void ULCOLoadingSubsystem::FinishLoading(bool bSuccess, const TCHAR* Reason)
{
	const bool bWasLoading = bLoading;
	if (bLoading)
	{
		bLastLoadingFailed = !bSuccess;
		if (bSuccess)
		{
			UE_LOG(LogLCOLoading, Log, TEXT("Loading READY: %s"), Reason);
		}
		else
		{
			UE_LOG(LogLCOLoading, Warning, TEXT("Loading FAILED/ABORTED: %s"), Reason);
		}
	}
	bLoading = false;
	RemoveScreens();
	ReleasePlayerInput();
	ActiveWorld.Reset();
	ZeroShaderFrames = 0;
	RemainingShaders = 0;
	LastCheckedFrame = MAX_uint64;
	LoadingStartTime = 0.0;
	PreparationStartTime = 0.0;
	RequiredLoadingCompletedTime = -1.0;

	if (bWasLoading)
	{
		OnLoadingFinished.Broadcast(bSuccess);
	}
}

void ULCOLoadingSubsystem::HandleWorldCleanup(UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
	if (World && World == ActiveWorld.Get())
	{
		FinishLoading(false, TEXT("Prepared world destroyed"));
	}
}

FText ULCOLoadingSubsystem::SelectLoadingTipText() const
{
	static const FName TipKeys[] =
	{
		TEXT("LOADING_TIP_001"),
		TEXT("LOADING_TIP_002"),
		TEXT("LOADING_TIP_003"),
		TEXT("LOADING_TIP_004")
	};

	const FName SelectedTipKey = TipKeys[FMath::RandRange(0, UE_ARRAY_COUNT(TipKeys) - 1)];
	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOLocalizationSubsystem* LocalizationSubsystem = GameInstance
		? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>()
		: nullptr;

	return LocalizationSubsystem
		? LocalizationSubsystem->GetUIText(SelectedTipKey)
		: FText::FromName(SelectedTipKey);
}

void ULCOLoadingSubsystem::ShowLoadingScreen(const FText& TipText)
{
	FLoadingScreenAttributes Attributes;
	// UMG owns the single end-to-end minimum and readiness hold. MoviePlayer only
	// covers blocking map loading and must not impose a separate three-second wait.
	Attributes.MinimumLoadingScreenDisplayTime = 0.0f;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bWaitForManualStop = false;
	Attributes.bMoviesAreSkippable = false;
	Attributes.WidgetLoadingScreen = SNew(SLCOLoadingScreen).TipText(TipText).StartTime(LoadingStartTime);
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
}
