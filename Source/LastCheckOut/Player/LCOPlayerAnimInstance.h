#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LCOPlayerAnimInstance.generated.h"

class ALCOPlayerCharacter;
class UCharacterMovementComponent;

// 플레이어 애니메이션 BP의 부모 클래스
// 매 프레임 캐릭터 상태(속도, 이동 방향, 웅크림, 공중 등)를 계산해 두고, Anim BP에서는 값만 가져다 쓴다
UCLASS()
class LASTCHECKOUT_API ULCOPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ---------- Anim BP에서 사용하는 값 ----------

	// 수평 이동 속도 (Blend Space 세로축)
	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	float GroundSpeed = 0.f;

	// 몸 기준 이동 방향 -180 ~ 180 (0: 앞, 90: 오른쪽, -90: 왼쪽, ±180: 뒤) (Blend Space 가로축)
	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	float Direction = 0.f;

	// 이동 입력이 있고 실제로 움직이는 중인지 (대기 / 이동 전환용)
	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	bool bShouldMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	bool bIsCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	bool bCameraEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "LCO|Anim")
	bool bIsHiding = false;

	// ---------- 설정 ----------

	// 이 속도 이상이어야 이동 중으로 판정 (미세한 미끄러짐 무시)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Anim")
	float MoveThreshold = 3.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ALCOPlayerCharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> Movement;
};