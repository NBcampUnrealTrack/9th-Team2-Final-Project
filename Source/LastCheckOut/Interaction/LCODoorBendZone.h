#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCODoorBendZone.generated.h"

class UBoxComponent;

// 문 통과 구간. 관리인이 이 영역 안에 있는 동안 허리를 숙이고 천천히 이동한다. (덩치가 문보다 커서 숙이고 지나가는 연출)
// 문 앞뒤에 걸치도록 레벨에 배치하고 박스 크기를 문 폭/깊이에 맞춘다.
UCLASS()
class LASTCHECKOUT_API ALCODoorBendZone : public AActor
{
	GENERATED_BODY()

public:
	ALCODoorBendZone();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DoorBend")
	TObjectPtr<UBoxComponent> Zone;
};
