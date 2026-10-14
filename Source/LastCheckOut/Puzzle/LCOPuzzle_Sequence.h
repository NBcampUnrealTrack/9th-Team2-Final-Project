#pragma once

#include "CoreMinimal.h"
#include "Puzzle/LCOPuzzleBase.h"
#include "LCOPuzzle_Sequence.generated.h"

// 버튼 순서 퍼즐
// 버튼(LCOInteract 컴포넌트)마다 InputID를 지정하고, CorrectSequence 순서대로 누르면 해결
// 예: 버튼 InputID = Red / Green / Blue, CorrectSequence = [Red, Blue, Green]
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOPuzzle_Sequence : public ALCOPuzzleBase
{
	GENERATED_BODY()

public:
	// 지금까지 입력한 순서
	UFUNCTION(BlueprintPure, Category = "Puzzle|Sequence")
	const TArray<FName>& GetCurrentInput() const { return CurrentInput; }

	UFUNCTION(BlueprintPure, Category = "Puzzle|Sequence")
	int32 GetSequenceLength() const { return CorrectSequence.Num(); }

protected:
	virtual void BeginPlay() override;

	// 입력을 기록하고 정답 여부 판정
	virtual void HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor) override;

	// 입력한 순서 비우기
	virtual void OnPuzzleReset_Implementation() override;

	// 입력이 기록됐을 때 (버튼 불 켜기, 딸깍 소리 등). InputIndex는 0부터 시작하는 입력 순번
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Sequence")
	void OnSequenceInput(FName InputID, int32 InputIndex);

	// 정답 순서 (InputID 목록). 같은 버튼을 여러 번 넣어도 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Sequence")
	TArray<FName> CorrectSequence;

	// true: 틀린 버튼을 누르는 즉시 실패 / false: 정답 길이만큼 다 누른 뒤에 판정 (어디서 틀렸는지 알 수 없어 더 어려움)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Sequence")
	bool bFailImmediately = true;

private:
	TArray<FName> CurrentInput;
};
