#include "UI/LCOMonologueWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"

void ULCOMonologueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 화면을 덮어도 클릭 / 입력을 막지 않음
	SetVisibility(ESlateVisibility::HitTestInvisible);
	HideImmediately();
}

void ULCOMonologueWidget::EnqueueMonologue(const FText& Text, float Duration)
{
	if (Text.IsEmpty())
	{
		return;
	}

	FLCOMonologueRequest Request;
	Request.Text = Text;
	Request.Duration = Duration;
	Queue.Add(Request);

	while (Queue.Num() > MaxQueueSize)
	{
		Queue.RemoveAt(0);
	}

	if (Phase == EPhase::Idle)
	{
		StartNext();
	}
	else if (Phase == EPhase::Hold)
	{
		// 다음 독백이 기다리고 있으면 현재 독백을 너무 오래 붙잡지 않음
		HoldDuration = FMath::Min(HoldDuration, FMath::Max(PhaseTime, MinDuration * 0.6f));
	}
}

void ULCOMonologueWidget::ClearAll()
{
	Queue.Reset();
	HideImmediately();
}

bool ULCOMonologueWidget::IsShowingOrQueued(const FText& Text) const
{
	const FString Str = Text.ToString();
	if (Phase != EPhase::Idle && Phase != EPhase::FadeOut && Current.Text.ToString() == Str)
	{
		return true;
	}
	return Queue.ContainsByPredicate([&Str](const FLCOMonologueRequest& R) { return R.Text.ToString() == Str; });
}

void ULCOMonologueWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Phase == EPhase::Idle)
	{
		return;
	}

	PhaseTime += InDeltaTime;

	switch (Phase)
	{
	case EPhase::FadeIn:
		SetFadeOpacity(FadeInTime > 0.0f ? FMath::Clamp(PhaseTime / FadeInTime, 0.0f, 1.0f) : 1.0f);
		if (PhaseTime >= FadeInTime)
		{
			Phase = EPhase::Hold;
			PhaseTime = 0.0f;
			SetFadeOpacity(1.0f);
		}
		break;

	case EPhase::Hold:
		if (PhaseTime >= HoldDuration)
		{
			Phase = EPhase::FadeOut;
			PhaseTime = 0.0f;
		}
		break;

	case EPhase::FadeOut:
		SetFadeOpacity(FadeOutTime > 0.0f ? 1.0f - FMath::Clamp(PhaseTime / FadeOutTime, 0.0f, 1.0f) : 0.0f);
		if (PhaseTime >= FadeOutTime)
		{
			HideImmediately();
			OnMonologueHidden();
			StartNext();
		}
		break;

	default:
		break;
	}
}

void ULCOMonologueWidget::StartNext()
{
	if (Queue.Num() == 0)
	{
		return;
	}

	Current = Queue[0];
	Queue.RemoveAt(0);

	if (Txt_Monologue)
	{
		Txt_Monologue->SetText(Current.Text);
	}

	UWidget* Target = MonologuePanel ? MonologuePanel.Get() : static_cast<UWidget*>(Txt_Monologue.Get());
	if (Target)
	{
		Target->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	HoldDuration = CalcDuration(Current);
	Phase = EPhase::FadeIn;
	PhaseTime = 0.0f;
	SetFadeOpacity(0.0f);

	OnMonologueShown(Current.Text);
}

void ULCOMonologueWidget::HideImmediately()
{
	Phase = EPhase::Idle;
	PhaseTime = 0.0f;
	SetFadeOpacity(0.0f);

	UWidget* Target = MonologuePanel ? MonologuePanel.Get() : static_cast<UWidget*>(Txt_Monologue.Get());
	if (Target)
	{
		Target->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ULCOMonologueWidget::SetFadeOpacity(float Opacity)
{
	UWidget* Target = MonologuePanel ? MonologuePanel.Get() : static_cast<UWidget*>(Txt_Monologue.Get());
	if (Target)
	{
		Target->SetRenderOpacity(Opacity);
	}
}

float ULCOMonologueWidget::CalcDuration(const FLCOMonologueRequest& Request) const
{
	if (Request.Duration > 0.0f)
	{
		return Request.Duration;
	}

	const float Auto = BaseDuration + Request.Text.ToString().Len() * PerCharacterDuration;
	return FMath::Clamp(Auto, MinDuration, FMath::Max(MinDuration, MaxDuration));
}
