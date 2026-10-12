#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOCameraCaptureComponent.generated.h"

class UTexture2D;
class ULCOCameraCapturePointComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnCameraPhotoCaptured,
	FName, CaptureID,
	UTexture2D*, PhotoImage,
	FText, PhotoTitle
);

UCLASS(ClassGroup=(LCO), meta=(BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOCameraCaptureComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOCameraCaptureComponent();

	// 촬영 버튼 입력 시 호출
	UFUNCTION(BlueprintCallable, Category="LCO|Camera")
	bool Capture();

	// 현재 조준 중인 촬영 대상 탐색
	UFUNCTION(BlueprintCallable, Category="LCO|Camera")
	ULCOCameraCapturePointComponent* FindCaptureTarget() const;

	// 사진 획득 이벤트
	UPROPERTY(BlueprintAssignable, Category="LCO|Camera")
	FOnCameraPhotoCaptured OnPhotoCaptured;

	// 카메라 필름 아이템 ID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LCO|Camera")
	FName CameraFilmItemID = TEXT("CameraFilm");

	// 촬영 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LCO|Camera",
		meta=(ClampMin="100.0"))
	float CaptureDistance = 500.0f;

protected:
	// 필름이 충분한지 확인
	bool HasCameraFilm() const;

	// 필름 1개 소모
	bool ConsumeFilm();

	// 촬영 대상에 실제 촬영 처리를 수행
	bool ProcessCapture(
		ULCOCameraCapturePointComponent* CapturePoint
	);
};