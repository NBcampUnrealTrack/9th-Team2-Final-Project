#include "Player/LCOPlayerCharacter.h"
#include "Player/LCOPlayerController.h"
#include "Player/LCOPlayerInteractionComponent.h"
#include "Player/LCOStaminaComponent.h"
#include "Player/LCOCameraOcclusionComponent.h"
#include "Inventory/InventoryComponent.h"
#include "AI/SuspicionComponent.h"
#include "Notebook/NotebookComponent.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "CameraCapture/LCOCameraCaptureComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "TimerManager.h"

ALCOPlayerCharacter::ALCOPlayerCharacter()
{
	// 달리기 상태, 시야각 갱신용
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// 스프링암: 3인칭일 때 거리 확보, 1인칭일 때는 길이 0
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, CameraPivotHeight));
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	CameraOcclusionComponent = CreateDefaultSubobject<ULCOCameraOcclusionComponent>(TEXT("CameraOcclusion"));
	CameraCaptureComponent = CreateDefaultSubobject<ULCOCameraCaptureComponent>(TEXT("CameraCapture"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	InteractionComponent = CreateDefaultSubobject<ULCOPlayerInteractionComponent>(TEXT("PlayerInteraction"));
	StaminaComponent = CreateDefaultSubobject<ULCOStaminaComponent>(TEXT("Stamina"));
	SuspicionComponent = CreateDefaultSubobject<USuspicionComponent>(TEXT("SuspicionComponent"));
	NotebookComponent = CreateDefaultSubobject<UNotebookComponent>(TEXT("Notebook"));
	QuickSlotComponent = CreateDefaultSubobject<ULCOQuickSlotComponent>(TEXT("QuickSlot"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// 1인칭에서 메시를 숨겨도 그림자는 보이도록
	GetMesh()->bCastHiddenShadow = true;

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
}

void ALCOPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 웅크리기 설정 (BP에서 바꾼 값 반영)
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(CrouchedHalfHeight);

	// 상호작용 트레이스(GetPawnViewLocation)가 웅크렸을 때도 카메라 높이와 맞도록
	CrouchedEyeHeight = CrouchedCameraPivotHeight;

	CurrentCameraHeight = CameraPivotHeight;

	// BP에서 바꾼 값 반영
	UpdateMoveSpeed();
	Camera->SetFieldOfView(DefaultFOV);
	SetViewMode(DefaultViewMode);
}

void ALCOPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateSprint();
	UpdateCameraFOV(DeltaSeconds);
	UpdateCameraHeight(DeltaSeconds);
}

void ALCOPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("[LCOPlayerCharacter] EnhancedInputComponent not found"));
		return;
	}

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALCOPlayerCharacter::Move);
	}

	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALCOPlayerCharacter::Look);
	}

	if (SprintAction)
	{
		// 누르고 있는 동안 달리기
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ALCOPlayerCharacter::StartSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ALCOPlayerCharacter::StopSprint);
	}

	if (InteractAction)
	{
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ALCOPlayerCharacter::Interact);
	}

	if (ToggleViewAction)
	{
		EIC->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ALCOPlayerCharacter::ToggleView);
	}

	if (CaptureAction)
	{
		EIC->BindAction(CaptureAction, ETriggerEvent::Started, this, &ALCOPlayerCharacter::TakePhoto);
	}
	if (CrouchAction)
	{
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ALCOPlayerCharacter::CrouchPressed);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ALCOPlayerCharacter::CrouchReleased);
	}
}

// ---------- 이동 / 시점 입력 ----------

void ALCOPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();

	// 카메라가 바라보는 방향(Yaw) 기준으로 이동
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void ALCOPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X * LookSensitivity);
	AddControllerPitchInput(Input.Y * LookSensitivity * (bInvertLookY ? -1.f : 1.f));
}

void ALCOPlayerCharacter::StartSprint()
{
	bWantsToSprint = true;
}

void ALCOPlayerCharacter::StopSprint()
{
	bWantsToSprint = false;
}

void ALCOPlayerCharacter::UpdateSprint()
{
	const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(MinSprintVelocity);
	const bool bOnGround = GetCharacterMovement()->IsMovingOnGround();

	SetSprinting(bWantsToSprint && bMoving && bOnGround && !bIsCrouched && StaminaComponent->CanSprint());
}

void ALCOPlayerCharacter::SetSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;

	UpdateMoveSpeed();
	StaminaComponent->SetConsuming(bIsSprinting);

	OnSprintStateChanged.Broadcast(bIsSprinting);

	if (bIsSprinting)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Sprint"));
		OnSprintStarted.Broadcast();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Walk"));
		OnWalkStarted.Broadcast();
	}
}

