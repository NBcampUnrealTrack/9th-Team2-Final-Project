#include "Player/LCOPlayerCharacter.h"
#include "Player/LCOPlayerController.h"
#include "Player/LCOPlayerInteractionComponent.h"
#include "Player/LCOStaminaComponent.h"
#include "Player/LCOCameraOcclusionComponent.h"
#include "Inventory/InventoryComponent.h"
#include "AI/SuspicionComponent.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "CameraCapture/LCOCameraCaptureComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Interaction/LCOHidingComponent.h"
#include "InputActionValue.h"
#include "Player/LCOSenseVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Player/LCOFootstepComponent.h"
#include "Components/SpotLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/CollisionProfile.h"

ALCOPlayerCharacter::ALCOPlayerCharacter()
{
	// 달리기 상태, 시야각 갱신용
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	// 손전등 (눈 높이 스프링암 시작점에 부착, 기본 꺼짐)
	FlashlightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	FlashlightComponent->SetupAttachment(SpringArm);
	FlashlightComponent->SetRelativeLocation(FlashlightOffset);
	FlashlightComponent->Intensity = 5000.f;
	FlashlightComponent->AttenuationRadius = 1500.f;
	FlashlightComponent->InnerConeAngle = 15.f;
	FlashlightComponent->OuterConeAngle = 28.f;
	FlashlightComponent->SetVisibility(false);
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
	QuickSlotComponent = CreateDefaultSubobject<ULCOQuickSlotComponent>(TEXT("QuickSlot"));
	HidingComponent = CreateDefaultSubobject<ULCOHidingComponent>(TEXT("Hiding"));
	FootstepComponent = CreateDefaultSubobject<ULCOFootstepComponent>(TEXT("Footstep"));
	// 1인칭 전용 몸 메시 (내 화면에서만 보임, 3인칭 메시 포즈를 복사)
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	FirstPersonMesh->CastShadow = false; // 그림자는 3인칭 메시가 담당
	// 1인칭 메시는 별도 시야각 / 크기로 그려서 벽에 팔이 파묻히지 않게 함
	Camera->bEnableFirstPersonFieldOfView = true;
	Camera->bEnableFirstPersonScale = true;
	HeldCameraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldCameraMesh"));
	HeldCameraMesh->SetupAttachment(GetMesh());
	HeldCameraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldCameraMesh->SetVisibility(false);
	HeldCameraMesh->bCastHiddenShadow = true;

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
	// 1인칭 팔 시야각 / 크기 (BP 값 반영)
	Camera->SetFirstPersonFieldOfView(FirstPersonArmsFOV);
	Camera->SetFirstPersonScale(FirstPersonArmsScale);
	FirstPersonMesh->HideBoneByName(TEXT("neck_01"), EPhysBodyOp::PBO_None);

	QuickSlotComponent->OnCameraEquipChanged.AddDynamic(this, &ALCOPlayerCharacter::HandleCameraEquipChanged);
	SetViewMode(DefaultViewMode);
	
	CurrentSenseCharges = MaxSenseCharges;

	// 버프 포스트 프로세스 등록 (강도 0으로 시작)
	MoveSpeedBuffPostProcessMID = CreateBuffPostProcess(MoveSpeedBuffPostProcessMaterial);
	MaxStaminaBuffPostProcessMID = CreateBuffPostProcess(MaxStaminaBuffPostProcessMaterial);
	// 숨을 때 진행 중이던 행동 정리
	HidingComponent->OnHidingChanged.AddUniqueDynamic(this, &ALCOPlayerCharacter::HandleHidingChanged);
	// BP에서 바꾼 위치 반영, 시작은 꺼짐
	FlashlightComponent->SetRelativeLocation(FlashlightOffset);
	SetFlashlightOn(false);
}

void ALCOPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	UpdateSprint();
	UpdateBuffEffects(DeltaSeconds);
	UpdateCameraFOV(DeltaSeconds);
	UpdateHeadBob(DeltaSeconds);
	UpdateFlashlight();
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

	// 촬영 입력은 LCOQuickSlotComponent::HandleCapturePressed에서만 처리 (중복 바인딩으로 Capture()가 두 번 호출되던 문제 수정)
	if (CrouchAction)
	{
		EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ALCOPlayerCharacter::CrouchPressed);
		EIC->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ALCOPlayerCharacter::CrouchReleased);
	}
	if (SenseAction)
	{
		// 떼면 취소 (IA에 Hold 트리거가 들어가 있어도 동작하도록 Canceled도 연결)
		EIC->BindAction(SenseAction, ETriggerEvent::Started, this, &ALCOPlayerCharacter::SensePressed);
		EIC->BindAction(SenseAction, ETriggerEvent::Completed, this, &ALCOPlayerCharacter::SenseReleased);
		EIC->BindAction(SenseAction, ETriggerEvent::Canceled, this, &ALCOPlayerCharacter::SenseReleased);
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

	// 숨어 있으면 바라보는 대상과 상관없이 밖으로 나오기 (옷장 안에서 트레이스가 컴포넌트에 안 닿는 경우 대비)
	if (HidingComponent->IsHiding())
	{
		HidingComponent->ExitHiding();
		return;
	}

	InteractionComponent->TryInteract();
}

void ALCOPlayerCharacter::ToggleView()
{
	if (IsActionBlocked())
	{
		return;
	}

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

	// 1인칭: 1인칭 메시(팔)만 내 화면에 보이고, 3인칭 메시는 숨김 (그림자는 3인칭 메시가 계속 담당)
	GetMesh()->SetOwnerNoSee(bFirstPerson);
	FirstPersonMesh->SetVisibility(bFirstPerson);

	// 손 카메라를 시점에 맞는 손에 부착
	AttachHeldCamera();
	// 1인칭에서는 내 몸 가림 처리 끄기 (그림자가 디더로 깨지는 문제 방지)
	CameraOcclusionComponent->SetSelfOcclusionEnabled(!bFirstPerson);
}

void ALCOPlayerCharacter::UpdateCameraFOV(float DeltaSeconds)
{
	// 평상시 / 달리기 FOV에 이동속도 버프 FOV를 더함 (버프 시작 / 종료 시 MoveSpeedBuffAlpha로 서서히)
	float TargetFOV = (bIsSprinting ? SprintFOV : DefaultFOV) + MoveSpeedBuffFOV * MoveSpeedBuffAlpha;

	// 감각 홀드 중에는 진행도에 따라 줌인 (발동 / 취소되면 진행도가 0이 되어 원래 FOV로 풀림)
	const float SenseZoomAlpha = FMath::InterpEaseInOut(0.f, 1.f, GetSenseHoldProgress(), 2.f);
	TargetFOV -= SenseZoomFOV * SenseZoomAlpha;

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
	const float BaseSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;

	GetCharacterMovement()->MaxWalkSpeed =
		BaseSpeed * PermanentMoveSpeedMultiplier + MoveSpeedBonus;

	GetCharacterMovement()->MaxWalkSpeedCrouched =
		CrouchSpeed * PermanentMoveSpeedMultiplier + MoveSpeedBonus;
}

