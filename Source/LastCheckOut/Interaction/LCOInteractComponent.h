#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "LCOInteractComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteracted, ULCOInteractComponent*, Component, AActor*, Interactor);

// 공통 상호작용 컴포넌트 (퍼즐, 문, 전화기 등 모든 상호작용 대상에 사용)
// StaticMeshComponent를 상속하므로 메시, 크기(Scale), 재질을 디테일 패널에서 자유롭게 변경 가능
// 플레이어가 Interact()를 호출하면 연결된 대상(ILCOInteractionReceiver 구현 액터)에 상호작용을 전달한다
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOInteractComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	ULCOInteractComponent();

	// 플레이어에서 상호작용 키 입력 시 호출. 대상에 전달되면 true 반환
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool Interact(AActor* Interactor);

	// 현재 상호작용 가능한 상태인지 확인 (UI 표시 여부 판단에도 사용)
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	// 플레이어가 바라볼 때 / 시선을 뗄 때 호출 (외곽선 강조용)
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetFocused(bool bFocused);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	// 상호작용을 전달할 대상. LinkedTarget이 없으면 Owner 액터를 사용
	// 대상이 ILCOInteractionReceiver를 구현하지 않았다면 nullptr 반환
	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetInteractionTarget() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FName GetInputID() const { return InputID; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetInteractPrompt() const { return InteractPrompt; }

	// 상호작용 성공 시 호출 (사운드, 버튼 눌림 애니메이션 등 연출용)
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteracted OnInteracted;

protected:
	// 상호작용을 전달할 대상 (ILCOInteractionReceiver 구현 액터)
	// 레벨에 배치된 인스턴스의 디테일 패널에서 지정. 비워두면 Owner 액터에 전달
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> LinkedTarget;

	// 대상에서 어떤 입력인지 구분하기 위한 ID (예: 버튼 순서 퍼즐의 "Red", "Blue")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName InputID;

	// 바라볼 때 UI에 표시할 문구 (예: "조사하기", "문 열기", "전화 걸기")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractPrompt;

	// false면 상호작용 불가
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bInteractionEnabled = true;
};
