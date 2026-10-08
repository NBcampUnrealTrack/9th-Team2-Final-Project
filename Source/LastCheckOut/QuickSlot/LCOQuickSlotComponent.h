#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOQuickSlotComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UInventoryComponent;
class ULCOQuickSlotWidget;
class APlayerController;
class AController;
class APawn;
struct FItemData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOQuickSlotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOQuickSlotItemUsed, int32, SlotIndex, FName, ItemID);

/**
 * 퀵슬롯(1~4) 컴포넌트
 * - BP_PlayerCharacter에 Add Component로 붙여서 사용 (캐릭터 / 컨트롤러 C++ 수정 없음)
 * - 슬롯 순서 등록: 인벤토리 UI에서 AssignSlot / SwapSlots / ClearSlot 호출
 * - Q 꾹 누르기: 원형 휠 표시 + 시간 느려짐 → 마우스 방향으로 선택 → 떼면 사용
 * - 평소엔 숨김, 달리는 동안만 HUD 표시 → 캐릭터의 달리기 상태 델리게이트에 HandleSprintStateChanged 바인딩
 * - 실제 아이템 효과는 OnQuickSlotItemUsed를 받는 쪽에서 처리 (소모품이면 거기서 RemoveItem 호출)
 *
 * 슬롯 인덱스: 0~3 (화면 표시 1~4)
 * 휠 방향:     위=1번, 오른쪽=2번, 아래=3번, 왼쪽=4번 (시계방향)
 */
UCLASS(ClassGroup = (LCO), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOQuickSlotComponent();

	static constexpr int32 NumSlots = 4;

	// ---------- 슬롯 등록 (인벤토리 UI에서 호출) ----------

	// 슬롯에 아이템 등록. 다른 슬롯에 같은 아이템이 있으면 그 슬롯은 비움. ItemID가 None이면 비우기
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	bool AssignSlot(int32 SlotIndex, FName ItemID);

	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void ClearSlot(int32 SlotIndex);

	// 두 슬롯 순서 교체
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void SwapSlots(int32 SlotA, int32 SlotB);

	// ---------- 조회 ----------

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	FName GetSlotItem(int32 SlotIndex) const;

	// 해당 아이템이 등록된 슬롯 인덱스 (없으면 -1)
	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	int32 FindSlotOfItem(FName ItemID) const;

	// 슬롯 아이템의 현재 보유 수량 (인벤토리 기준)
	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	int32 GetSlotItemCount(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	bool IsWheelOpen() const { return bWheelOpen; }

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	bool IsSprinting() const { return bIsSprinting; }

	UInventoryComponent* GetInventory() const;
	const FItemData* GetSlotItemData(int32 SlotIndex) const;

	// ---------- 사용 ----------

	// 보유 수량이 있을 때만 성공 → OnQuickSlotItemUsed 브로드캐스트
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	bool UseSlot(int32 SlotIndex);

	// ---------- 달리기 상태 ----------

	// 캐릭터의 달리기/걷기 상태 델리게이트(bool bIsSprinting)에 바인딩할 함수
	// 델리게이트가 생기기 전에는 BP에서 직접 호출해서 테스트 가능
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void HandleSprintStateChanged(bool bInIsSprinting);

	// HUD / 휠 위젯 다시 그리기 (인벤토리 수량이 바뀐 뒤 호출하면 즉시 반영)
	UFUNCTION(BlueprintCallable, Category = "LCO|QuickSlot")
	void RefreshWidgets();

	// ---------- 델리게이트 ----------

	// 슬롯 등록/교체/해제 시
	UPROPERTY(BlueprintAssignable, Category = "LCO|QuickSlot")
	FOnLCOQuickSlotsChanged OnQuickSlotsChanged;

	// 휠에서 아이템 사용 시 (실제 효과는 여기 바인딩해서 처리)
	UPROPERTY(BlueprintAssignable, Category = "LCO|QuickSlot")
	FOnLCOQuickSlotItemUsed OnQuickSlotItemUsed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- 입력 (BP 클래스 디폴트에서 지정) ----------

	// Q 키 액션 (Value Type: Digital(bool), Trigger는 비워둘 것 - 누르는 동안 매 프레임 Triggered 필요)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Input")
	TObjectPtr<UInputAction> QuickSlotAction;

	// 퀵슬롯 전용 IMC (Q 매핑). 비워두면 기존 IMC에 Q를 직접 추가해야 함
	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Input")
	TObjectPtr<UInputMappingContext> QuickSlotMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Input")
	int32 MappingContextPriority = 1;

	// ---------- 위젯 ----------

	// 달릴 때 보이는 1~4 HUD (WBP_QuickSlotHUD)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Widget")
	TSubclassOf<ULCOQuickSlotWidget> HUDWidgetClass;

	// Q 꾹 눌렀을 때 원형 휠 (WBP_QuickSlotWheel)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Widget")
	TSubclassOf<ULCOQuickSlotWidget> WheelWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Widget")
	int32 HUDZOrder = 1;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Widget")
	int32 WheelZOrder = 4;

	// ---------- 휠 설정 ----------

	// 휠이 열려 있는 동안 게임 속도 (1 = 정상)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Wheel", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float WheelTimeDilation = 0.3f;

	// 이 거리 이상 마우스를 움직여야 선택됨 (중앙 근처에서 떼면 취소)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Wheel", meta = (ClampMin = "0.0"))
	float SelectDeadZone = 8.f;

	// 가상 커서 최대 반경 (반대 방향으로 빨리 전환되도록 제한)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|QuickSlot|Wheel", meta = (ClampMin = "1.0"))
	float WheelCursorMaxRadius = 30.f;

	// 등록된 슬롯 아이템 (테스트용으로 디폴트 값 지정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LCO|QuickSlot")
	TArray<FName> SlotItemIDs;

private:
	// 컨트롤러 확보 → 입력 바인딩 + 위젯 생성
	void TrySetupForController(AController* NewController);

	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void CreateWidgets(APlayerController* PC);
	void RemoveWidgets();

	// Q 입력
	void HandleWheelStarted();
	void HandleWheelTriggered();
	void HandleWheelCompleted();
	void HandleWheelCanceled();

	void OpenWheel();
	void CloseWheel(bool bUseSelected);
	void UpdateHUDVisibility();

	bool IsValidSlotIndex(int32 SlotIndex) const { return SlotIndex >= 0 && SlotIndex < NumSlots; }
	void NotifySlotsChanged();

	UPROPERTY(Transient)
	TObjectPtr<ULCOQuickSlotWidget> HUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULCOQuickSlotWidget> WheelWidget;

	TWeakObjectPtr<APlayerController> CachedPC;

	bool bInputBound = false;
	bool bWheelOpen = false;
	bool bIsSprinting = false;

	// 휠이 열린 동안 누적한 마우스 이동량 (위 = +Y)
	FVector2D WheelCursor = FVector2D::ZeroVector;
	int32 SelectedIndex = INDEX_NONE;
};
