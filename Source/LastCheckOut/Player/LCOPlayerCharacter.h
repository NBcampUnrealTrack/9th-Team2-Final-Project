#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LCOPlayerCharacter.generated.h"


class USuspicionComponent;
class UNotebookComponent;
class ULCOQuickSlotComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInventoryComponent;
class ULCOPlayerInteractionComponent;
class ULCOStaminaComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOMoveStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOSprintStateChanged, bool, bIsSprinting);

UENUM(BlueprintType)
enum class ELCOViewMode : uint8
{
	FirstPerson  UMETA(DisplayName = "First Person"),
	ThirdPerson  UMETA(DisplayName = "Third Person")
};

// 플레이어 캐릭터
// 이동(WASD), 시점(마우스), 달리기(Shift, 스태미나 소모), 상호작용(F), 1인칭/3인칭 전환(V) 담당
// 메뉴 입력(아이템, 수첩, 옵션, 퀵슬롯)은 ALCOPlayerController에서 처리
UCLASS()
class LASTCHECKOUT_API ALCOPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALCOPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 시점 변경 (컷신, 특정 구역에서 강제 전환할 때도 사용)
	UFUNCTION(BlueprintCallable, Category = "LCO|Camera")
	void SetViewMode(ELCOViewMode NewMode);

	UFUNCTION(BlueprintPure, Category = "LCO|Camera")
	ELCOViewMode GetViewMode() const { return ViewMode; }

	UFUNCTION(BlueprintPure, Category = "LCO|Inventory")
	UInventoryComponent* GetInventory() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Interaction")
	ULCOPlayerInteractionComponent* GetInteraction() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	ULCOStaminaComponent* GetStamina() const { return StaminaComponent; }
	
	UFUNCTION(BlueprintPure, Category = "LCO|Suspicion")
	USuspicionComponent* GetSuspicion() const { return SuspicionComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	UNotebookComponent* GetNotebook() const { return NotebookComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	ULCOQuickSlotComponent* GetQuickSlot() const { return QuickSlotComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Movement")
	bool IsSprinting() const { return bIsSprinting; }
	
	// 현재 스태미나 직접 설정
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void SetStamina(int32 NewStamina);

	// 회복 (캔커피 등)
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void RestoreStamina(int32 Amount);

	// 소모
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void ConsumeStamina(int32 Amount);

	// 최대 스태미나 변경
	UFUNCTION(BlueprintCallable, Category = "LCO|Stamina")
	void SetMaxStamina(int32 NewMaxStamina, bool bFillToMax = false);

	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	int32 GetCurrentStamina() const;

	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	int32 GetMaxStamina() const;

	// 0.0 ~ 1.0 (프로그레스바용)
	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	float GetStaminaPercent() const;

	// 0 ~ 100 (텍스트 "75%" 표시용)
	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	int32 GetStaminaPercentInt() const;
	
	// 일정 시간 이동속도 증가 (걷기 / 달리기 모두 +Amount)
	// 이미 적용 중이면 수치와 시간을 새로 덮어씀 (중첩 X). Amount, Duration이 0 이하면 실패
	UFUNCTION(BlueprintCallable, Category = "LCO|Buff")
	bool ApplyMoveSpeedBuff(float Amount, float Duration);

	// 일정 시간 최대 스태미나 증가 (+Amount, 늘어난 만큼 즉시 회복)
	// 이미 적용 중이면 수치와 시간을 새로 덮어씀 (중첩 X). Amount, Duration이 0 이하면 실패
	UFUNCTION(BlueprintCallable, Category = "LCO|Buff")
	bool ApplyMaxStaminaBuff(int32 Amount, float Duration);

	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	bool IsMoveSpeedBuffActive() const;

	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	bool IsMaxStaminaBuffActive() const;

	// 남은 시간(초). 버프가 없으면 0 (UI 표시용)
	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	float GetMoveSpeedBuffRemainingTime() const;

	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	float GetMaxStaminaBuffRemainingTime() const;

	// 걷기 → 달리기로 바뀔 때 (발소리, 숨소리, UI 등)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Movement")
	FOnLCOMoveStateChanged OnSprintStarted;

	// 달리기 → 걷기로 바뀔 때 (Shift 뗌, 스태미나 소진, 멈춤)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Movement")
	FOnLCOMoveStateChanged OnWalkStarted;
	
	UPROPERTY(BlueprintAssignable, Category = "LCO|Movement")
	FOnLCOSprintStateChanged OnSprintStateChanged;

protected:
	virtual void BeginPlay() override;

	// ---------- 입력 처리 ----------
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void Interact();
	void ToggleView();

	// 달리기 조건(Shift, 이동 중, 지면, 스태미나)을 확인해서 상태 갱신
	void UpdateSprint();
	void SetSprinting(bool bNewSprinting);
	
	// 퀵슬롯에서 아이템 사용 시 (효과는 OnUseItem, 소비 아이템이면 1개 소모)
	void UpdateMoveSpeed();
	void EndMoveSpeedBuff();
	void EndMaxStaminaBuff();

	// ---------- 컴포넌트 ----------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// LCODoor가 FindComponentByClass로 열쇠를 확인하므로 반드시 필요
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<ULCOPlayerInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	TObjectPtr<ULCOStaminaComponent> StaminaComponent;
	
	// 의심도 (AI 담당). FindComponentByClass로도 찾을 수 있음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suspicion")
	TObjectPtr<USuspicionComponent> SuspicionComponent;

	// 수첩 (발견한 증거 목록)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Notebook")
	TObjectPtr<UNotebookComponent> NotebookComponent;

	// 퀵슬롯 (Q 휠). 입력 / 위젯은 BP에서 이 컴포넌트에 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot")
	TObjectPtr<ULCOQuickSlotComponent> QuickSlotComponent;

	// ---------- 입력 에셋 (BP에서 지정) ----------
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleViewAction;

	// ---------- 카메라 설정 ----------
	// 시작 시점
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	ELCOViewMode DefaultViewMode = ELCOViewMode::FirstPerson;

	// 캡슐 중심 기준 카메라 높이 (눈 높이)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraPivotHeight = 64.f;

	// 3인칭 카메라 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float ThirdPersonArmLength = 230.f;

	// 3인칭 어깨 너머 오프셋 (Y: 오른쪽, Z: 위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector ThirdPersonSocketOffset = FVector(0.f, 55.f, 65.f);

	// ---------- 이동 설정 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 550.f;

	// 이 속도 이상으로 움직이고 있어야 달리기로 판정 (제자리 Shift는 스태미나 소모 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float MinSprintVelocity = 10.f;

private:
	ELCOViewMode ViewMode = ELCOViewMode::FirstPerson;

	// Shift를 누르고 있는지
	bool bWantsToSprint = false;

	// 실제로 달리는 중인지
	bool bIsSprinting = false;
	
	// 이동속도 버프 증가량 (버프 없으면 0)
	float MoveSpeedBonus = 0.f;

	// 최대 스태미나 버프 증가량 (버프 없으면 0)
	int32 MaxStaminaBonus = 0;

	// 최대 스태미나 버프 적용 전 원래 최대치
	int32 BaseMaxStamina = 0;

	FTimerHandle MoveSpeedBuffTimer;
	FTimerHandle MaxStaminaBuffTimer;
};