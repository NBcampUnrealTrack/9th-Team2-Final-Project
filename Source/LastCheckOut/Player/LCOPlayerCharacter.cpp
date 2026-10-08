#include "Player/LCOPlayerCharacter.h"
#include "Player/LCOPlayerController.h"
#include "Player/LCOPlayerInteractionComponent.h"
#include "Player/LCOStaminaComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

ALCOPlayerCharacter::ALCOPlayerCharacter()
{
	// 달리기 상태 갱신용
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

	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	InteractionComponent = CreateDefaultSubobject<ULCOPlayerInteractionComponent>(TEXT("PlayerInteraction"));
	StaminaComponent = CreateDefaultSubobject<ULCOStaminaComponent>(TEXT("Stamina"));

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

	// BP에서 바꾼 값 반영
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	SetViewMode(DefaultViewMode);
}

void ALCOPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateSprint();
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
}

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
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
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

	SetSprinting(bWantsToSprint && bMoving && bOnGround && StaminaComponent->CanSprint());
}

void ALCOPlayerCharacter::SetSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;

	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	StaminaComponent->SetConsuming(bIsSprinting);

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

void ALCOPlayerCharacter::SetViewMode(ELCOViewMode NewMode)
{
	ViewMode = NewMode;
	const bool bFirstPerson = (NewMode == ELCOViewMode::FirstPerson);

	// BP에서 바꾼 카메라 높이 반영
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, CameraPivotHeight));
	SpringArm->TargetArmLength = bFirstPerson ? 0.f : ThirdPersonArmLength;
	SpringArm->SocketOffset = bFirstPerson ? FVector::ZeroVector : ThirdPersonSocketOffset;
	SpringArm->bDoCollisionTest = !bFirstPerson;

	// 1인칭: 몸이 카메라 방향으로 즉시 회전
	// 3인칭: 몸이 카메라 방향으로 부드럽게 회전 (배그 방식, 회전 속도는 RotationRate)
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = !bFirstPerson;

	// 1인칭에서는 내 화면에서 내 메시 숨김 (카메라가 머리 안에 들어가는 문제 방지)
	GetMesh()->SetOwnerNoSee(bFirstPerson);
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