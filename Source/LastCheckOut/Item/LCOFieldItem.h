#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "Save/LCOSaveableActor.h"
#include "LCOFieldItem.generated.h"

class ULCOInteractComponent;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnFieldItemPickedUpSignature, ALCOFieldItem*, FieldItem, AActor*, Interactor, FName, ItemID, int32, Quantity);

// 필드에 오브젝트 형태로 놓인 아이템 (증거품, 소비 아이템, 열쇠 등 공통)
// 캐비닛(ALCOContainer)과 같은 상호작용 컴포넌트를 사용하므로
// 플레이어 레이캐스트 감지, 외곽선, 상호작용 안내 UI가 별도 작업 없이 그대로 동작한다
// 획득하면 숨김 + 충돌 해제 상태가 되고, 맵 이동 / 세이브 후에도 다시 나타나지 않는다
// (ALCOOpenableBase와 같은 ILCOSaveableActor → LCOWorldStateSubsystem 경로로 저장·복원)
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOFieldItem : public AActor, public ILCOInteractionReceiver, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOFieldItem();

	// ILCOInteractionReceiver
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 아직 줍지 않았으면 Items에 (ItemID, Quantity)를 남기고, 주웠으면 비워서 저장 (캐비닛의 "남은 아이템"과 같은 의미)
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	UFUNCTION(BlueprintPure, Category = "FieldItem")
	bool IsPickedUp() const { return bPickedUp; }

	UFUNCTION(BlueprintPure, Category = "FieldItem")
	FName GetItemID() const { return ItemID; }

	// 아이템 DataTable의 ItemName (DataTable에 없으면 ItemID 그대로)
	UFUNCTION(BlueprintPure, Category = "FieldItem")
	FText GetItemDisplayName() const;

	// 에디터 드롭다운용. 아이템 DataTable의 Row 이름(아이템 ID) 목록 반환
	UFUNCTION()
	TArray<FName> GetItemIDOptions() const;

	// 아이템을 획득했을 때 호출 (UI 알림, 퀘스트 갱신 등 외부 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "FieldItem")
	FOnFieldItemPickedUpSignature OnPickedUpDelegate;

protected:
	// 인벤토리에 추가하고 획득 상태로 전환. 실패하면 필드에 그대로 남는다
	bool TryPickUp(AActor* Interactor);

	// 숨김 + 충돌 해제 + 상호작용 비활성화 (Destroy하지 않는 이유: 세이브 시스템이 상태를 계속 읽을 수 있게)
	void ApplyPickedUpState();

	// 획득 이벤트 (사운드, 연출 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "FieldItem")
	void OnPickedUp(AActor* Interactor);

	// 메시 + 충돌 + 외곽선 + 상호작용을 모두 담당 (StaticMeshComponent 상속)
	// BP나 레벨 인스턴스에서 Static Mesh만 바꿔 끼우면 된다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FieldItem")
	TObjectPtr<ULCOInteractComponent> InteractComponent;

	// 획득할 아이템 ID. 아이템 DataTable의 Row 목록에서 선택
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieldItem", meta = (GetOptions = "GetItemIDOptions"))
	FName ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieldItem", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	// 상호작용 안내 문구. {ItemName}이 아이템 이름으로 바뀐다. 비우면 InteractComponent의 기본 문구 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FieldItem")
	FText PickupPromptFormat;

	// 아이템 DataTable (기본값: DT_ItemData, ALCOContainer와 동일)
	UPROPERTY(EditDefaultsOnly, Category = "FieldItem")
	TSoftObjectPtr<UDataTable> ItemDataTable;

	// 획득 여부 (세이브로 복원)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FieldItem")
	bool bPickedUp = false;
};
