#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LCOPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInventoryComponent;
class ULCOPlayerInteractionComponent;
class ULCOStaminaComponent;
class USuspicionComponent;
class ULCOQuickSlotComponent;
class ULCOCameraCaptureComponent;
class ULCOCameraOcclusionComponent;
class ULCOHidingComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ALCOSenseVolume;
class UStaticMeshComponent;
class UAnimMontage;
class ALCOHidingSpot;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOMoveStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOSprintStateChanged, bool, bIsSprinting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOCrouchStateChanged, bool, bIsCrouching);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLCOSenseChargesChanged, int32, CurrentCharges, int32, MaxCharges);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOSenseUsed, bool, bSuccess);

UENUM(BlueprintType)
enum class ELCOViewMode : uint8
{
	FirstPerson  UMETA(DisplayName = "First Person"),
	ThirdPerson  UMETA(DisplayName = "Third Person")
};

// 플레이어 캐릭터
// 이동(WASD), 시점(마우스), 달리기(Shift, 스태미나 소모), 상호작용(F), 1인칭/3인칭 전환(V), 촬영(좌클릭) 담당
// 메뉴 입력(아이템, 수첩, 옵션)은 ALCOPlayerController에서 처리
UCLASS()
class LASTCHECKOUT_API ALCOPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

	// 카메라를 손에 들고 있는지 (애님 BP에서 상체 포즈 전환용)
	UFUNCTION(BlueprintPure, Category = "LCO|Camera")
	bool IsCameraEquipped() const { return bCameraEquipped; }
	
