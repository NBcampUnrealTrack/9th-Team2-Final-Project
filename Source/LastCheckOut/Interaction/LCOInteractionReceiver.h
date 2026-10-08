#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LCOInteractionReceiver.generated.h"

class ULCOInteractComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class ULCOInteractionReceiver : public UInterface
{
	GENERATED_BODY()
};

// 상호작용을 전달받는 대상(퍼즐, 문, 전화기 등)이 구현하는 인터페이스
// ULCOInteractComponent가 상호작용되면 연결된 대상의 HandleInteraction()이 호출된다
class LASTCHECKOUT_API ILCOInteractionReceiver
{
	GENERATED_BODY()

public:
	// 상호작용 시 호출. 대상별 동작(퍼즐 입력, 문 열기, 전화 걸기 등)을 구현
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void HandleInteraction(ULCOInteractComponent* SourceComponent, AActor* Interactor);

	// 상호작용 잠금 여부. true면 상호작용 불가 (해결된 퍼즐, 잠긴 문 등)
	// 구현하지 않으면 false(잠기지 않음)로 동작
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool IsInteractionLocked(ULCOInteractComponent* SourceComponent, AActor* Interactor) const;

	// 현재 상태에 맞는 상호작용 문구 (예: "닫기", "잠겨 있다")
	// 빈 문구를 반환하면 컴포넌트의 기본 문구(InteractPrompt)를 사용. 구현하지 않으면 빈 문구로 동작
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(ULCOInteractComponent* SourceComponent, AActor* Interactor) const;
};
