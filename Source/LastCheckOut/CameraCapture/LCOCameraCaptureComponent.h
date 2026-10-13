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
	Cooldown		UMETA(DisplayName = "촬영 대기 중"),
	AlreadyCaptured UMETA(DisplayName = "이미 촬영한 대상"),
	InvalidData     UMETA(DisplayName = "촬영 데이터 오류"),
	SystemError     UMETA(DisplayName = "시스템 오류"),

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnCameraCaptureFailed,
	ELCOCameraCaptureResult,
	Result
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
	
	// 사진 획득 실패
	UPROPERTY(BlueprintAssignable, Category = "LCO|Camera")
	FOnCameraCaptureFailed OnCaptureFailed;

	// 촬영 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LCO|Camera",
		meta=(ClampMin="100.0"))
	float CaptureDistance = 500.0f;
	
	// 촬영한 사진을 등록할 수첩 DataTable
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LCO|Camera")
	TObjectPtr<UDataTable> NotebookTable = nullptr;
	
	// ---------------------------------------------------------------------------------------------
	
	// 촬영 성공 후 다음 촬영까지 대기 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LCO|Camera",
		meta=(ClampMin="0.0"))
	float CaptureCooldown = 5.0f;
	
	// 다음 촬영이 가능한 월드 시간
	float NextCaptureAllowedTime = 0.0f;

protected:

	ELCOCameraCaptureResult ProcessCapture(
		ULCOCameraCapturePointComponent* CapturePoint
	);
};