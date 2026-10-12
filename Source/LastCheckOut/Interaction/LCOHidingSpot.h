#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "LCOHidingSpot.generated.h"

class UCameraComponent;
class ULCOHidingComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHidingSpotEnteredSignature, ALCOHidingSpot*, HidingSpot, AActor*, Hider);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHidingSpotExitedSignature, ALCOHidingSpot*, HidingSpot, AActor*, Hider, bool, bForced);

// 안에 숨을 수 있는 상호작용 액터 (옷장 등)
// 상호작용하면 Interactor의 ULCOHidingComponent로 숨기 / 나오기를 처리한다
// 숨기 상태(이동 차단, 충돌, 카메라)는 ULCOHidingComponent가 담당하고, 이 액터는 위치 / 문구 / 연출 이벤트를 제공한다
// 메시와 문 열림 연출은 BP 자식에서 구현한다
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOHidingSpot : public AActor, public ILCOInteractionReceiver
{
	GENERATED_BODY()

public:
	ALCOHidingSpot();

	// ILCOInteractionReceiver
	// 비어 있으면 숨고, 상호작용한 액터가 안에 숨어 있으면 나온다
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 다른 액터가 숨어 있거나, Interactor에 ULCOHidingComponent가 없으면 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 숨어 있는 본인에게는 "나가기", 그 외에는 "숨기"
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 숨어 있는 액터를 강제로 끌어낸다 (관리인 AI가 숨는 걸 봤을 때 등). 비어 있으면 무시
	UFUNCTION(BlueprintCallable, Category = "HidingSpot")
	void ForceEject();

	UFUNCTION(BlueprintPure, Category = "HidingSpot")
	bool IsOccupied() const { return HiddenActor.IsValid(); }

	// 현재 숨어 있는 액터 (없으면 nullptr)
	UFUNCTION(BlueprintPure, Category = "HidingSpot")
	AActor* GetHiddenActor() const { return HiddenActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "HidingSpot")
	FTransform GetHideTransform() const;

	UFUNCTION(BlueprintPure, Category = "HidingSpot")
	FTransform GetExitTransform() const;

	// 숨어 있는 동안 HidingCamera로 시점을 전환할지
	UFUNCTION(BlueprintPure, Category = "HidingSpot")
	bool UsesHidingCamera() const { return bUseHidingCamera; }

	UFUNCTION(BlueprintPure, Category = "HidingSpot")
	float GetCameraBlendTime() const { return CameraBlendTime; }

	// ULCOHidingComponent에서 숨기 / 나오기가 끝났을 때 호출 (직접 호출하지 말 것)
	void NotifyEntered(AActor* Hider);
	void NotifyExited(AActor* Hider, bool bForced);

	// 누군가 숨었을 때 (AI, 사운드 등 외부 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "HidingSpot")
	FOnHidingSpotEnteredSignature OnEnteredDelegate;

	// 숨어 있던 액터가 나왔을 때. bForced: ForceEject로 끌려 나왔는지
	UPROPERTY(BlueprintAssignable, Category = "HidingSpot")
	FOnHidingSpotExitedSignature OnExitedDelegate;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- BP 연출 이벤트 ----------

	// 숨었을 때 (문 여닫기 Timeline, 사운드, 화면 어둡게 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "HidingSpot")
	void OnHiderEntered(AActor* Hider);

	// 나왔을 때. bForced가 true면 끌려 나온 것
	UFUNCTION(BlueprintImplementableEvent, Category = "HidingSpot")
	void OnHiderExited(AActor* Hider, bool bForced);

	// ---------- 컴포넌트 ----------

	// 숨었을 때 액터가 위치할 곳 (옷장 안). 회전은 바라볼 방향
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HidingSpot")
	TObjectPtr<USceneComponent> HidePoint;

	// 나올 때 위치할 곳 (옷장 앞). 회전은 바라볼 방향
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HidingSpot")
	TObjectPtr<USceneComponent> ExitPoint;

	// 숨어 있는 동안 사용할 내부 카메라 (문 틈새 시점 등). bUseHidingCamera가 true일 때만 사용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "HidingSpot")
	TObjectPtr<UCameraComponent> HidingCamera;

	// ---------- 설정값 ----------

	// true면 숨어 있는 동안 HidingCamera로 시점 전환. false면 플레이어 카메라 유지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HidingSpot")
	bool bUseHidingCamera = false;

	// 카메라 전환 블렌드 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HidingSpot", meta = (ClampMin = "0.0", EditCondition = "bUseHidingCamera"))
	float CameraBlendTime = 0.3f;

	// ---------- 상호작용 문구 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HidingSpot|Prompt")
	FText HidePrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HidingSpot|Prompt")
	FText ExitPrompt;

private:
	ULCOHidingComponent* FindHidingComponent(const AActor* Actor) const;

	TWeakObjectPtr<AActor> HiddenActor;
};
