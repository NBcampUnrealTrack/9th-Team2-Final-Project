#include "LCOCameraCapturePointComponent.h"

ULCOCameraCapturePointComponent::ULCOCameraCapturePointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 현재 대상이 촬영 가능한 상태인지 확인한다.
bool ULCOCameraCapturePointComponent::CanCapture() const
{
	// 이미 촬영한 대상은 다시 촬영할 수 없다.
	return !bHasBeenCaptured
		&& PhotoImage != nullptr;
}

// 촬영이 성공했을 때 대상의 촬영 완료 상태를 변경한다.
void ULCOCameraCapturePointComponent::MarkCaptured()
{
	// 이후 같은 대상을 다시 촬영해 사진을 중복 획득하는 것을 방지한다.
	bHasBeenCaptured = true;
}