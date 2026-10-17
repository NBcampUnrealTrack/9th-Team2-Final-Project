#include "UI/LCOLoadingOverlayWidget.h"

#include "UI/SLCOLoadingScreen.h"

void ULCOLoadingOverlayWidget::InitializeLoadingScreen(const FText& InTipText, double InStartTime)
{
	TipText = InTipText;
	StartTime = InStartTime;
}

TSharedRef<SWidget> ULCOLoadingOverlayWidget::RebuildWidget()
{
	return SNew(SLCOLoadingScreen).TipText(TipText).StartTime(StartTime);
}
