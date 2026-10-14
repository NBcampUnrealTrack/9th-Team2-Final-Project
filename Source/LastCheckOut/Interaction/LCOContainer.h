#pragma once

#include "CoreMinimal.h"
#include "Interaction/LCOOpenableBase.h"
#include "LCOContainer.generated.h"

class UDataTable;

// 창고에 보관되는 아이템 항목
USTRUCT(BlueprintType)
struct FLCOContainerItem
{
	GENERATED_BODY()

	// 아이템 ID. 에디터에서 아이템 DataTable의 Row 목록 중 선택 (ALCOContainer::GetItemIDOptions)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (GetOptions = "GetItemIDOptions"))
	FName ItemID;

	// 보관 수량
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContainerOpenedSignature, ALCOContainer*, Container, AActor*, Interactor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnContainerItemTakenSignature, ALCOContainer*, Container, AActor*, Interactor, FName, ItemID, int32, Quantity);

// 아이템을 보관하는 상호작용 액터 (서랍장, 사물함, 상자 등)
// 상호작용하면 열리면서 창고 열기 이벤트를 호출하고, 아이템 획득은 UI에서 TakeItem()을 호출해 처리한다
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOContainer : public ALCOOpenableBase
{
	GENERATED_BODY()

public:
	ALCOContainer();

	// ILCOInteractionReceiver
	// bBlockInteractionWhileOpen이 true면 열려 있는 동안(UI 사용 중) 상호작용 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 창고는 상호작용으로 닫지 않으므로, 열려 있을 때도 열기 문구(UI 다시 열기) 사용
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 열림 / 잠금(부모) + 남은 아이템 저장
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;

	// 남은 아이템으로 교체. 창고의 열림은 UI 사용 중 상태라 항상 닫힌 상태로 복원
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	// UI에서 아이템을 클릭 / 드래그했을 때 호출. 보관 목록에서 빼서 Interactor의 인벤토리에 추가
	// 보관 수량이 부족하거나, 인벤토리가 없거나, 아이템 DataTable에 없는 ID면 false 반환
	UFUNCTION(BlueprintCallable, Category = "Container")
	bool TakeItem(AActor* Interactor, FName ItemID, int32 Quantity = 1);

	// 현재 보관 중인 아이템 목록 (UI 표시용)
	UFUNCTION(BlueprintPure, Category = "Container")
	const TArray<FLCOContainerItem>& GetContainedItems() const { return ContainedItems; }

	UFUNCTION(BlueprintPure, Category = "Container")
	bool IsEmpty() const { return ContainedItems.IsEmpty(); }

	// 에디터 드롭다운용. 아이템 DataTable의 Row 이름(아이템 ID) 목록 반환
	UFUNCTION()
	TArray<FName> GetItemIDOptions() const;

	// 창고가 열려 UI를 띄워야 할 때 호출 (UI 담당 / 외부 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "Container")
	FOnContainerOpenedSignature OnContainerOpenedDelegate;

	// 아이템을 획득했을 때 호출 (UI 목록 갱신 등 외부 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "Container")
	FOnContainerItemTakenSignature OnItemTakenDelegate;

protected:
	// 닫혀 있으면 열고, 열려 있으면 다시 창고 열기 이벤트만 호출 (닫기는 UI를 닫을 때 Close() 호출)
	virtual void HandleUnlockedInteraction(AActor* Interactor) override;

	// ---------- BP 이벤트 ----------

	// 창고 열기 이벤트. 여기서 아이템 획득 UI를 띄운다
	UFUNCTION(BlueprintImplementableEvent, Category = "Container")
	void OnContainerOpened(AActor* Interactor);

	// 아이템 획득 이벤트 (획득 사운드, 연출 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Container")
	void OnItemTaken(AActor* Interactor, FName ItemID, int32 Quantity);

	// ---------- 설정값 ----------

	// 보관 중인 아이템 목록. BP 기본값 / 레벨 인스턴스에서 아이템 ID(드롭다운)와 수량 입력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container")
	TArray<FLCOContainerItem> ContainedItems;

	// 열려 있는 동안 상호작용을 막을지 (UI를 닫을 때 Close()를 호출하면 다시 상호작용 가능)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container")
	bool bBlockInteractionWhileOpen = true;

	// 아이템 ID 드롭다운에 사용할 아이템 DataTable (기본값: DT_ItemData)
	UPROPERTY(EditDefaultsOnly, Category = "Container")
	TSoftObjectPtr<UDataTable> ItemDataTable;
};
