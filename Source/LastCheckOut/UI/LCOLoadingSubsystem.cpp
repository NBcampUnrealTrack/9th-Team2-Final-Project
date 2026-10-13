#include "UI/LCOLoadingSubsystem.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
class SLCOMovieLoadingScreen final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLCOMovieLoadingScreen) {}
		SLATE_ARGUMENT(FText, TipText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		TipText = InArgs._TipText;
		LoadingStates =
		{
			FText::FromString(TEXT("LOADING")),
			FText::FromString(TEXT("LOADING.")),
			FText::FromString(TEXT("LOADING..")),
			FText::FromString(TEXT("LOADING..."))
		};

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor(0.009f, 0.010f, 0.010f, 1.0f))
			.Padding(0.0f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(45.0f, 0.0f, 0.0f, 63.0f))
				[
					SNew(SBox)
					.WidthOverride(360.0f)
					[
						SNew(STextBlock)
						.Text(TipText)
						.Font(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Regular")), 16))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
						.AutoWrapText(true)
					]
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(FMargin(0.0f, 0.0f, 68.0f, 27.0f))
				[
					SAssignNew(LoadingTextBlock, STextBlock)
					.Text(LoadingStates[0])
					.Font(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), 25))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
			]
		];
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		ElapsedSinceTextChange += InDeltaTime;
		if (ElapsedSinceTextChange < TextChangeInterval || !LoadingTextBlock.IsValid())
		{
			return;
		}

		ElapsedSinceTextChange = FMath::Fmod(ElapsedSinceTextChange, TextChangeInterval);
		CurrentLoadingStateIndex = (CurrentLoadingStateIndex + 1) % LoadingStates.Num();
		LoadingTextBlock->SetText(LoadingStates[CurrentLoadingStateIndex]);
	}

private:
	static constexpr float TextChangeInterval = 0.4f;

	FText TipText;
	TArray<FText> LoadingStates;
	TSharedPtr<STextBlock> LoadingTextBlock;
	float ElapsedSinceTextChange = 0.0f;
	int32 CurrentLoadingStateIndex = 0;
};
}

void ULCOLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);
}

void ULCOLoadingSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Deinitialize();
}

void ULCOLoadingSubsystem::RequestLevelTravel(FName LevelName)
{
	if (LevelName.IsNone())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, LevelName);
	}
}

void ULCOLoadingSubsystem::HandlePreLoadMap(const FString& /*MapName*/)
{
	ShowLoadingScreen(SelectLoadingTipText());
}

void ULCOLoadingSubsystem::HandlePostLoadMapWithWorld(UWorld* /*LoadedWorld*/)
{
	// MoviePlayer closes automatically after loading and the minimum display time.
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
	FLoadingScreenAttributes LoadingScreenAttributes;
	LoadingScreenAttributes.MinimumLoadingScreenDisplayTime = MinimumLoadingScreenTime;
	LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes = true;
	LoadingScreenAttributes.bWaitForManualStop = false;
	LoadingScreenAttributes.bMoviesAreSkippable = false;
	LoadingScreenAttributes.WidgetLoadingScreen = SNew(SLCOMovieLoadingScreen)
		.TipText(TipText);

	GetMoviePlayer()->SetupLoadingScreen(LoadingScreenAttributes);
}
