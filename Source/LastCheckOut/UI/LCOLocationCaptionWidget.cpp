#include "UI/LCOLocationCaptionWidget.h"
#include "Components/TextBlock.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void ULCOLocationCaptionWidget::StartCaption(const TArray<FLCOLocationCaptionLine>& InLines, int32 InClockStartSeconds)
{
	ClearTimers();

	const ULCOLocalizationSubsystem* Localization = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr;

	LineStrings.Reset();
	ClockLineIndex = INDEX_NONE;
	for (int32 Index = 0; Index < InLines.Num(); ++Index)
	{
		const FLCOLocationCaptionLine& Line = InLines[Index];
		LineStrings.Add(Localization ? Localization->GetUIText(Line.TextKey).ToString() : Line.TextKey.ToString());
		if (Line.bShowClock && ClockLineIndex == INDEX_NONE)
		{
			ClockLineIndex = Index;
		}
	}

	// 모든 줄 비우기
	for (int32 Index = 0; Index < 5; ++Index)
	{
		if (UTextBlock* Text = GetLineText(Index))
		{
			Text->SetText(FText::GetEmpty());
		}
	}

	ClockSeconds = FMath::Max(0, InClockStartSeconds);
	bClockRunning = false;
	CurrentLine = 0;
	CurrentChar = 0;
	bFadingOut = false;
	FadeAlpha = 1.f;
	SetRenderOpacity(1.f);
	LastTypingSoundTime = -1.0;

	// Log Test
	UE_LOG(LogTemp, Warning, TEXT("[LCOLocationCaption] StartCaption: %d lines"), LineStrings.Num());

	ResumeTyping();
}

void ULCOLocationCaptionWidget::ResumeTyping()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TypeTimerHandle, this, &ThisClass::TypeNextChar, CharInterval, true);
	}
}

void ULCOLocationCaptionWidget::TypeNextChar()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 모든 줄 끝 → 유지 후 사라짐
	if (CurrentLine >= LineStrings.Num())
	{
		World->GetTimerManager().ClearTimer(TypeTimerHandle);
		World->GetTimerManager().SetTimer(HoldTimerHandle, this, &ThisClass::BeginFadeOut, 
			FMath::Max(HoldTime, 0.01f), false);
		return;
	}

	const FString FullLine = GetFullLineString(CurrentLine);
	UTextBlock* Text = GetLineText(CurrentLine);

	// 표시할 텍스트 블록이 없거나 빈 줄이면 건너뜀
	if (!Text || FullLine.IsEmpty())
	{
		++CurrentLine;
		CurrentChar = 0;
		return;
	}

	++CurrentChar;
	Text->SetText(FText::FromString(FullLine.Left(CurrentChar)));
	PlayTypingSound(FullLine[CurrentChar - 1]);

	if (CurrentChar < FullLine.Len())
	{
		return;
	}

	// 한 줄 끝: 시계 줄이면 시계 시작
	if (CurrentLine == ClockLineIndex && !bClockRunning)
	{
		bClockRunning = true;
		World->GetTimerManager().SetTimer(ClockTimerHandle, this, &ThisClass::TickClock, 1.f, true);
	}

	++CurrentLine;
	CurrentChar = 0;

	// 다음 줄까지 잠시 쉼
	if (LineInterval > 0.f)
	{
		World->GetTimerManager().ClearTimer(TypeTimerHandle);
		World->GetTimerManager().SetTimer(TypeTimerHandle, this, &ThisClass::ResumeTyping, LineInterval, false);
	}
}

void ULCOLocationCaptionWidget::PlayTypingSound(TCHAR Character)
{
	if (!TypingSound || FChar::IsWhitespace(Character))
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetRealTimeSeconds();
	if (LastTypingSoundTime >= 0.0 && Now - LastTypingSoundTime < TypingSoundInterval)
	{
		return;
	}
	LastTypingSoundTime = Now;

	const float Pitch = FMath::FRandRange(FMath::Min(TypingPitchMin, TypingPitchMax), FMath::Max(TypingPitchMin, TypingPitchMax));
	UGameplayStatics::PlaySound2D(this, TypingSound, TypingVolume, Pitch);
}

void ULCOLocationCaptionWidget::TickClock()
{
	ClockSeconds = (ClockSeconds + 1) % 86400;
	if (UTextBlock* Text = GetLineText(ClockLineIndex))
	{
		Text->SetText(FText::FromString(GetFullLineString(ClockLineIndex)));
	}
}

void ULCOLocationCaptionWidget::BeginFadeOut()
{
	bFadingOut = true;
}

void ULCOLocationCaptionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bFadingOut)
	{
		return;
	}

	FadeAlpha -= InDeltaTime / FadeOutTime;
	SetRenderOpacity(FMath::Max(FadeAlpha, 0.f));

	if (FadeAlpha <= 0.f)
	{
		bFadingOut = false;
		ClearTimers();

		// Log Test
		UE_LOG(LogTemp, Warning, TEXT("[LCOLocationCaption] Caption finished"));
		OnCaptionFinished.Broadcast();
	}
}

void ULCOLocationCaptionWidget::NativeDestruct()
{
	ClearTimers();
	Super::NativeDestruct();
}

void ULCOLocationCaptionWidget::ClearTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TypeTimerHandle);
		World->GetTimerManager().ClearTimer(ClockTimerHandle);
		World->GetTimerManager().ClearTimer(HoldTimerHandle);
	}
}

UTextBlock* ULCOLocationCaptionWidget::GetLineText(int32 Index) const
{
	switch (Index)
	{
	case 0: return Txt_Line1;
	case 1: return Txt_Line2;
	case 2: return Txt_Line3;
	case 3: return Txt_Line4;
	case 4: return Txt_Line5;
	default: return nullptr;
	}
}

FString ULCOLocationCaptionWidget::GetFullLineString(int32 Index) const
{
	if (!LineStrings.IsValidIndex(Index))
	{
		return FString();
	}

	return Index == ClockLineIndex ? LineStrings[Index] + TEXT(" - ") + GetClockString() : LineStrings[Index];
}

FString ULCOLocationCaptionWidget::GetClockString() const
{
	return FString::Printf(TEXT("%02d:%02d:%02d"), ClockSeconds / 3600, (ClockSeconds / 60) % 60, ClockSeconds % 60);
}
