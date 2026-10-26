#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "LCONPCBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCConversationStarted, AActor*, Partner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCConversationEnded);

class ALCOActivitySpot;
class UBehaviorTree;

// 호텔 NPC 공용 베이스 (관리인, 직원 등이 상속)
// 대화 가능한 NPC는 상호작용(F)을 받으면 이동을 멈추고 플레이어를 바라본다
UCLASS()
class LASTCHECKOUT_API ALCONPCBase : public ACharacter, public ILCOInteractionReceiver
{
	GENERATED_BODY()

#pragma region 초기화와 이동 속도
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

	// 현재 속도에 곱하는 배율(문 통과 같은 구간에서 일시적으로 느리게). SetWalkSpeed/SetRunSpeed로 속도가 바뀌어도 유지된다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Movement")
	void SetMoveSpeedScale(float NewScale);

protected:
	virtual void BeginPlay() override;

	virtual void PossessedBy(AController* NewController) override;

	// 대화 DataTable 등에서 NPC를 식별하는 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	float WalkSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Movement")
	float RunSpeed = 450.f;

private:
	// 배율을 곱하기 전의 속도와 현재 배율
	float BaseMoveSpeed = 0.f;
	float MoveSpeedScale = 1.f;

protected:
#pragma endregion

#pragma region 들고 있는 물건
public:
	// 물건(예: 짐 카트)을 NPC 앞에 붙여 함께 움직이게 한다. Offset은 NPC 기준 위치(Z는 발바닥 기준), Yaw는 정면에서 더 돌릴 각도이다.
	// 붙어 있는 동안 물건의 충돌은 꺼서 NPC나 플레이어가 막히지 않게 한다. 이미 들고 있으면 먼저 놓는다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Prop")
	void AttachCarriedProp(AActor* Prop, FVector Offset, float Yaw);

	// 들고 있던 물건을 현재 위치에 놓는다. 충돌을 다시 켠다.
	UFUNCTION(BlueprintCallable, Category = "NPC|Prop")
	void DropCarriedProp();

	// 들고 있던 물건을 지정한 위치와 방향에 놓는다. 충돌을 다시 켠다.
	void DropCarriedPropAt(const FVector& Location, const FRotator& Rotation);

	UFUNCTION(BlueprintPure, Category = "NPC|Prop")
	AActor* GetCarriedProp() const { return CarriedProp.Get(); }

protected:
	// 물건을 들고(밀고) 있는 동안의 이동 속도 배율. 무거운 것을 미는 느낌으로 느리게 걷는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Prop", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float CarriedPropSpeedScale = 0.7f;

private:
	TWeakObjectPtr<AActor> CarriedProp;
#pragma endregion

#pragma region 대화
public:
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

	// 플레이어가 관리인에게 쫓기는 중인지 (체이싱 중에는 F 대화를 막는다)
	bool IsPlayerBeingChased() const;

protected:
	virtual void Tick(float DeltaSeconds) override;

	// 대화가 가능한 NPC인지 (관리인은 false)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Conversation")
	bool bCanTalk = false;

	// 대화 상대를 향해 몸을 돌리는 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Conversation")
	float TurnInterpSpeed = 6.f;

private:
	TWeakObjectPtr<AActor> ConversationPartner;

	bool bInConversation = false;
#pragma endregion

#pragma region 루틴 (활동 지점)
public:
	// 루틴 목록에서 다음 활동 지점을 돌려준다. 끝에 도달하면 처음으로 돌아가고, 목록이 비어 있으면 nullptr이다.
	ALCOActivitySpot* GetNextRoutineSpot();

	// 루틴(활동 지점 일)을 켜고 끈다. 꺼져 있으면 Do Activity 태스크가 바로 실패해 BT의 다음 분기로 넘어간다.
	// 예: 관리인의 복도 정비는 2막 의심 단계에서만 켠다.
	bool IsRoutineEnabled() const { return bRoutineEnabled; }

	UFUNCTION(BlueprintCallable, Category = "NPC|Routine")
	void SetRoutineEnabled(bool bEnabled) { bRoutineEnabled = bEnabled; }

protected:
	// AI가 빙의되면 자동으로 실행할 Behavior Tree. 루틴으로 움직이는 NPC에 지정한다. (관리인은 컨트롤러가 직접 실행하므로 비워 둔다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Routine")
	TObjectPtr<UBehaviorTree> DefaultBehaviorTree;

	// 이 NPC가 순서대로 돌며 할 일을 하는 활동 지점 목록. 레벨에 배치한 인스턴스에서 지정한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "NPC|Routine")
	TArray<TObjectPtr<ALCOActivitySpot>> Routine;

	// 루틴을 실행할지 여부. 시작할 때 꺼 두었다가 조건이 되면 SetRoutineEnabled(true)로 켠다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Routine")
	bool bRoutineEnabled = true;

private:
	int32 RoutineIndex = 0;
#pragma endregion

#pragma region 머리 위 동작 표시 (디버그)
public:
	// 머리 위에 표시할 디버그 문구를 지정한다. (루틴 태스크가 현재 동작을 알린다)
	void SetDebugStatus(const FString& Text) { DebugStatus = Text; }

protected:
	// NPC 머리 위에 현재 동작(DebugStatus)을 표시한다. 몽타주가 없는 동안 동작을 확인하는 용도이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Debug")
	bool bDrawDebugStatus = true;

private:
	FString DebugStatus;
#pragma endregion
};