void ALCOPlayerCharacter::Interact()
{
	// 메뉴(아이템, 수첩, 옵션)가 열려 있으면 상호작용 막기
	const ALCOPlayerController* PC = Cast<ALCOPlayerController>(Controller);
	if (PC && PC->IsInUIMode())
	{
		return;
	}

	InteractionComponent->TryInteract();
}

void ALCOPlayerCharacter::ToggleView()
{
	SetViewMode(ViewMode == ELCOViewMode::FirstPerson ? ELCOViewMode::ThirdPerson : ELCOViewMode::FirstPerson);
}

// ---------- 카메라 ----------

void ALCOPlayerCharacter::SetViewMode(ELCOViewMode NewMode)
{
	ViewMode = NewMode;
	const bool bFirstPerson = (NewMode == ELCOViewMode::FirstPerson);

	// BP에서 바꾼 카메라 높이 반영
	ApplyCameraHeight();
	SpringArm->TargetArmLength = bFirstPerson ? 0.f : ThirdPersonArmLength;
	SpringArm->SocketOffset = bFirstPerson ? FVector::ZeroVector : ThirdPersonSocketOffset;
	SpringArm->bDoCollisionTest = !bFirstPerson;

	// 3인칭에서만 카메라가 살짝 늦게 따라오도록 (1인칭은 머리에 고정)
	SpringArm->bEnableCameraLag = !bFirstPerson;
	SpringArm->CameraLagSpeed = ThirdPersonCameraLagSpeed;

	// 1인칭: 몸이 카메라 방향으로 즉시 회전
	// 3인칭: 몸이 카메라 방향으로 부드럽게 회전 (배그 방식, 회전 속도는 RotationRate)
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = !bFirstPerson;

	// 1인칭에서는 내 화면에서 내 메시 숨김 (카메라가 머리 안에 들어가는 문제 방지)
	GetMesh()->SetOwnerNoSee(bFirstPerson);
}

void ALCOPlayerCharacter::UpdateCameraFOV(float DeltaSeconds)
{
	const float TargetFOV = bIsSprinting ? SprintFOV : DefaultFOV;
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, FOVInterpSpeed));
}

void ALCOPlayerCharacter::SetLookSensitivity(float NewSensitivity)
{
	LookSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 5.f);
}

void ALCOPlayerCharacter::SetInvertLookY(bool bInvert)
{
	bInvertLookY = bInvert;
}

// ---------- 스태미나 ----------

void ALCOPlayerCharacter::SetStamina(int32 NewStamina)
{
	StaminaComponent->SetCurrentStamina(NewStamina);
}

void ALCOPlayerCharacter::RestoreStamina(int32 Amount)
{
	StaminaComponent->RestoreStamina(Amount);
}

void ALCOPlayerCharacter::ConsumeStamina(int32 Amount)
{
	StaminaComponent->ConsumeStamina(Amount);
}

void ALCOPlayerCharacter::SetMaxStamina(int32 NewMaxStamina, bool bFillToMax)
{
	StaminaComponent->SetMaxStamina(NewMaxStamina, bFillToMax);
}

int32 ALCOPlayerCharacter::GetCurrentStamina() const
{
	return StaminaComponent->GetCurrentStamina();
}

int32 ALCOPlayerCharacter::GetMaxStamina() const
{
	return StaminaComponent->GetMaxStamina();
}

float ALCOPlayerCharacter::GetStaminaPercent() const
{
	return StaminaComponent->GetStaminaPercent();
}

int32 ALCOPlayerCharacter::GetStaminaPercentInt() const
{
	return StaminaComponent->GetStaminaPercentInt();
}

// ---------- 버프 ----------

void ALCOPlayerCharacter::UpdateMoveSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = (bIsSprinting ? SprintSpeed : WalkSpeed) + MoveSpeedBonus;

	// 웅크렸을 때도 이동속도 버프 적용
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed + MoveSpeedBonus;
}

bool ALCOPlayerCharacter::ApplyMoveSpeedBuff(float Amount, float Duration)
{
	if (Amount <= 0.f || Duration <= 0.f)
	{
		return false;
	}

	// 중첩하지 않고 수치와 시간만 갱신
	MoveSpeedBonus = Amount;
	UpdateMoveSpeed();

	GetWorldTimerManager().SetTimer(MoveSpeedBuffTimer, this, &ALCOPlayerCharacter::EndMoveSpeedBuff, Duration, false);

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] MoveSpeedBuff +%.0f for %.1fs"), Amount, Duration);
	return true;
}

void ALCOPlayerCharacter::EndMoveSpeedBuff()
{
	MoveSpeedBonus = 0.f;
	UpdateMoveSpeed();

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] MoveSpeedBuff End"));
}

