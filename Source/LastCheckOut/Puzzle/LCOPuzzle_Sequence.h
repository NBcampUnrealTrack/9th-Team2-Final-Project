#pragma once

#include "CoreMinimal.h"
#include "Puzzle/LCOPuzzleBase.h"
#include "LCOPuzzle_Sequence.generated.h"

// 버튼 순서 퍼즐
// 버튼(LCOInteract 컴포넌트)마다 InputID를 지정하고, CorrectSequence 순서대로 누르면 해결
// 예: 버튼 InputID = Red / Green / Blue, CorrectSequence = [Red, Blue, Green]
// 키패드: 지우기 버튼(ClearInputID) / 확인 버튼(ConfirmInputID)을 지원
//         bRequireConfirm이면 정답 길이만큼 입력해도 자동 판정하지 않고, 확인 버튼을 눌러야 판정
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

	// 지우기 버튼으로 입력을 비웠을 때 (표시창 비우기, 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Sequence")
	void OnSequenceCleared();

	// 입력이 무시됐을 때 (bRequireConfirm에서 자릿수가 다 찼는데 더 누름 / 아무것도 입력하지 않고 확인). 짧은 에러음 등
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Sequence")
	void OnSequenceInputRejected(FName InputID);

	// 정답 순서 (InputID 목록). 같은 버튼을 여러 번 넣어도 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Sequence")
	TArray<FName> CorrectSequence;

	// true: 틀린 버튼을 누르는 즉시 실패 / false: 정답 길이만큼 다 누른 뒤에 판정 (어디서 틀렸는지 알 수 없어 더 어려움)
	// bRequireConfirm이면 사용하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Sequence", meta = (EditCondition = "!bRequireConfirm"))
	bool bFailImmediately = true;

	// true: 정답 길이만큼 입력해도 자동 판정하지 않고, 확인 버튼(ConfirmInputID)을 눌러야 판정 (키패드)
	// 정답 길이를 넘는 입력은 무시 (OnSequenceInputRejected)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Sequence")
	bool bRequireConfirm = false;

	// 이 InputID의 버튼을 누르면 입력을 모두 지움 (None이면 사용 안 함). CorrectSequence에 넣지 말 것
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Sequence")
	FName ClearInputID = TEXT("Clear");

	// 이 InputID의 버튼을 누르면 정답 판정 (bRequireConfirm일 때만 사용). CorrectSequence에 넣지 말 것
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Sequence", meta = (EditCondition = "bRequireConfirm"))
	FName ConfirmInputID = TEXT("Enter");

private:
	// 지금까지 입력이 정답과 같으면 해결, 아니면 입력을 비우고 실패
	void CheckAnswer();

	TArray<FName> CurrentInput;
};
