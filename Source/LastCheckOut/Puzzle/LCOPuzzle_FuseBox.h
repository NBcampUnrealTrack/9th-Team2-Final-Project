#pragma once

#include "CoreMinimal.h"
#include "Puzzle/LCOPuzzleBase.h"
#include "LCOPuzzle_FuseBox.generated.h"

class ALCOElevator;
class ULCOInteractComponent;
class UMaterialInterface;

// 고스트로 바꾸기 전 칸 메시의 원래 모습 (퓨즈를 끼울 때 복원)
USTRUCT()
struct FLCOFuseSlotOriginalLook
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	bool bCastShadow = true;
};

// 배전반 퍼즐: 빈 퓨즈 칸에 퓨즈 아이템을 끼워서 모두 채우면 해결
// - 퓨즈 칸 = LCOInteract 컴포넌트, InputID = FuseSlotIDs 중 하나 (예: Slot1, Slot2)
//   칸 컴포넌트에 퓨즈 메시(SM_Fuse)를 넣어두면, 비어 있을 때는 숨기고 끼우면 보이게 함 (bAutoShowFuseMesh)
//   GhostMaterial을 지정하면 숨기는 대신 반투명 퓨즈(고스트)로 보여주고, 끼우면 원래 머티리얼로 되돌림
//   숨겨진 상태에서도 콜리전은 남아 있어서 F키로 감지됨
// - 칸별 표시등: Component Tags에 칸 InputID를 넣은 메시는 빈 칸이면 LightOffMaterial, 끼우면 LightOnMaterial
// - 빈 칸에 상호작용 → 인벤토리에 FuseItemID가 있으면 1개 소모하고 끼움 / 없으면 MissingFusePrompt 표시
// - 이미 끼운 칸은 상호작용 잠김. 모든 칸을 채우면 SolvePuzzle → PoweredElevators 전원 켜기 + 기본 해결 결과(UnlockTargets, StoryResult)
// - 끼운 칸은 저장됨 (퓨즈를 소모하므로 일부만 끼운 상태도 맵 이동 / 이어하기 후 유지)
// - FuseSlotIDs에 없는 InputID(문 등)는 OnPuzzleInteracted로 BP에서 처리
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOPuzzle_FuseBox : public ALCOPuzzleBase
{
	GENERATED_BODY()

public:
	ALCOPuzzle_FuseBox();

	// ILCOInteractionReceiver
	// 기본 잠금 + 이미 끼운 칸이면 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 빈 칸인데 퓨즈가 없으면 MissingFusePrompt, 그 외에는 기본 문구
	virtual FText GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// ILCOSaveableActor
	// 활성 상태 + 끼운 칸 목록 (Items의 ItemID = 칸 InputID)
	virtual FLCOActorSaveState SaveActorState_Implementation() const override;
	virtual void LoadActorState_Implementation(const FLCOActorSaveState& State) override;

	UFUNCTION(BlueprintPure, Category = "Puzzle|FuseBox")
	bool IsSlotFilled(FName SlotID) const { return FilledSlots.Contains(SlotID); }

	UFUNCTION(BlueprintPure, Category = "Puzzle|FuseBox")
	int32 GetFilledCount() const { return FilledSlots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Puzzle|FuseBox")
	int32 GetSlotCount() const { return FuseSlotIDs.Num(); }

protected:
	virtual void BeginPlay() override;

	// 퓨즈 칸이면 퓨즈 끼우기 시도
	virtual void HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor) override;

	// 해결 상태로 복원될 때 모든 칸을 끼운 모습으로
	virtual void HandleRestoredAsSolved() override;

	// ---------- BP 연출 이벤트 ----------

	// 퓨즈를 끼웠을 때 (딸깍 소리, 불빛 등). bInstant: 저장 복원이라 연출 없이 모습만 맞출 때 true
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|FuseBox")
	void OnFuseInserted(FName SlotID, ULCOInteractComponent* Slot, bool bInstant);

	// 퓨즈 없이 빈 칸에 상호작용했을 때 (안내 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|FuseBox")
	void OnFuseMissing(FName SlotID, ULCOInteractComponent* Slot);

	// ---------- 설정값 ----------

	// 퓨즈 칸 InputID 목록. 모두 채우면 해결
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|FuseBox")
	TArray<FName> FuseSlotIDs;

	// 끼울 때 소모하는 아이템 ID (DT_ItemData에 등록된 ID)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|FuseBox")
	FName FuseItemID = TEXT("Fuse");

	// 칸 컴포넌트의 메시를 비어 있을 때 숨기고, 끼우면 보이게 할지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|FuseBox")
	bool bAutoShowFuseMesh = true;

	// 빈 칸 퓨즈 메시에 씌울 반투명 머티리얼 (bAutoShowFuseMesh일 때). 비우면 빈 칸은 숨김
	// 바라볼 때 외곽선이 보이려면 머티리얼의 Allow Custom Depth Writes를 켤 것
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|FuseBox")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	// 칸별 표시등: Component Tags에 칸 InputID(예: Slot1)를 넣은 메시 컴포넌트
	// 그 메시의 머티리얼 중 LightOffMaterial / LightOnMaterial인 슬롯만 교체 (몸체 머티리얼은 그대로)
	// 시작 시 빈 칸은 Off, 퓨즈를 끼우면 On. 둘 중 하나라도 비우면 사용 안 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|FuseBox|Light")
	TObjectPtr<UMaterialInterface> LightOffMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|FuseBox|Light")
	TObjectPtr<UMaterialInterface> LightOnMaterial;

	// 해결 시 전원을 켤 엘리베이터 (엘리베이터가 전원 상태를 따로 저장)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Puzzle|Result")
	TArray<TObjectPtr<ALCOElevator>> PoweredElevators;

	// 빈 칸인데 퓨즈가 없을 때
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Prompt")
	FText MissingFusePrompt;

private:
	// 칸 InputID로 컴포넌트 찾기
	ULCOInteractComponent* FindSlot(FName SlotID) const;

	bool HasFuse(const AActor* Interactor) const;

	// 칸을 채운 상태로 표시 (상태 기록 + 메시 표시 + BP 이벤트)
	void FillSlot(FName SlotID, bool bInstant);

	// 칸 표시등 켜기 / 끄기 (태그가 SlotID인 메시 컴포넌트)
	void SetSlotLight(FName SlotID, bool bOn);

	// 빈 칸 모습: GhostMaterial이 있으면 고스트, 없으면 숨김
	void ShowEmptySlot(FName SlotID, ULCOInteractComponent* Slot);

	// 끼운 칸 모습: 고스트였으면 원래 머티리얼 / 그림자 복원 후 표시
	void ShowFilledSlot(FName SlotID, ULCOInteractComponent* Slot);

	// 해결 시 엘리베이터 전원 켜기
	void PowerElevators();

	TSet<FName> FilledSlots;

	// 고스트로 바꾼 칸의 원래 모습
	UPROPERTY()
	TMap<FName, FLCOFuseSlotOriginalLook> GhostedSlots;
};
