#include "UI/LCOPhoneOverlayWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

TSharedRef<SWidget> ULCOPhoneOverlayWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrushColor(FLinearColor::Black);
		WidgetTree->RootWidget = Background;
	}
	return Super::RebuildWidget();
}

void ULCOPhoneOverlayWidget::Configure(const FText& Title, UTexture2D* Texture, bool bTutorial, float HoldSeconds)
{
	TakeWidget();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!bTutorial) return;
	Background->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.97f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	USizeBox* Card = WidgetTree->ConstructWidget<USizeBox>();
	Card->SetWidthOverride(800.f);
	Background->SetContent(Card);
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Card->SetContent(Content);
	UTextBlock* Heading = WidgetTree->ConstructWidget<UTextBlock>();
	Heading->SetText(Title);
	Content->AddChildToVerticalBox(Heading);
	if (Texture)
	{
		USizeBox* ImageSize = WidgetTree->ConstructWidget<USizeBox>();
		ImageSize->SetHeightOverride(450.f);
		UImage* Image = WidgetTree->ConstructWidget<UImage>();
		Image->SetBrushFromTexture(Texture);
		ImageSize->SetContent(Image);
		Content->AddChildToVerticalBox(ImageSize);
	}
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
	Hint->SetText(FText::Format(NSLOCTEXT("LCOPhoneTutorial", "HoldToClose", "F키를 {0}초 동안 누르면 닫힙니다."), FText::AsNumber(HoldSeconds)));
	Content->AddChildToVerticalBox(Hint);
}

void ULCOPhoneOverlayWidget::SetFadeOpacity(float Opacity)
{
	SetRenderOpacity(Opacity);
}
