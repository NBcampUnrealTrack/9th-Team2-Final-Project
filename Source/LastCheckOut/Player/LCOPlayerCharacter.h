#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LCOPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInventoryComponent;
class ULCOPlayerInteractionComponent;
struct FInputActionValue;

UENUM(BlueprintType)
enum class ELCOViewMode : uint8
{
	FirstPerson  UMETA(DisplayName = "First Person"),
	ThirdPerson  UMETA(DisplayName = "Third Person")
};

// 플레이어 캐릭터
// 이동(WASD), 시점(마우스), 달리기(Shift), 상호작용(F), 1인칭/3인칭 전환(V) 담당
// 메뉴 입력(아이템, 수첩, 옵션)은 ALCOPlayerController에서 처리
UCLASS()
class LASTCHECKOUT_API ALCOPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALCOPlayerCharacter();

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

protected:
	virtual void BeginPlay() override;

	// ---------- 입력 처리 ----------
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void Interact();
	void ToggleView();

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
	float ThirdPersonArmLength = 250.f;

	// 3인칭 어깨 너머 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector ThirdPersonSocketOffset = FVector(0.f, 40.f, 10.f);

	// ---------- 이동 설정 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 550.f;

private:
	ELCOViewMode ViewMode = ELCOViewMode::FirstPerson;
};