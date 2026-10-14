#pragma once

#include "CoreMinimal.h"
#include "Puzzle/LCOPuzzleBase.h"
#include "LCOPuzzle_AnswerInput.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnswerInputRequestedSignature, ALCOPuzzle_AnswerInput*, Puzzle, AActor*, User);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnswerInputClosedSignature, ALCOPuzzle_AnswerInput*, Puzzle, bool, bSolved);

// UI에서 답을 입력하거나 풀어서 해결하는 퍼즐
// 상호작용 → 입력창 열기 이벤트(OnAnswerInputRequested) → UI에서 결과 전달 / CancelAnswerInput() 호출
// 판정 방식 (bUIJudgesAnswer)
// - false: 정답 비교 (번호 자물쇠, 비밀번호, 단어 맞추기). UI는 SubmitAnswer(입력값)만 호출하고, 정답은 퍼즐의 CorrectAnswer에 둔다
// - true : UI 판정 (선 연결, 그림 맞추기 등 UI 미니게임). UI가 직접 판정해서 ReportSolved() / ReportFailed() 호출
// 정답 비교 방식을 바꾸려면 CheckAnswer를 재정의
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOPuzzle_AnswerInput : public ALCOPuzzleBase
{
	GENERATED_BODY()

public:
	// 입력창이 열려 있는 동안 잠금
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// [정답 비교] 입력한 답 제출. 정답이면 입력창을 닫고 해결 후 true, 오답이면 FailPuzzle() 후 false (입력창은 열린 채로 유지)
	// UI 판정 방식(bUIJudgesAnswer)에서는 사용 불가 (false 반환)
	UFUNCTION(BlueprintCallable, Category = "Puzzle|AnswerInput")
	bool SubmitAnswer(const FString& Answer);

	// [UI 판정] UI에서 퍼즐을 풀었을 때 호출. 입력창을 닫고 해결 후 true (입력창이 닫혀 있거나 정답 비교 방식이면 false)
	UFUNCTION(BlueprintCallable, Category = "Puzzle|AnswerInput")
	bool ReportSolved();

	// [UI 판정] UI에서 틀렸을 때 호출. FailPuzzle() 처리 (입력창은 열린 채로 유지)
	UFUNCTION(BlueprintCallable, Category = "Puzzle|AnswerInput")
	void ReportFailed();

	// UI가 직접 판정하는 퍼즐인지 (UI에서 어떤 함수를 호출할지 구분용)
	UFUNCTION(BlueprintPure, Category = "Puzzle|AnswerInput")
	bool IsJudgedByUI() const { return bUIJudgesAnswer; }

	// 입력 취소 (입력창 닫기)
	UFUNCTION(BlueprintCallable, Category = "Puzzle|AnswerInput")
	void CancelAnswerInput();

	UFUNCTION(BlueprintPure, Category = "Puzzle|AnswerInput")
	bool IsAnswerInputOpen() const { return InputUser.IsValid(); }

	// 정답 글자 수 (입력칸 개수 표시용. 정답 자체는 노출하지 않음)
	UFUNCTION(BlueprintPure, Category = "Puzzle|AnswerInput")
	int32 GetAnswerLength() const { return CorrectAnswer.Len(); }

	// 입력창을 열어야 할 때 (UI 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "Puzzle|AnswerInput")
	FOnAnswerInputRequestedSignature OnAnswerInputRequestedDelegate;

	// 입력창을 닫아야 할 때. bSolved: 정답으로 닫혔는지 (false면 취소)
	UPROPERTY(BlueprintAssignable, Category = "Puzzle|AnswerInput")
	FOnAnswerInputClosedSignature OnAnswerInputClosedDelegate;

protected:
	virtual void BeginPlay() override;

	// 입력창 열기
	virtual void HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor) override;

	// 입력한 답이 정답인지 판정. 기본은 CorrectAnswer와 비교 (bIgnoreCase, 앞뒤 공백 무시)
	UFUNCTION(BlueprintNativeEvent, Category = "Puzzle|AnswerInput")
	bool CheckAnswer(const FString& Answer) const;

	// ---------- BP 이벤트 ----------

	// 입력창 열기. 여기서 입력 UI를 띄운다
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|AnswerInput")
	void OnAnswerInputRequested(AActor* User);

	// 입력창 닫기 (정답 / 취소 공통). 입력 UI 정리용
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|AnswerInput")
	void OnAnswerInputClosed(bool bSolved);

	// ---------- 설정값 ----------

	// true면 UI가 직접 판정 (ReportSolved / ReportFailed), false면 CorrectAnswer와 비교 (SubmitAnswer)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|AnswerInput")
	bool bUIJudgesAnswer = false;

	// 정답 (예: "1981", "별관")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|AnswerInput", meta = (EditCondition = "!bUIJudgesAnswer"))
	FString CorrectAnswer;

	// 대소문자 무시 (영문 답일 때)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|AnswerInput", meta = (EditCondition = "!bUIJudgesAnswer"))
	bool bIgnoreCase = true;

private:
	// 입력창을 닫고 퍼즐 해결 (정답 비교 / UI 판정 공통)
	void SolveFromInput();

	// 입력창 상태 해제 + 닫기 이벤트
	void CloseAnswerInput(bool bSolved);

	// 입력창을 연 액터 (닫혀 있으면 nullptr)
	TWeakObjectPtr<AActor> InputUser;
};
