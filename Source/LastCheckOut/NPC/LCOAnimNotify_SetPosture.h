#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "NPC/LCONPCBase.h"
#include "LCOAnimNotify_SetPosture.generated.h"

// 몽타주의 원하는 지점에서 NPC의 기본 자세(서기/앉기)를 바꾼다.
// 앉기 몽타주는 끝 근처에 Sitting, 일어서기 몽타주는 시작에 Standing을 두면 몽타주가 블렌드 아웃되기 전에 자세가 바뀌어 튀지 않는다.
UCLASS(meta = (DisplayName = "NPC 자세 변경"))
class LASTCHECKOUT_API ULCOAnimNotify_SetPosture : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	// 이 노티파이 시점에 바꿀 자세
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Posture")
	ENPCPosture Posture = ENPCPosture::Sitting;
};