void ALCOPlayerCharacter::SetPermanentMoveSpeedMultiplier(float Multiplier)
{
	PermanentMoveSpeedMultiplier = FMath::Max(1.0f, Multiplier);
	UpdateMoveSpeed();
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

int32 ALCOPlayerCharacter::GetBaseMaxStamina() const
{
	// 최대 스태미나 버프 중이면 버프 적용 전 최대치
	return IsMaxStaminaBuffActive() ? BaseMaxStamina : StaminaComponent->GetMaxStamina();
}

void ALCOPlayerCharacter::UpdateBuffEffects(float DeltaSeconds)
{
	const float BlendSpeed = 1.f / BuffEffectBlendTime;

	const float NewMoveSpeedAlpha = FMath::FInterpConstantTo(MoveSpeedBuffAlpha, IsMoveSpeedBuffActive() ? 1.f : 0.f, DeltaSeconds, BlendSpeed);
	if (NewMoveSpeedAlpha != MoveSpeedBuffAlpha)
	{
		MoveSpeedBuffAlpha = NewMoveSpeedAlpha;
		ApplyBuffPostProcess(MoveSpeedBuffPostProcessMID, MoveSpeedBuffAlpha * MoveSpeedBuffPostProcessIntensity);
	}

	const float NewMaxStaminaAlpha = FMath::FInterpConstantTo(MaxStaminaBuffAlpha, IsMaxStaminaBuffActive() ? 1.f : 0.f, DeltaSeconds, BlendSpeed);
	if (NewMaxStaminaAlpha != MaxStaminaBuffAlpha)
	{
		MaxStaminaBuffAlpha = NewMaxStaminaAlpha;
		ApplyBuffPostProcess(MaxStaminaBuffPostProcessMID, MaxStaminaBuffAlpha * MaxStaminaBuffPostProcessIntensity);
	}
}

UMaterialInstanceDynamic* ALCOPlayerCharacter::CreateBuffPostProcess(UMaterialInterface* Material)
{
	if (!Material)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
	MID->SetScalarParameterValue(BuffPostProcessIntensityParam, 0.f);

	// 버프가 없을 때는 꺼 둠 (Weight 0이면 렌더링에서 제외)
	Camera->PostProcessSettings.AddBlendable(MID, 0.f);
	return MID;
}

void ALCOPlayerCharacter::ApplyBuffPostProcess(UMaterialInstanceDynamic* MID, float Intensity)
{
	if (!MID)
	{
		return;
	}

	// 강도는 머티리얼 파라미터로 조절 (블렌더블이 하나뿐이면 Weight로는 강도가 바뀌지 않음)
	MID->SetScalarParameterValue(BuffPostProcessIntensityParam, Intensity);

	// 강도 0이면 블렌더블을 꺼서 머티리얼 연산 생략
	for (FWeightedBlendable& Blendable : Camera->PostProcessSettings.WeightedBlendables.Array)
	{
		if (Blendable.Object == MID)
		{
			Blendable.Weight = Intensity > 0.f ? 1.f : 0.f;
			break;
		}
	}
}

// ---------- 카메라 촬영 ----------

void ALCOPlayerCharacter::TakePhoto()
{
	// 메뉴가 열려 있거나 숨어 있으면 막기
	if (IsActionBlocked())
	{
		return;
	}

	// 카메라를 들고 있을 때만 촬영 (맨손 촬영 방지)
	if (!bCameraEquipped)
	{
		return;
	}

	if (!CameraCaptureComponent->Capture())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Capture failed (대상 없음 / 필름 없음 / 이미 촬영)"));
	}
}

void ALCOPlayerCharacter::HandleCameraEquipChanged(bool bEquipped)
{
	bCameraEquipped = bEquipped;
	HeldCameraMesh->SetVisibility(bEquipped);

	if (UAnimMontage* Montage = bEquipped ? CameraEquipMontage : CameraUnequipMontage)
	{
		PlayAnimMontage(Montage);
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Camera %s"), bEquipped ? TEXT("Equipped") : TEXT("Unequipped"));
}

// ---------- 웅크리기 ----------

void ALCOPlayerCharacter::CrouchPressed()
{
	// 메뉴가 열려 있거나 숨어 있으면 막기
	if (IsActionBlocked())
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
	// 1인칭은 눈 위치를 살짝 앞으로 (몸 방향 기준이라 아래를 봐도 그대로)
	const float ForwardOffset = (ViewMode == ELCOViewMode::FirstPerson) ? FirstPersonCameraForwardOffset : 0.f;
	SpringArm->SetRelativeLocation(FVector(ForwardOffset, 0.f, CurrentCameraHeight));
}

// ---------- 감각(가칭) ----------

void ALCOPlayerCharacter::SetSenseCharges(int32 NewCharges)
{
	const int32 Clamped = FMath::Clamp(NewCharges, 0, MaxSenseCharges);
	if (Clamped == CurrentSenseCharges)
	{
		return;
	}

	CurrentSenseCharges = Clamped;
	OnSenseChargesChanged.Broadcast(CurrentSenseCharges, MaxSenseCharges);
}

void ALCOPlayerCharacter::RestoreSenseCharges(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	SetSenseCharges(CurrentSenseCharges + Amount);
}

bool ALCOPlayerCharacter::CanUseSense() const
{
	// 최대 스태미나 버프와 관계없이 기본 최대치 기준
	return CurrentSenseCharges >= 1 && StaminaComponent->GetCurrentStamina() >= GetBaseMaxStamina();
}

bool ALCOPlayerCharacter::IsSenseHolding() const
{
	return GetWorldTimerManager().IsTimerActive(SenseHoldTimer);
}

float ALCOPlayerCharacter::GetSenseHoldProgress() const
{
	if (!IsSenseHolding())
	{
		return 0.f;
	}

	return FMath::Clamp(GetWorldTimerManager().GetTimerElapsed(SenseHoldTimer) / SenseHoldTime, 0.f, 1.f);
}

void ALCOPlayerCharacter::SensePressed()
{
	// 메뉴가 열려 있거나 숨어 있으면 막기
	if (IsActionBlocked())
	{
		return;
	}

	// 조건이 안 되면 홀드도 시작하지 않음 (자원 소모 / 결과 브로드캐스트 없음)
	if (!CanUseSense())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Sense unavailable (Charges %d, Stamina %d / Base %d)"),
			CurrentSenseCharges, StaminaComponent->GetCurrentStamina(), GetBaseMaxStamina());
		return;
	}

	GetWorldTimerManager().SetTimer(SenseHoldTimer, this, &ALCOPlayerCharacter::ActivateSense, SenseHoldTime, false);
}

