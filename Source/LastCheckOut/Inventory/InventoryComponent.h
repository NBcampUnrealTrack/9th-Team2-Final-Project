#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventorySlot.h"
#include "Engine/DataTable.h"
#include "Item/ItemData.h"
#include "InventoryComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LASTCHECKOUT_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

protected:
	virtual void BeginPlay() override;

public:

	// 플레이어가 가지고 있는 아이템 슬롯 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FInventorySlot> InventorySlots;
	
	// ------------------------------------------------------------------------------------------------------
	
	// 아이템을 인벤토리에 추가
	UFUNCTION(BlueprintCallable)
	void AddItem(FName ItemID, int32 Quantity);

	// 특정 아이템을 가지고 있는지 확인
	UFUNCTION(BlueprintCallable)
	bool HasItem(FName ItemID) const;

	// 특정 아이템의 현재 보유 개수 반환
	UFUNCTION(BlueprintPure)
	int32 GetItemCount(FName ItemID) const;

	// 특정 아이템을 인벤토리에서 제거
	UFUNCTION(BlueprintCallable)
	bool RemoveItem(FName ItemID, int32 Quantity);
	
	// ---------------------------------------------------------------------------------------------------------
	
	// 아이템 정보를 저장하고 있는 DataTable
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDataTable> ItemDataTable;
	
	// ItemID를 이용해 DataTable에서 아이템 정보를 가져옴
	const FItemData* GetItemData(FName ItemID) const;
};