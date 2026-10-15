#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "Save/LCOSaveableActor.h"
#include "LCOSwitch.generated.h"

class ALCOPuzzleBase;
class ALCOOpenableBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSwitchChanged, ALCOSwitch*, Switch, bool, bIsOn);

// 켜고 끄는 단순 장치 (레버, 전원 스위치, 조명 스위치, 배전반 등)
// 켜짐 상태는 맵 이동 / 세이브 시 LCOWorldStateSubsystem이 저장·복원 (ILCOSaveableActor)
// - 켜면 PuzzleTargets 활성화 / UnlockTargets 잠금 해제, 끄면 PuzzleTargets 비활성화
// - 대상(퍼즐, 문)은 각자 자기 상태를 저장하므로, 복원 시에는 대상에 다시 적용하지 않음
// - 한 번만 당기는 레버: bCanTurnOff = false
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOSwitch : public AActor, public ILCOInteractionReceiver, public ILCOSaveableActor
{
	GENERATED_BODY()

public:
	ALCOSwitch();

	// ILCOInteractionReceiver
	// 켜기 / 끄기 토글
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 켜져 있고 다시 끌 수 없으면(bCanTurnOff = false) 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 꺼져 있으면 TurnOnPrompt, 켜져 있으면 TurnOffPrompt (비어 있으면 컴포넌트 문구)
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 켜짐 상태 저장 (FLCOActorSaveState::bIsEnabled 사용)
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;

	// 켜짐 상태 복원. 연출 / 대상 적용 없이 OnSwitchInitialized로 모습만 맞춤
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	// 켜기. 이미 켜져 있으면 무시
	UFUNCTION(BlueprintCallable, Category = "Switch")
	void TurnOn();

	// 끄기. 이미 꺼져 있거나 bCanTurnOff = false면 무시
	UFUNCTION(BlueprintCallable, Category = "Switch")
	void TurnOff();

	UFUNCTION(BlueprintCallable, Category = "Switch")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "Switch")
	bool IsOn() const { return bIsOn; }

	// 실제로 켜지거나 꺼질 때 (복원 시에는 호출되지 않음)
	UPROPERTY(BlueprintAssignable, Category = "Switch")
	FOnSwitchChanged OnSwitchChanged;

protected:
	virtual void BeginPlay() override;

	// ---------- BP 연출 이벤트 ----------

	// 애니메이션 없이 켜진 / 꺼진 모습으로 맞춤 (레버 각도, 조명 등)
	// 게임 시작(BeginPlay) 때 한 번, 맵 상태 복원(LoadActorState) 때 한 번 더 호출될 수 있음 → 여러 번 불려도 모습만 맞추도록 구현
	// Timeline을 쓰는 BP는 Set New Time(켜짐 1 / 꺼짐 0)으로 Timeline 위치도 같이 맞출 것
	UFUNCTION(BlueprintImplementableEvent, Category = "Switch")
	void OnSwitchInitialized(bool bStartOn);

	// 켜질 때 (레버 당기는 애니메이션, 딸깍 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Switch")
	void OnTurnedOn();

	// 꺼질 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Switch")
	void OnTurnedOff();

	// ---------- 설정값 (BP 기본값 / 레벨 인스턴스에서 변경 가능) ----------

	// 켜짐 상태. 에디터에서 설정한 값이 시작 상태가 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	bool bIsOn = false;

	// 켠 뒤 다시 끌 수 있는지 (false: 한 번만 당기는 레버)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	bool bCanTurnOff = true;

	// ---------- 연결 대상 ----------

	// 켜면 Activate(), 끄면 Deactivate()할 퍼즐
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Switch|Target")
	TArray<TObjectPtr<ALCOPuzzleBase>> PuzzleTargets;

	// 켜면 Unlock()할 문 / 서랍장 (끌 때 다시 잠그지는 않음)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Switch|Target")
	TArray<TObjectPtr<ALCOOpenableBase>> UnlockTargets;

	// ---------- 상호작용 문구 (비워두면 LCOInteract 컴포넌트의 Interact Prompt 사용) ----------

	// 꺼져 있을 때 (예: "레버 당기기", "전원 켜기"). 기본값은 비어 있음 → 컴포넌트 문구 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch|Prompt")
	FText TurnOnPrompt;

	// 켜져 있을 때 (예: "전원 끄기")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch|Prompt")
	FText TurnOffPrompt;

private:
	// 켜짐 / 꺼짐에 따라 연결 대상에 적용 (실제로 상태가 바뀔 때만)
	void ApplyTargets();
};
