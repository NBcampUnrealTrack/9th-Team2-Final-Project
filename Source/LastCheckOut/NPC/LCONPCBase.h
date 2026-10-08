#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "LCONPCBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCConversationStarted, AActor*, Partner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCConversationEnded);

// 호텔 NPC 공용 베이스 (관리인, 직원 등이 상속)
// 대화 가능한 NPC는 상호작용(F)을 받으면 이동을 멈추고 플레이어를 바라본다
UCLASS()
class LASTCHECKOUT_API ALCONPCBase : public ACharacter, public ILCOInteractionReceiver
{
	GENERATED_BODY()

public:
	ALCONPCBase();

	FName GetNPCId() const { return NPCId; }

	// 현재 이동 속도(cm/s)를 변경한다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetMovementSpeed(float NewSpeed);

	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetWalkSpeed();

	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetRunSpeed();

	// 대화를 시작한다. AI를 멈추고 대화 상대를 바라본다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Conversation")
	void BeginConversation(AActor* Partner);

	// 대화를 끝낸다. 대화 UI가 닫힐 때 호출하며, AI가 이어서 동작한다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Conversation")
	void EndConversation();

	UFUNCTION(BlueprintPure, Category = "NPC|Conversation")
	bool IsInConversation() const { return bInConversation; }

	// 대화가 시작될 때 호출 (대화 UI/시스템이 바인딩)
	UPROPERTY(BlueprintAssignable, Category = "NPC|Conversation")
	FOnNPCConversationStarted OnConversationStarted;

	// 대화가 끝날 때 호출
	UPROPERTY(BlueprintAssignable, Category = "NPC|Conversation")
	FOnNPCConversationEnded OnConversationEnded;

	// ILCOInteractionReceiver
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 대화 DataTable 등에서 NPC를 식별하는 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	float WalkSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	float RunSpeed = 450.f;

	// 대화가 가능한 NPC인지 (관리인은 false)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Conversation")
	bool bCanTalk = false;

	// 대화 상대를 향해 몸을 돌리는 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Conversation")
	float TurnInterpSpeed = 6.f;

private:
	TWeakObjectPtr<AActor> ConversationPartner;

	bool bInConversation = false;
};
