#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOHidingComponent.generated.h"

class ALCOHidingSpot;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHidingChangedSignature, bool, bIsHiding, ALCOHidingSpot*, HidingSpot);

// 숨을 수 있는 캐릭터에 붙이는 컴포넌트 (플레이어)
// ALCOHidingSpot과 상호작용하면 숨기 상태로 전환한다: 숨는 위치로 순간이동, 이동 / 충돌 / 메시 끄기, (선택) 카메라 전환
// 숨기 로직을 캐릭터 클래스에 직접 넣지 않기 위해 분리. AI는 IsHiding()으로 숨은 상태를 확인한다
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOHidingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOHidingComponent();

	// HidingSpot 안으로 숨는다. 이미 숨어 있거나 Owner가 Character가 아니면 false
	UFUNCTION(BlueprintCallable, Category = "Hiding")
	bool EnterHiding(ALCOHidingSpot* HidingSpot);

	// 숨은 곳에서 나온다. bForced: 끌려 나오는 경우 (AI 등). 숨어 있지 않으면 무시
	UFUNCTION(BlueprintCallable, Category = "Hiding")
	void ExitHiding(bool bForced = false);

	UFUNCTION(BlueprintPure, Category = "Hiding")
	bool IsHiding() const { return CurrentHidingSpot.IsValid(); }

	// 현재 숨어 있는 곳 (없으면 nullptr)
	UFUNCTION(BlueprintPure, Category = "Hiding")
	ALCOHidingSpot* GetHidingSpot() const { return CurrentHidingSpot.Get(); }

	// 숨거나 나올 때 호출 (UI, 입력 차단, AI 등 외부 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "Hiding")
	FOnHidingChangedSignature OnHidingChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<ALCOHidingSpot> CurrentHidingSpot;
};
