#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Shared by the movie screen, the travel guard and the UMG overlay. No asset dependency.
class SLCOLoadingScreen final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLCOLoadingScreen) {}
		SLATE_ARGUMENT(FText, TipText)
		SLATE_ARGUMENT(double, StartTime)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		TipText = InArgs._TipText;
		StartTime = InArgs._StartTime;
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
					.Text(LoadingStates[GetLoadingStateIndex()])
					.Font(FCoreStyle::GetDefaultFontStyle(FName(TEXT("Bold")), 25))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
			]
		];
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		if (LoadingTextBlock.IsValid())
		{
			// A common monotonic origin keeps the dots in phase across MoviePlayer and UMG.
			LoadingTextBlock->SetText(LoadingStates[GetLoadingStateIndex()]);
		}
	}

private:
	int32 GetLoadingStateIndex() const
	{
		const double Elapsed = FMath::Max(0.0, FPlatformTime::Seconds() - StartTime);
		return static_cast<int32>(FMath::Fmod(Elapsed / TextChangeInterval, 4.0));
	}

	static constexpr float TextChangeInterval = 0.4f;

	FText TipText;
	TArray<FText> LoadingStates;
	TSharedPtr<STextBlock> LoadingTextBlock;
	double StartTime = 0.0;
};
