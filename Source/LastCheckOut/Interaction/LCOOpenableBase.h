#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "Save/LCOSaveableActor.h"
#include "LCOOpenableBase.generated.h"

// 열고 닫을 수 있는 상호작용 액터의 공통 부모 (문, 서랍장, 사물함, 금고 등)
// 상태(열림 / 잠금)와 열쇠 확인 로직을 담당하고, 메시와 열림 연출(Timeline 등)은 BP 자식에서 구현한다
// 열림 / 잠금 상태는 맵 이동 / 세이브 시 LCOWorldStateSubsystem이 저장·복원 (ILCOSaveableActor)
UCLASS(Abstract, Blueprintable)
class LASTCHECKOUT_API ALCOOpenableBase : public AActor, public ILCOInteractionReceiver, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOOpenableBase();

	// ILCOInteractionReceiver
	// 잠겨 있으면 열쇠 확인, 열 수 있는 상태면 HandleUnlockedInteraction() 호출
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 열려 있고 다시 닫을 수 없으면(bCanClose = false) 더 할 수 있는 동작이 없으므로 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 상태별 문구: 잠김(열쇠 보유 여부에 따라) / 열림 / 닫힘
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 열림 / 잠금 상태 저장
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;

	// 열림 / 잠금 상태 복원. 연출 없이 OnInitialized로 모습만 맞춤
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	// 열기. 잠겨 있거나 이미 열려 있으면 무시
	UFUNCTION(BlueprintCallable, Category = "Openable")
	void Open();

	// 닫기. 이미 닫혀 있으면 무시
	UFUNCTION(BlueprintCallable, Category = "Openable")
	void Close();

	// 잠금 해제 (퍼즐 해결, 스토리 이벤트 등에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Openable")
	void Unlock();

	// 잠그기
	UFUNCTION(BlueprintCallable, Category = "Openable")
	void Lock();

	UFUNCTION(BlueprintPure, Category = "Openable")
	bool IsOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintPure, Category = "Openable")
	bool IsLocked() const { return bIsLocked; }

protected:
	virtual void BeginPlay() override;

	// 잠기지 않은 상태에서 상호작용했을 때의 동작. 기본은 열기 / 닫기 토글
	// 자식 클래스에서 다른 동작이 필요하면 재정의 (예: 창고는 열린 상태에서 다시 상호작용하면 UI 열기)
	virtual void HandleUnlockedInteraction(AActor* Interactor);

	// ---------- BP 연출 이벤트 ----------

	// 애니메이션 없이 열린 / 닫힌 모습으로 맞춤
	// 게임 시작(BeginPlay) 때 한 번, 맵 상태 복원(LoadActorState) 때 한 번 더 호출될 수 있음 → 여러 번 불려도 모습만 맞추도록 구현
	// Timeline으로 여닫는 BP는 Set New Time(열림 1 / 닫힘 0)으로 Timeline 위치도 같이 맞출 것 (안 맞추면 다음 Reverse / Play가 어긋남)
	UFUNCTION(BlueprintImplementableEvent, Category = "Openable")
	void OnInitialized(bool bStartOpen);

	// 열릴 때 (Timeline 열림 애니메이션 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Openable")
	void OnOpened();

	// 닫힐 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Openable")
	void OnClosed();

	// 잠금이 해제될 때 (철컥 사운드 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Openable")
	void OnUnlocked();

	// 잠긴 상태에서 상호작용했지만 열지 못했을 때 ("잠겨 있다" 피드백)
	UFUNCTION(BlueprintImplementableEvent, Category = "Openable")
	void OnLockedTry(AActor* Interactor);

	// ---------- 설정값 (BP 기본값 / 레벨 인스턴스에서 변경 가능) ----------

	// 열림 상태. 에디터에서 설정한 값이 시작 상태가 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable")
	bool bIsOpen = false;

	// 잠금 상태. 에디터에서 설정한 값이 시작 상태가 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable")
	bool bIsLocked = false;

	// 열린 상태에서 상호작용으로 다시 닫을 수 있는지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable")
	bool bCanClose = true;

	// 플레이어 상호작용 쿨타임(초). 열림 / 닫힘 애니메이션 도중 연타 방지용으로 애니메이션 길이에 맞출 것. 0이면 사용 안 함
	// 코드에서 직접 호출하는 Open() / Close() (퍼즐, 스위치, 창고 UI 등)에는 적용되지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable", meta = (ClampMin = "0.0", Units = "s"))
	float InteractionCooldown = 1.f;

	// 잠금 해제에 필요한 아이템 ID. 비워두면 열쇠로 열 수 없음 (퍼즐, 이벤트로만 해제)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Lock", meta = (EditCondition = "bIsLocked"))
	FName RequiredItemID;

	// 잠금 해제 시 열쇠 아이템을 소모할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Lock", meta = (EditCondition = "bIsLocked"))
	bool bConsumeKeyItem = false;

	// ---------- 상호작용 문구 (비워두면 LCOInteract 컴포넌트의 Interact Prompt 사용) ----------

	// 닫혀 있을 때 (예: "문 열기", "서랍 열기"). 기본값은 비어 있음 → 컴포넌트 문구 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Prompt")
	FText OpenPrompt;

	// 열려 있을 때 (예: "문 닫기")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Prompt")
	FText ClosePrompt;

	// 잠겨 있고 열쇠가 없을 때
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Prompt")
	FText LockedPrompt;

	// 잠겨 있지만 열쇠를 가지고 있을 때
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Prompt")
	FText UnlockWithKeyPrompt;

	// Interactor의 인벤토리에 열쇠(RequiredItemID)가 있는지
	bool HasKeyItem(const AActor* Interactor) const;

private:
	// 상호작용한 액터의 인벤토리에서 열쇠를 확인하고, 있으면 잠금 해제
	bool TryUnlockWithKey(AActor* Interactor);

	// 마지막으로 플레이어 상호작용을 처리한 시각 (월드 시간). 음수면 아직 없음
	float LastInteractionTime = -1.f;
};
