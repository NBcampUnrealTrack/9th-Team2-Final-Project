#include "NPC/LCONPCBase.h"

#include "GameFramework/CharacterMovementComponent.h"

ALCONPCBase::ALCONPCBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
}

void ALCONPCBase::BeginPlay()
{
	Super::BeginPlay();

	SetWalkSpeed();
}

void ALCONPCBase::SetMovementSpeed(float NewSpeed)
{
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
}

void ALCONPCBase::SetWalkSpeed()
{
	SetMovementSpeed(WalkSpeed);
}

void ALCONPCBase::SetRunSpeed()
{
	SetMovementSpeed(RunSpeed);
}
