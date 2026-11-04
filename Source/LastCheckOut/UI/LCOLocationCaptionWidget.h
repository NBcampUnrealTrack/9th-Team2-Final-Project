#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCOLocationCaptionWidget.generated.h"

class UTextBlock;
class USoundBase;

// 자막 한 줄 (UI.csv 키)
USTRUCT(BlueprintType)
struct FLCOLocationCaptionLine
{
	GENERATED_BODY()

	// UI.csv 키
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption")
	FName TextKey;

	// 켜면 이 줄 뒤에 " - HH:MM:SS" 시계가 붙고 1초마다 올라감
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption")
	bool bShowClock = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOLocationCaptionFinished);

// 위치 자막 (장소 / 시간 / 인물을 한 줄씩 타이핑 → 유지 → 사라짐)
// - WBP에서 부모 클래스로 지정하고 Txt_Line1 ~ Txt_Line5 텍스트 블록을 배치 (줄마다 스타일 자유)
// - 줄 순서대로 Txt_Line1, Txt_Line2 ... 에 표시
UCLASS(Abstract)
class LASTCHECKOUT_API ULCOLocationCaptionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 자막 시작. ClockStartSeconds = 시계 시작 시각 (하루 기준 초, 예: 15:20:00 = 55200)
	UFUNCTION(BlueprintCallable, Category = "LCO|Caption")
	void StartCaption(const TArray<FLCOLocationCaptionLine>& InLines, int32 InClockStartSeconds);

	// 사라짐까지 끝났을 때
	UPROPERTY(BlueprintAssignable, Category = "LCO|Caption")
	FOnLCOLocationCaptionFinished OnCaptionFinished;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// 글자 하나가 나타나는 간격
	UPROPERTY(EditAnywhere, Category = "LCO|Caption", meta = (ClampMin = "0.01", Units = "s"))
	float CharInterval = 0.05f;

	// 한 줄이 끝나고 다음 줄까지 간격
	UPROPERTY(EditAnywhere, Category = "LCO|Caption", meta = (ClampMin = "0.0", Units = "s"))
	float LineInterval = 0.4f;

	// 다 나온 뒤 유지 시간
	UPROPERTY(EditAnywhere, Category = "LCO|Caption", meta = (ClampMin = "0.0", Units = "s"))
	float HoldTime = 2.5f;

	// 사라지는 시간
	UPROPERTY(EditAnywhere, Category = "LCO|Caption", meta = (ClampMin = "0.01", Units = "s"))
	float FadeOutTime = 1.f;

	// 글자가 나올 때 타자 소리 (비우면 소리 없음, 공백 제외)
	UPROPERTY(EditAnywhere, Category = "LCO|Caption|Sound")
	TObjectPtr<USoundBase> TypingSound;

	UPROPERTY(EditAnywhere, Category = "LCO|Caption|Sound", meta = (ClampMin = "0.0"))
	float TypingVolume = 1.f;

	UPROPERTY(EditAnywhere, Category = "LCO|Caption|Sound", meta = (ClampMin = "0.1"))
	float TypingPitchMin = 0.95f;

	UPROPERTY(EditAnywhere, Category = "LCO|Caption|Sound", meta = (ClampMin = "0.1"))
	float TypingPitchMax = 1.05f;

	// 이 간격보다 빨리 재생하지 않음 (소리 겹침 방지)
	UPROPERTY(EditAnywhere, Category = "LCO|Caption|Sound", meta = (ClampMin = "0.0", Units = "s"))
	float TypingSoundInterval = 0.05f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Line1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Line2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Line3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Line4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Line5;

private:
	UTextBlock* GetLineText(int32 Index) const;
	FString GetFullLineString(int32 Index) const;
	FString GetClockString() const;

	void TypeNextChar();
	void ResumeTyping();
	void TickClock();
	void BeginFadeOut();
	void ClearTimers();
	void PlayTypingSound(TCHAR Character);

	double LastTypingSoundTime = -1.0;

	// 줄별 문장 (시계 줄은 시계 제외한 앞부분)
	TArray<FString> LineStrings;
	int32 ClockLineIndex = INDEX_NONE;
	int32 ClockSeconds = 0;
	bool bClockRunning = false;

	int32 CurrentLine = 0;
	int32 CurrentChar = 0;

	bool bFadingOut = false;
	float FadeAlpha = 1.f;

	FTimerHandle TypeTimerHandle;
	FTimerHandle ClockTimerHandle;
	FTimerHandle HoldTimerHandle;
};
