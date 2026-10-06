#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interaction/LCOInteractionReceiver.h"
#include "LCOPuzzleBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPuzzleSolved, ALCOPuzzleBase*, Puzzle);

// 모든 퍼즐의 부모 클래스
// 상호작용 컴포넌트(ULCOInteractComponent)로부터 입력을 전달받아 퍼즐 로직을 처리한다
UCLASS(Abstract, Blueprintable)
class LASTCHECKOUT_API ALCOPuzzleBase : public AActor, public ILCOInteractionReceiver
{
	GENERATED_BODY()

public:
	ALCOPuzzleBase();

	// ILCOInteractionReceiver
	// 퍼즐별 입력 처리. 자식 클래스(C++ / BP)에서 재정의
	virtual void HandleInteraction_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) override;

	// 해결된 퍼즐은 상호작용 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 퍼즐 해결 처리. 해결 조건을 만족했을 때 자식 클래스에서 호출
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SolvePuzzle();

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	bool IsSolved() const { return bIsSolved; }

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	FName GetPuzzleID() const { return PuzzleID; }

	// 퍼즐 해결 시 호출 (문 열기, 다음 퍼즐 활성화 등 결과 연결용)
	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FOnPuzzleSolved OnSolved;

protected:
	// 해결 시 연출용 BP 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle")
	void OnPuzzleSolved();

	// 퍼즐 고유 ID (진행도 저장 등에 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FName PuzzleID;

	// 퍼즐 해결 여부
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Puzzle")
	bool bIsSolved = false;
};
