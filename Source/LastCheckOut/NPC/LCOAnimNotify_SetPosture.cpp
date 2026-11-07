#include "NPC/LCOAnimNotify_SetPosture.h"

#include "Components/SkeletalMeshComponent.h"

void ULCOAnimNotify_SetPosture::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// 에디터 미리보기에는 NPC가 없으므로 캐스팅이 실패하면 아무것도 하지 않는다.
	if (ALCONPCBase* NPC = MeshComp ? Cast<ALCONPCBase>(MeshComp->GetOwner()) : nullptr)
	{
		NPC->SetPosture(Posture);
	}
}

FString ULCOAnimNotify_SetPosture::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("SetPosture: %s"), Posture == ENPCPosture::Sitting ? TEXT("Sitting") : TEXT("Standing"));
}
