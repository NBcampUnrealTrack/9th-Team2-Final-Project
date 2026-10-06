#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LCONPCBase.generated.h"

/* 호텔 NPC 공용 베이스 (관리인, 직원 등이 상속) */
UCLASS()
class LASTCHECKOUT_API ALCONPCBase : public ACharacter
{
	GENERATED_BODY()

public:
	ALCONPCBase();

	FName GetNPCId() const { return NPCId; }

	/** 현재 이동 속도(cm/s)를 변경한다. */
	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetMovementSpeed(float NewSpeed);

	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetWalkSpeed();

	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetRunSpeed();

protected:
	virtual void BeginPlay() override;

	/** 대화 DataTable 등에서 NPC를 식별하는 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	float WalkSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	float RunSpeed = 450.f;
};