void ALCOPlayerCharacter::SenseReleased()
{
	// 홀드 시간 전에 떼면 취소
	if (IsSenseHolding())
	{
		GetWorldTimerManager().ClearTimer(SenseHoldTimer);
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Sense canceled"));
	}
}

void ALCOPlayerCharacter::ActivateSense()
{
	// 홀드하는 동안 상태가 바뀌었을 수 있으므로 다시 확인 (달리기로 스태미나 소모, 메뉴 열림 등)
	const ALCOPlayerController* PC = Cast<ALCOPlayerController>(Controller);
	if ((PC && PC->IsInUIMode()) || !CanUseSense())
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Sense blocked on activate"));
		return;
	}

	SetSenseCharges(CurrentSenseCharges - 1);
	StaminaComponent->ConsumeStamina(GetBaseMaxStamina());

	ALCOSenseVolume* Volume = FindSenseVolume();
	const bool bSuccess = (Volume != nullptr);

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Sense %s (%s) Charges %d/%d"),
		bSuccess ? TEXT("Success") : TEXT("Fail"), *GetNameSafe(Volume), CurrentSenseCharges, MaxSenseCharges);

	if (Volume)
	{
		Volume->NotifySensed(this);
	}

	OnSenseUsed.Broadcast(bSuccess);
}

ALCOSenseVolume* ALCOPlayerCharacter::FindSenseVolume() const
{
	const FVector Location = GetActorLocation();
	for (TActorIterator<ALCOSenseVolume> It(GetWorld()); It; ++It)
	{
		if (It->IsSenseEnabled() && It->IsLocationInside(Location))
		{
			return *It;
		}
	}
	return nullptr;
}

// ---------- 숨기 ----------

bool ALCOPlayerCharacter::IsActionBlocked() const
{
	// 메뉴 열림 / 퍼즐 보기 / 숨는 중에는 웅크리기, 시점 전환, 촬영, 감각 막기 (F 상호작용은 별도 처리라 영향 없음)
	const ALCOPlayerController* PC = Cast<ALCOPlayerController>(Controller);
	return (PC && (PC->IsInUIMode() || PC->IsInPuzzleView())) || HidingComponent->IsHiding();
}

void ALCOPlayerCharacter::HandleHidingChanged(bool bIsHiding, ALCOHidingSpot* HidingSpot)
{
	if (!bIsHiding)
	{
		return;
	}

	// 숨기 전에 누르고 있던 달리기 입력 초기화 (나오자마자 달리지 않도록)
	bWantsToSprint = false;
	SetSprinting(false);

	// 감각 홀드 중이었으면 취소
	if (IsSenseHolding())
	{
		GetWorldTimerManager().ClearTimer(SenseHoldTimer);
		UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Sense canceled (hiding)"));
	}
}

// ---------- 1인칭 팔 ----------