bool ALCOPlayerCharacter::ApplyMaxStaminaBuff(int32 Amount, float Duration)
{
	if (Amount <= 0 || Duration <= 0.f)
	{
		return false;
	}

	// 버프가 없을 때의 최대치를 기억 (버프 중 다시 사용해도 원래 값 유지)
	if (!IsMaxStaminaBuffActive())
	{
		BaseMaxStamina = StaminaComponent->GetMaxStamina();
	}

	MaxStaminaBonus = Amount;
	StaminaComponent->SetMaxStamina(BaseMaxStamina + MaxStaminaBonus);

	// 늘어난 만큼 바로 채워줌
	StaminaComponent->RestoreStamina(Amount);

	GetWorldTimerManager().SetTimer(MaxStaminaBuffTimer, this, &ALCOPlayerCharacter::EndMaxStaminaBuff, Duration, false);

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] MaxStaminaBuff +%d for %.1fs"), Amount, Duration);
	return true;
}

void ALCOPlayerCharacter::EndMaxStaminaBuff()
{
	MaxStaminaBonus = 0;

	// 원래 최대치로 복구 (현재 스태미나가 더 많으면 최대치에 맞춰 잘림)
	StaminaComponent->SetMaxStamina(BaseMaxStamina);

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] MaxStaminaBuff End"));
}

bool ALCOPlayerCharacter::IsMoveSpeedBuffActive() const
{
	return GetWorldTimerManager().IsTimerActive(MoveSpeedBuffTimer);
}

bool ALCOPlayerCharacter::IsMaxStaminaBuffActive() const
{
	return GetWorldTimerManager().IsTimerActive(MaxStaminaBuffTimer);
}

float ALCOPlayerCharacter::GetMoveSpeedBuffRemainingTime() const
{
	return FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(MoveSpeedBuffTimer));
}

float ALCOPlayerCharacter::GetMaxStaminaBuffRemainingTime() const
{
	return FMath::Max(0.f, GetWorldTimerManager().GetTimerRemaining(MaxStaminaBuffTimer));
}

// ---------- 카메라 촬영 ----------

void ALCOPlayerCharacter::TakePhoto()
{
	// 메뉴(인벤토리, 수첩, 옵션)가 열려 있으면 촬영 막기 (UI 클릭이 촬영으로 이어지지 않도록)
	const ALCOPlayerController* PC = Cast<ALCOPlayerController>(Controller);
	if (PC && PC->IsInUIMode())
	{
		return;
	}

	if (!CameraCaptureComponent->Capture())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Capture failed (대상 없음 / 필름 없음 / 이미 촬영)"));
	}
}

// ---------- 웅크리기 ----------

void ALCOPlayerCharacter::CrouchPressed()
{
	// 메뉴가 열려 있으면 막기
	const ALCOPlayerController* PC = Cast<ALCOPlayerController>(Controller);
	if (PC && PC->IsInUIMode())
	{
		return;
	}

	// 토글: 웅크리려는 중이면 일어서기, 아니면 웅크리기
	// (bWantsToCrouch 기준이라 천장에 막혀 못 일어서는 중에 누르면 다시 웅크림으로 취소됨)
	if (bToggleCrouch && GetCharacterMovement()->bWantsToCrouch)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ALCOPlayerCharacter::CrouchReleased()
{
	// 누르고 있는 동안만 웅크리는 방식일 때만 뗄 때 일어섬
	if (!bToggleCrouch)
	{
		UnCrouch();
	}
}

void ALCOPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// 캡슐이 줄면서 중심이 아래로 내려가므로, 카메라가 뚝 떨어지지 않게 보정 → 이후 Tick에서 부드럽게 내려감
	CurrentCameraHeight += ScaledHalfHeightAdjust;
	ApplyCameraHeight();

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Crouch"));
	OnCrouchStateChanged.Broadcast(true);
}

void ALCOPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// 캡슐이 커지면서 중심이 위로 올라가므로, 카메라가 뚝 튀지 않게 보정 → 이후 Tick에서 부드럽게 올라감
	CurrentCameraHeight -= ScaledHalfHeightAdjust;
	ApplyCameraHeight();

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] UnCrouch"));
	OnCrouchStateChanged.Broadcast(false);
}

void ALCOPlayerCharacter::UpdateCameraHeight(float DeltaSeconds)
{
	const float TargetHeight = bIsCrouched ? CrouchedCameraPivotHeight : CameraPivotHeight;
	CurrentCameraHeight = FMath::FInterpTo(CurrentCameraHeight, TargetHeight, DeltaSeconds, CameraHeightInterpSpeed);
	ApplyCameraHeight();
}

void ALCOPlayerCharacter::ApplyCameraHeight()
{
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, CurrentCameraHeight));
}