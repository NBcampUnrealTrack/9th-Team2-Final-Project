#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOSafeZoneVolume.generated.h"

class UBoxComponent;

// 안전지대(로비 등). 플레이어가 들어오면 진행 중인 체이싱을 종료하고, 안에 있는 동안 체이싱과 잡기가 동작하지 않는다.
// 레벨에 배치하고 박스 크기를 영역에 맞춘다.
UCLASS()
class LASTCHECKOUT_API ALCOSafeZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	ALCOSafeZoneVolume();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SafeZone")
	TObjectPtr<UBoxComponent> Zone;
};
