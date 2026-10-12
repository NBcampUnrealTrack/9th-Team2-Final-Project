#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOCameraCapturePointComponent.generated.h"

class UTexture2D;

UCLASS(ClassGroup=(LCO), meta=(BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOCameraCapturePointComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOCameraCapturePointComponent();

	// 촬영 지점 식별자
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LCO|Camera")
	FName CaptureID = NAME_None;

	// 촬영 성공 시 표시할 미리 준비된 사진
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LCO|Camera")
	TObjectPtr<UTexture2D> PhotoImage = nullptr;

	// 사진 제목
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LCO|Camera")
	FText PhotoTitle;

	// 사진 설명 또는 추리 단서
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LCO|Camera")
	FText PhotoDescription;

	// 이미 촬영했는지 여부
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="LCO|Camera")
	bool bHasBeenCaptured = false;

	// 촬영 가능한 상태인지 확인
	UFUNCTION(BlueprintPure, Category="LCO|Camera")
	bool CanCapture() const;

	// 촬영 완료 처리
	UFUNCTION(BlueprintCallable, Category="LCO|Camera")
	void MarkCaptured();
};