#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOCameraCaptureComponent.generated.h"

class UTexture2D;
class ULCOCameraCapturePointComponent;
class UDataTable;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnCameraPhotoCaptured,
	FName, CaptureID,
	UTexture2D*, PhotoImage,
	FText, PhotoTitle,
	FText, PhotoDescription
);

UENUM(BlueprintType)
enum class ELCOCameraCaptureResult : uint8
{
	Success         UMETA(DisplayName = "촬영 성공"),
	NoTarget        UMETA(DisplayName = "촬영 대상 없음"),
	AlreadyCaptured UMETA(DisplayName = "이미 촬영한 대상"),
	NoFilm          UMETA(DisplayName = "필름 부족"),
	InvalidData     UMETA(DisplayName = "촬영 데이터 오류"),
	SystemError     UMETA(DisplayName = "시스템 오류")
};


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
	
	// 촬영한 사진을 등록할 수첩 DataTable
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LCO|Camera")
	TObjectPtr<UDataTable> NotebookTable = nullptr;

protected:
	// 필름이 충분한지 확인
	bool HasCameraFilm() const;

	// 필름 1개 소모
	bool ConsumeFilm();

	ELCOCameraCaptureResult ProcessCapture(
		ULCOCameraCapturePointComponent* CapturePoint
	);
};