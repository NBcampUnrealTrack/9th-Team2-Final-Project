#pragma once

#include "CoreMinimal.h"
#include "Interaction/LCOContainer.h"
#include "LCODrawerContainer.generated.h"

// 2단계 창고 (서랍을 열고 → 안의 물건(서류철 등)을 조사해야 창고 UI가 열림)
// - 물건 컴포넌트: LCOInteract의 InputID = ContentsInputID (기본 "Contents"). 서랍이 열려 있을 때만 상호작용 가능 → 창고 UI 열기(OnContainerOpened)
// - 그 외 LCOInteract 컴포넌트(서랍 손잡이 / 앞판 등, InputID 비워도 됨): 서랍 열기 / 닫기
// - 서랍 연출은 OnDrawerOpened / OnDrawerClosed / OnDrawerInitialized에서 구현
//   (부모의 OnOpened / OnClosed / OnInitialized는 "창고 UI 사용 중" 상태이므로 서랍 애니메이션에 쓰지 말 것)
// - 잠금(bIsLocked / 열쇠)은 서랍에 적용됨
// - 저장: 서랍 열림 / 잠금 / 남은 아이템 (창고 UI는 항상 닫힌 상태로 복원)
// - 퍼즐 UnlockTargets에 넣을 때 bOpenTargetsOnSolve는 쓰지 말 것 (Open()은 창고 UI 상태를 바꿈. 서랍을 열려면 OpenDrawer)
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCODrawerContainer : public ALCOContainer
{
	GENERATED_BODY()

public:
	ALCODrawerContainer();

	// ILCOInteractionReceiver
	// 어떤 컴포넌트로 상호작용했는지 기록한 뒤 부모 처리(쿨타임 / 잠금 / 열쇠)로 전달
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 물건: 서랍이 닫혀 있으면 잠금 / 공통: 창고 UI 사용 중이면 잠금 / 서랍: 열려 있고 bCanClose가 false면 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 물건이면 ContentsPrompt, 서랍이면 DrawerOpenPrompt / DrawerClosePrompt (잠겨 있으면 잠김 문구)
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 부모 저장값의 bIsOpen 자리에 서랍 열림 상태를 저장 (창고 UI 열림은 저장할 필요 없음)
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;

	// 서랍 열림 복원 + 부모 복원(잠금 / 남은 아이템, 창고 UI는 닫힘)
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	// 서랍 열기. 잠겨 있거나 이미 열려 있으면 무시
	UFUNCTION(BlueprintCallable, Category = "Container|Drawer")
	void OpenDrawer();

	// 서랍 닫기. 이미 닫혀 있거나 창고 UI 사용 중이면 무시
	UFUNCTION(BlueprintCallable, Category = "Container|Drawer")
	void CloseDrawer();

	UFUNCTION(BlueprintPure, Category = "Container|Drawer")
	bool IsDrawerOpen() const { return bDrawerOpen; }

protected:
	virtual void BeginPlay() override;

	// 물건이면 창고 UI 열기(부모), 그 외에는 서랍 열기 / 닫기
	virtual void HandleUnlockedInteraction(AActor* Interactor) override;

	// ---------- BP 연출 이벤트 ----------

	// 애니메이션 없이 서랍 열린 / 닫힌 모습으로 맞춤 (BeginPlay, 저장 복원 때. 여러 번 불려도 모습만 맞추도록 구현)
	// Timeline을 쓰면 Set New Time(열림 1 / 닫힘 0)으로 위치도 같이 맞출 것
	UFUNCTION(BlueprintImplementableEvent, Category = "Container|Drawer")
	void OnDrawerInitialized(bool bOpen);

	// 서랍이 열릴 때 (Timeline 열림 애니메이션, 소리)
	UFUNCTION(BlueprintImplementableEvent, Category = "Container|Drawer")
	void OnDrawerOpened();

	// 서랍이 닫힐 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Container|Drawer")
	void OnDrawerClosed();

	// ---------- 설정값 ----------

	// 서랍 시작 상태
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Drawer")
	bool bDrawerOpen = false;

	// 서랍 안 물건(서류철 등) LCOInteract 컴포넌트의 InputID. 이 InputID의 컴포넌트가 창고 UI를 연다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Drawer")
	FName ContentsInputID = TEXT("Contents");

	// 서랍이 닫혀 있을 때 문구 (비우면 컴포넌트 문구)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Drawer")
	FText DrawerOpenPrompt;

	// 서랍이 열려 있을 때 문구 (비우면 컴포넌트 문구)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Drawer")
	FText DrawerClosePrompt;

	// 물건 문구 (비우면 컴포넌트 문구)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Drawer")
	FText ContentsPrompt;

private:
	bool IsContentsComponent(const ULCOInteractComponent* Component) const;

	// HandleInteraction 처리 중인 컴포넌트가 물건인지 (HandleUnlockedInteraction에는 컴포넌트가 넘어오지 않으므로 기록)
	bool bHandlingContents = false;
};