public:
	ALCOPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ---------- 시점 ----------

	// 시점 변경 (컷신, 특정 구역에서 강제 전환할 때도 사용)
	UFUNCTION(BlueprintCallable, Category = "LCO|Camera")
	void SetViewMode(ELCOViewMode NewMode);

	UFUNCTION(BlueprintPure, Category = "LCO|Camera")
	ELCOViewMode GetViewMode() const { return ViewMode; }

	// 마우스 감도 배율 (1 = 기본, 0.1 ~ 5). 설정 UI에서 호출
	UFUNCTION(BlueprintCallable, Category = "LCO|Camera")
	void SetLookSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintPure, Category = "LCO|Camera")
	float GetLookSensitivity() const { return LookSensitivity; }

	// 마우스 상하 반전. 설정 UI에서 호출
	UFUNCTION(BlueprintCallable, Category = "LCO|Camera")
	void SetInvertLookY(bool bInvert);

	UFUNCTION(BlueprintPure, Category = "LCO|Camera")
	bool IsInvertLookY() const { return bInvertLookY; }

	// ---------- 컴포넌트 접근 ----------

	UFUNCTION(BlueprintPure, Category = "LCO|Inventory")
	UInventoryComponent* GetInventory() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Interaction")
	ULCOPlayerInteractionComponent* GetInteraction() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	ULCOStaminaComponent* GetStamina() const { return StaminaComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Suspicion")
	USuspicionComponent* GetSuspicion() const { return SuspicionComponent; }
	
	UFUNCTION(BlueprintPure, Category = "LCO|QuickSlot")
	ULCOQuickSlotComponent* GetQuickSlot() const { return QuickSlotComponent; }

	UFUNCTION(BlueprintPure, Category = "LCO|Camera")
	ULCOCameraCaptureComponent* GetCameraCapture() const { return CameraCaptureComponent; }
	
	UFUNCTION(BlueprintPure, Category = "LCO|Camera")
   	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	
	UFUNCTION(BlueprintPure, Category = "LCO|Hiding")
	ULCOHidingComponent* GetHiding() const { return HidingComponent; }

	// ---------- 이동 ----------

	UFUNCTION(BlueprintPure, Category = "LCO|Movement")
	bool IsSprinting() const { return bIsSprinting; }
	
	UFUNCTION(BlueprintPure, Category = "LCO|Movement")
	bool IsCrouching() const { return bIsCrouched; }

	// ACharacter: 실제로 웅크리기 / 일어서기가 끝났을 때 엔진이 호출
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// ---------- 스태미나 (UI / 아이템에서 캐릭터를 통해 바로 사용) ----------

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

	// ---------- 버프 (아이템 사용 시 InventoryComponent::UseItem에서 호출) ----------

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
	
	// 현재 적용 중인 버프 수치. 버프가 없으면 0 (맵 이동 시 버프 유지용, LCOGameMode에서 사용)
	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	float GetMoveSpeedBuffAmount() const { return MoveSpeedBonus; }

	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	int32 GetMaxStaminaBuffAmount() const { return MaxStaminaBonus; }
	
		// 버프 화면 효과 진행도 0 ~ 1 (시작 / 종료 시 BuffEffectBlendTime 동안 서서히 변함). BP 추가 연출용
	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	float GetMoveSpeedBuffEffectAlpha() const { return MoveSpeedBuffAlpha; }

	UFUNCTION(BlueprintPure, Category = "LCO|Buff")
	float GetMaxStaminaBuffEffectAlpha() const { return MaxStaminaBuffAlpha; }

	// 최대 스태미나 버프를 뺀 기본 최대 스태미나 (감각 사용 조건 / 소모량 기준)
	UFUNCTION(BlueprintPure, Category = "LCO|Stamina")
	int32 GetBaseMaxStamina() const;

	// ---------- 감각(가칭) ----------

	UFUNCTION(BlueprintPure, Category = "LCO|Sense")
	int32 GetCurrentSenseCharges() const { return CurrentSenseCharges; }

	UFUNCTION(BlueprintPure, Category = "LCO|Sense")
	int32 GetMaxSenseCharges() const { return MaxSenseCharges; }

	// 사용 횟수 회복 (최대 횟수 초과 불가). 세이브 시 전체 회복은 GetMaxSenseCharges()를 넘기면 됨
	UFUNCTION(BlueprintCallable, Category = "LCO|Sense")
	void RestoreSenseCharges(int32 Amount);

	// 지금 사용할 수 있는지 (남은 횟수 1 이상 + 스태미나가 기본 최대치 이상). UI 아이콘 활성화 등
	UFUNCTION(BlueprintPure, Category = "LCO|Sense")
	bool CanUseSense() const;

	// Tab 홀드 중인지
	UFUNCTION(BlueprintPure, Category = "LCO|Sense")
	bool IsSenseHolding() const;

	// 홀드 진행도 0 ~ 1 (게이지 UI용, 홀드 중이 아니면 0)
	UFUNCTION(BlueprintPure, Category = "LCO|Sense")
	float GetSenseHoldProgress() const;

	// ---------- 이벤트 ----------

	// 걷기 → 달리기로 바뀔 때 (발소리, 숨소리, UI 등)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Movement")
	FOnLCOMoveStateChanged OnSprintStarted;

	// 달리기 → 걷기로 바뀔 때 (Shift 뗌, 스태미나 소진, 멈춤)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Movement")
	FOnLCOMoveStateChanged OnWalkStarted;

	// 달리기 상태가 바뀔 때 (true: 달리기 시작, false: 걷기로 복귀)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Movement")
	FOnLCOSprintStateChanged OnSprintStateChanged;
	
	// 웅크리기 상태가 바뀔 때 (true: 웅크림, false: 일어섬) - AI 감지, UI, 사운드 등
	UPROPERTY(BlueprintAssignable, Category = "LCO|Movement")
	FOnLCOCrouchStateChanged OnCrouchStateChanged;
	
	// 감각 사용 횟수가 바뀔 때 (UI 갱신용)
	UPROPERTY(BlueprintAssignable, Category = "LCO|Sense")
	FOnLCOSenseChargesChanged OnSenseChargesChanged;

	// 감각을 사용했을 때 (true: 감각 볼륨 안에서 성공, false: 실패). 의심도 등 후속 처리는 구독해서 처리
	UPROPERTY(BlueprintAssignable, Category = "LCO|Sense")
	FOnLCOSenseUsed OnSenseUsed;

protected:
	virtual void BeginPlay() override;

	// ---------- 입력 처리 ----------
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void Interact();
	void ToggleView();

	// 카메라 촬영 (CameraCapture 컴포넌트로 전달)
	void TakePhoto();
	
	// 메뉴가 열려 있거나 숨어 있어서 행동(웅크리기, 시점 전환, 촬영, 감각)을 막아야 하는지
	bool IsActionBlocked() const;

	// 숨기 / 나오기 시 캐릭터 상태 정리 (HidingComponent의 OnHidingChanged에 바인딩)
	UFUNCTION()
	void HandleHidingChanged(bool bIsHiding, ALCOHidingSpot* HidingSpot);
	
	// 퀵슬롯에서 카메라를 들거나 내릴 때 (메시 표시 + 몽타주)
	UFUNCTION()
	void HandleCameraEquipChanged(bool bEquipped);
	
	// 웅크리기 입력
	void CrouchPressed();
	void CrouchReleased();

	// 웅크리기 / 일어서기에 맞춰 카메라 높이를 부드럽게 변경
	void UpdateCameraHeight(float DeltaSeconds);
	void ApplyCameraHeight();
	// 시점에 맞는 메시의 손에 손 카메라 부착 (1인칭: 1인칭 메시 / 3인칭: 3인칭 메시)
	void AttachHeldCamera();

	// 달리기 조건(Shift, 이동 중, 지면, 스태미나)을 확인해서 상태 갱신
	void UpdateSprint();
	void SetSprinting(bool bNewSprinting);

	// 현재 상태(걷기 / 달리기 + 이동속도 버프)에 맞게 MaxWalkSpeed 갱신
	void UpdateMoveSpeed();

	void EndMoveSpeedBuff();
	void EndMaxStaminaBuff();

	// 달리기 상태에 맞춰 시야각을 부드럽게 변경
	void UpdateCameraFOV(float DeltaSeconds);
	
		// 버프 활성 여부에 맞춰 화면 효과 진행도를 보간하고 포스트 프로세스에 반영
	void UpdateBuffEffects(float DeltaSeconds);

	// BP에서 지정한 머티리얼로 MID를 만들어 카메라 포스트 프로세스에 등록 (머티리얼이 없으면 nullptr)
	UMaterialInstanceDynamic* CreateBuffPostProcess(UMaterialInterface* Material);
	void ApplyBuffPostProcess(UMaterialInstanceDynamic* MID, float Intensity);

	// 감각 입력 (Tab 홀드). 누르면 홀드 시작, SenseHoldTime 전에 떼면 취소
	void SensePressed();
	void SenseReleased();

	// 홀드 완료 시 호출. 조건 재확인 후 자원 소모 + 성공 판정
	void ActivateSense();

	// 현재 위치가 들어 있는 감각 볼륨 (없으면 nullptr)
	ALCOSenseVolume* FindSenseVolume() const;

	void SetSenseCharges(int32 NewCharges);

	// ---------- 컴포넌트 ----------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// 3인칭 카메라 가림 처리 (벽에 붙으면 내 몸 숨김, 카메라를 가리는 NPC 숨김)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<ULCOCameraOcclusionComponent> CameraOcclusionComponent;

	// 카메라 촬영 (필름 소모, 사진 획득 이벤트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<ULCOCameraCaptureComponent> CameraCaptureComponent;

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
	

	// 퀵슬롯 (Q 휠). 입력 / 위젯은 BP에서 이 컴포넌트에 지정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot")
	TObjectPtr<ULCOQuickSlotComponent> QuickSlotComponent;
	
	// 숨기 (옷장 등 LCOHidingSpot과 상호작용). 숨은 상태에서는 상호작용 키가 나오기로 동작
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hiding")
	TObjectPtr<ULCOHidingComponent> HidingComponent;
	
		// 1인칭 전용 몸 메시 (내 화면에서만 보임). ABP_FP_Copy로 3인칭 메시 포즈를 복사해서 팔이 보이게 함
    	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
    	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UStaticMeshComponent> HeldCameraMesh;

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

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CaptureAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;
	
	// 감각 (Tab 홀드). 홀드 시간은 SenseHoldTime으로 코드에서 처리하므로 IA에 Hold 트리거 불필요
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SenseAction;

	// ---------- 카메라 설정 ----------
	// 시작 시점
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	ELCOViewMode DefaultViewMode = ELCOViewMode::FirstPerson;

	// 캡슐 중심 기준 카메라 높이 (눈 높이)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraPivotHeight = 64.f;
	
	// 웅크렸을 때 카메라 높이 (캡슐 중심 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CrouchedCameraPivotHeight = 40.f;

	// 웅크리기 / 일어서기 시 카메라 높이가 바뀌는 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraHeightInterpSpeed = 10.f;

	// 3인칭 카메라 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float ThirdPersonArmLength = 230.f;

	// 3인칭 어깨 너머 오프셋 (Y: 오른쪽, Z: 위)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector ThirdPersonSocketOffset = FVector(0.f, 55.f, 65.f);

	// 3인칭 카메라 위치 지연 속도 (클수록 빠르게 따라옴)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float ThirdPersonCameraLagSpeed = 12.f;
	
	// 손에 드는 카메라 메시를 붙일 소켓
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Held")
	FName HeldCameraSocket = TEXT("hand_r");

	// 카메라를 들 때 / 내릴 때 몽타주 (비워두면 재생 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Held")
	TObjectPtr<UAnimMontage> CameraEquipMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Held")
	TObjectPtr<UAnimMontage> CameraUnequipMontage;
	

	// 마우스 감도 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Look", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float LookSensitivity = 1.f;

	// 마우스 상하 반전
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Look")
	bool bInvertLookY = false;

	// 기본 시야각
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FOV")
	float DefaultFOV = 90.f;

	// 달릴 때 시야각 (속도감)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FOV")
	float SprintFOV = 100.f;

	// 시야각이 바뀌는 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FOV")
	float FOVInterpSpeed = 8.f;
	
	// 이동속도 버프 중 추가 시야각 (평상시 / 달리기 FOV에 더해짐)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FOV")
	float MoveSpeedBuffFOV = 5.f;
	
	// 감각(Tab) 홀드 중 줄어드는 시야각 (줌인). 홀드 진행도에 따라 점점 줌인, 발동 / 취소되면 원래대로 풀림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FOV")
	float SenseZoomFOV = 25.f;
	
	// 1인칭 팔을 그릴 때 쓰는 시야각 (카메라 FOV와 별개라 줌인해도 팔은 그대로)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FirstPerson")
    float FirstPersonArmsFOV = 70.f;
    
    // 1인칭 팔 크기 (작을수록 벽에 덜 파묻힘)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|FirstPerson", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float FirstPersonArmsScale = 0.6f;
	
	// ---------- 버프 화면 효과 ----------
	// 버프 시작 / 종료 시 효과가 완전히 켜지거나 꺼지는 데 걸리는 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|BuffEffect", meta = (ClampMin = "0.01"))
	float BuffEffectBlendTime = 0.5f;

	// 포스트 프로세스 머티리얼의 강도 스칼라 파라미터 이름 (두 머티리얼 공통)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|BuffEffect")
	FName BuffPostProcessIntensityParam = TEXT("Intensity");

	// 이동속도 버프 포스트 프로세스 (Post Process 도메인 머티리얼). 비워두면 효과 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|BuffEffect")
	TObjectPtr<UMaterialInterface> MoveSpeedBuffPostProcessMaterial;

	// 버프가 완전히 적용됐을 때의 강도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|BuffEffect", meta = (ClampMin = "0"))
	float MoveSpeedBuffPostProcessIntensity = 1.f;

	// 최대 스태미나 버프 포스트 프로세스. 비워두면 효과 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|BuffEffect")
	TObjectPtr<UMaterialInterface> MaxStaminaBuffPostProcessMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|BuffEffect", meta = (ClampMin = "0"))
	float MaxStaminaBuffPostProcessIntensity = 1.f;

	// ---------- 이동 설정 ----------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 550.f;

	// 이 속도 이상으로 움직이고 있어야 달리기로 판정 (제자리 Shift는 스태미나 소모 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float MinSprintVelocity = 10.f;
	
	// 웅크렸을 때 이동 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crouch")
	float CrouchSpeed = 150.f;

	// 웅크렸을 때 캡슐 절반 높이 (서 있을 때 96)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crouch")
	float CrouchedHalfHeight = 60.f;

	// true: 한 번 누르면 웅크림 / 다시 누르면 일어섬
	// false: 누르고 있는 동안만 웅크림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Crouch")
	bool bToggleCrouch = true;
	
	// ---------- 감각 설정 ----------
	// 최대 사용 횟수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense", meta = (ClampMin = "0"))
	int32 MaxSenseCharges = 3;

	// 이 시간(초) 동안 누르고 있어야 발동
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense", meta = (ClampMin = "0.1"))
	float SenseHoldTime = 1.f;

private:
	ELCOViewMode ViewMode = ELCOViewMode::FirstPerson;

	// Shift를 누르고 있는지
	bool bWantsToSprint = false;

	// 실제로 달리는 중인지
	bool bIsSprinting = false;
	
	bool bCameraEquipped = false;
	
	// 현재 카메라 높이 (부드럽게 바뀌는 중간값)
	float CurrentCameraHeight = 0.f;

	// 이동속도 버프 증가량 (버프 없으면 0)
	float MoveSpeedBonus = 0.f;

	// 최대 스태미나 버프 증가량 (버프 없으면 0)
	int32 MaxStaminaBonus = 0;

	// 최대 스태미나 버프 적용 전 원래 최대치
	int32 BaseMaxStamina = 0;

	FTimerHandle MoveSpeedBuffTimer;
	FTimerHandle MaxStaminaBuffTimer;
	
	FTimerHandle SenseHoldTimer;

	int32 CurrentSenseCharges = 0;

	// 버프 화면 효과 진행도 (0 ~ 1)
	float MoveSpeedBuffAlpha = 0.f;
	float MaxStaminaBuffAlpha = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoveSpeedBuffPostProcessMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaxStaminaBuffPostProcessMID;
};