void ALCOPlayerCharacter::AttachHeldCamera()
{
	const bool bFirstPerson = (ViewMode == ELCOViewMode::FirstPerson);

	// BP에서 조정한 상대 위치 / 회전은 유지한 채 손만 바꿈
	USkeletalMeshComponent* HandMesh = bFirstPerson ? FirstPersonMesh.Get() : GetMesh();
	HeldCameraMesh->AttachToComponent(HandMesh, FAttachmentTransformRules::KeepRelativeTransform, HeldCameraSocket);

	// 1인칭에서는 팔과 같은 방식으로 그려야 손에 붙어 보임 (시야각 / 크기 동일)
	HeldCameraMesh->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
	HeldCameraMesh->SetOwnerNoSee(false);
}

// ---------- 헤드밥 ----------

void ALCOPlayerCharacter::UpdateHeadBob(float DeltaSeconds)
{
	const bool bFirstPerson = (ViewMode == ELCOViewMode::FirstPerson);
	const bool bMoving = GetVelocity().SizeSquared2D() > FMath::Square(MinSprintVelocity)
		&& GetCharacterMovement()->IsMovingOnGround();
	const bool bShouldBob = bHeadBobEnabled && bFirstPerson && bMoving;

	// 상태별 목표 빈도 / 크기
	float TargetFrequency = WalkBobFrequency;
	float TargetAmplitude = WalkBobAmplitude;
	if (bIsCrouched)
	{
		TargetFrequency = CrouchBobFrequency;
		TargetAmplitude = CrouchBobAmplitude;
	}
	else if (bIsSprinting)
	{
		TargetFrequency = SprintBobFrequency;
		TargetAmplitude = SprintBobAmplitude;
	}

	// 걷기 → 달리기 등 상태가 바뀔 때도 부드럽게
	CurrentBobFrequency = FMath::FInterpTo(CurrentBobFrequency, TargetFrequency, DeltaSeconds, BobBlendSpeed);
	CurrentBobAmplitude = FMath::FInterpTo(CurrentBobAmplitude, TargetAmplitude, DeltaSeconds, BobBlendSpeed);
	HeadBobAlpha = FMath::FInterpTo(HeadBobAlpha, bShouldBob ? 1.f : 0.f, DeltaSeconds, BobBlendSpeed);

	// 흔들림이 꺼져 있을 때는 진행하지 않음 (다시 걸으면 이어서 흔들림)
	if (HeadBobAlpha > KINDA_SMALL_NUMBER)
	{
		HeadBobTime += DeltaSeconds * CurrentBobFrequency;
	}

	// 위아래: 걸음마다 1번 / 좌우: 두 걸음마다 1번
	const float Vertical = FMath::Sin(HeadBobTime * 2.f * PI) * CurrentBobAmplitude;
	const float Side = FMath::Sin(HeadBobTime * PI) * CurrentBobAmplitude * BobSideRatio;

	Camera->SetRelativeLocation(FVector(0.f, Side, Vertical) * HeadBobAlpha);
}

// ---------- 손전등 ----------

void ALCOPlayerCharacter::ToggleFlashlight()
{
	SetFlashlightOn(!bFlashlightOn);
}

void ALCOPlayerCharacter::SetFlashlightOn(bool bOn)
{
	const bool bChanged = (bFlashlightOn != bOn);
	bFlashlightOn = bOn;

	FlashlightComponent->SetVisibility(bOn);
	UpdateFlashlight();

	if (!bChanged)
	{
		return;
	}

	if (FlashlightToggleSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FlashlightToggleSound, FlashlightComponent->GetComponentLocation());
	}

	UE_LOG(LogTemp, Log, TEXT("[LCOPlayerCharacter] Flashlight %s"), bOn ? TEXT("On") : TEXT("Off"));
	OnFlashlightToggled.Broadcast(bOn);
}

void ALCOPlayerCharacter::UpdateFlashlight()
{
	if (!bFlashlightOn || !Controller)
	{
		return;
	}

	// 1인칭 / 3인칭 모두 화면이 바라보는 방향으로 비춤
	FlashlightComponent->SetWorldRotation(Controller->GetControlRotation());
}