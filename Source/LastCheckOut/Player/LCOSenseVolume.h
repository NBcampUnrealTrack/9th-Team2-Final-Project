#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LCOSenseVolume.generated.h"

class UBoxComponent;

// 감각(가칭) 성공 영역. 플레이어가 이 박스 안에서 감각을 사용하면 성공으로 판정한다
// 레벨에 배치하고 박스 크기를 영역에 맞춘다
UCLASS()
class LASTCHECKOUT_API ALCOSenseVolume : public AActor
{
	GENERATED_BODY()

public:
	ALCOSenseVolume();

	// Location이 박스 안에 있는지 (회전 / 스케일 반영)
	UFUNCTION(BlueprintPure, Category = "LCO|Sense")
	bool IsLocationInside(const FVector& Location) const;

	UFUNCTION(BlueprintPure, Category = "LCO|Sense")
	bool IsSenseEnabled() const { return bSenseEnabled; }

	// 스토리 진행에 따라 켜고 끌 때 사용
	UFUNCTION(BlueprintCallable, Category = "LCO|Sense")
	void SetSenseEnabled(bool bEnabled) { bSenseEnabled = bEnabled; }

	// 이 볼륨 안에서 감각을 성공했을 때 캐릭터가 호출
	void NotifySensed(AActor* Sensor);

protected:
	// 볼륨별 연출(단서 강조 등)은 BP에서 구현
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Sense")
	void OnSensed(AActor* Sensor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sense")
	TObjectPtr<UBoxComponent> Zone;

	// false면 안에서 사용해도 실패 처리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense")
	bool bSenseEnabled = true;

	// 이 볼륨에서 감각 성공 시 띄울 독백 (UI.csv 키). 비우면 DefaultMonologueKey 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense|Monologue")
	FName MonologueKey;

	// 볼륨별 문구가 없을 때 기본 성공 독백 (None이면 표시 안 함)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sense|Monologue")
	FName DefaultMonologueKey = TEXT("MONOLOGUE_SENSE_SUCCESS");
};