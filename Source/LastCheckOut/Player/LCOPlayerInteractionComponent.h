#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOPlayerInteractionComponent.generated.h"

class ULCOInteractComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractFocusChanged, ULCOInteractComponent*, NewFocus);

// 플레이어 쪽 상호작용 처리
// 매 프레임 카메라 방향으로 라인트레이스해서 바라보는 ULCOInteractComponent를 찾고 (외곽선 표시),
// 상호작용 키(F) 입력 시 해당 컴포넌트의 Interact()를 호출한다
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOPlayerInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 바라보는 대상과 상호작용. 성공 시 true
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();
	
	UFUNCTION(BlueprintCallable, Category = "Interaction")
    	void RefreshFocus();

	// 현재 바라보는 대상 (없으면 nullptr)
	UFUNCTION(BlueprintPure, Category = "Interaction")
	ULCOInteractComponent* GetFocusedComponent() const { return FocusedComponent; }

	// 바라보는 대상이 바뀔 때 호출 (상호작용 UI 표시/숨김용, 대상이 없으면 nullptr)
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractFocusChanged OnFocusChanged;

protected:
	// 캐릭터 눈 위치 기준 상호작용 가능 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float InteractDistance = 200.f;

private:
	void UpdateFocus();
	void SetFocus(ULCOInteractComponent* NewFocus);
	ULCOInteractComponent* TraceForInteractable() const;

	UPROPERTY(Transient)
	TObjectPtr<ULCOInteractComponent> FocusedComponent;
};