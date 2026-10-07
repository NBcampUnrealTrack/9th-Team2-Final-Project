#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LCOInteractionReceiver.h"
#include "LCOOpenableBase.generated.h"

// 열고 닫을 수 있는 상호작용 액터의 공통 부모 (문, 서랍장, 사물함, 금고 등)
// 상태(열림 / 잠금)와 열쇠 확인 로직을 담당하고, 메시와 열림 연출(Timeline 등)은 BP 자식에서 구현한다
UCLASS(Abstract, Blueprintable)
class LASTCHECKOUT_API ALCOOpenableBase : public AActor, public ILCOInteractionReceiver
{
	GENERATED_BODY()

public:
	ALCOOpenableBase();

	// ILCOInteractionReceiver
	// 잠겨 있으면 열쇠 확인, 열 수 있는 상태면 HandleUnlockedInteraction() 호출
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

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

	// 게임 시작 시 초기 상태 적용 (애니메이션 없이 열린 / 닫힌 상태로 배치)
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

	// 잠금 해제에 필요한 아이템 ID. 비워두면 열쇠로 열 수 없음 (퍼즐, 이벤트로만 해제)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Lock", meta = (EditCondition = "bIsLocked"))
	FName RequiredItemID;

	// 잠금 해제 시 열쇠 아이템을 소모할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable|Lock", meta = (EditCondition = "bIsLocked"))
	bool bConsumeKeyItem = false;

private:
	// 상호작용한 액터의 인벤토리에서 열쇠를 확인하고, 있으면 잠금 해제
	bool TryUnlockWithKey(AActor* Interactor);
};
