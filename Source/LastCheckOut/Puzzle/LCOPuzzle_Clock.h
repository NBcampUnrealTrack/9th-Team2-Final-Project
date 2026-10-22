#pragma once

#include "CoreMinimal.h"
#include "Puzzle/LCOPuzzleBase.h"
#include "LCOPuzzle_Clock.generated.h"

class UCameraComponent;
class APlayerController;
class ULCOInteractComponent;

// 괘종시계 퍼즐
// - 시계(LCOInteract)에 상호작용 → 시계 앞 카메라(ViewCamera)로 화면 고정, 이동 / 시점 입력 차단, 마우스 커서 표시
// - 보기 화면에서 시침 / 분침(LCOInteract, InputID = HourHandID / MinuteHandID)을 클릭해서 돌림 (Shift + 클릭: 반대로)
//   시침과 분침은 따로 움직임 (분침을 돌려도 시침은 그대로, 시침은 정각 눈금 단위로 이동)
// - 확정 버튼(LCOInteract, InputID = ConfirmID)을 클릭하면 정답 판정 → 정답이면 SolvePuzzle, 오답이면 FailPuzzle
// - 그 밖에 InputID가 있는 부품(뚜껑 등)을 클릭하면 OnClockPartClicked로 BP에 전달
//   (예: 뚜껑을 열어야 안쪽 진자(확정)가 보이고 클릭되는 구조. 닫힌 뚜껑이 앞을 막으므로 진자는 열렸을 때만 클릭됨)
// - 바늘 회전 애니메이션은 C++에서 처리: 회전하는 동안 매 프레임 OnClockTimeChanged로 현재 각도를 보냄 → BP는 그 각도로 Pivot만 회전
// - 나가기: ESC(옵션 키, LCOPlayerController의 퍼즐 보기 상태 사용) 또는 우클릭
//   퍼즐 보기 중에는 인벤토리 / 수첩 키가 무시되고, ESC를 눌러도 일시정지 메뉴가 열리지 않음
UCLASS(Blueprintable)
class LASTCHECKOUT_API ALCOPuzzle_Clock : public ALCOPuzzleBase
{
	GENERATED_BODY()

public:
	ALCOPuzzle_Clock();

	virtual void Tick(float DeltaTime) override;

	// 보기 중에는 월드 상호작용 잠금 (시계 화면에서 화면 가운데 컴포넌트가 포커스되는 것 방지)
	virtual bool IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const override;

	// 시계 보기 종료 (나가기 입력, 해결 후, 관리인에게 잡혔을 때 등 외부에서도 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Clock")
	void ExitClockView();

	UFUNCTION(BlueprintPure, Category = "Puzzle|Clock")
	bool IsViewing() const { return ViewingController != nullptr; }

	// 현재 시침이 가리키는 시 (1 ~ 12)
	UFUNCTION(BlueprintPure, Category = "Puzzle|Clock")
	int32 GetCurrentHour() const { return CurrentHour == 0 ? 12 : CurrentHour; }

	// 현재 분침이 가리키는 분 (0 ~ 59)
	UFUNCTION(BlueprintPure, Category = "Puzzle|Clock")
	int32 GetCurrentMinute() const { return CurrentMinute; }

	// 바늘이 회전 애니메이션 중인지
	UFUNCTION(BlueprintPure, Category = "Puzzle|Clock")
	bool IsHandsAnimating() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ALCOPuzzleBase: 어떤 시계 부품이든 월드에서 상호작용하면 시계 보기 시작
	virtual void HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor) override;

	// 오답 후 리셋 시 시작 시각으로 (bResetOnFail이 켜져 있을 때만)
	virtual void OnPuzzleReset_Implementation() override;

	// 해결 상태로 복원되면 바늘을 정답 시각으로
	virtual void HandleRestoredAsSolved() override;

	// ---------- 컴포넌트 ----------

	// 시계 보기 카메라. BP에서 시계가 잘 보이는 위치로 옮길 것
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock")
	TObjectPtr<UCameraComponent> ViewCamera;

	// ---------- BP 이벤트 ----------

	// 바늘 각도 갱신 (각도: 12시 방향 기준 시계 방향, 도). BP는 이 각도로 HourPivot / MinutePivot을 회전시키면 됨
	// 회전 애니메이션 중에는 매 프레임 호출되고, bInstant는 애니메이션 없이 바로 맞출 때(시작 / 복원) true
	// 각도는 0 ~ 360을 넘을 수 있음 (한 바퀴를 넘어 돌 때 끊기지 않도록). Make Rotator에 그대로 넣으면 됨
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Clock")
	void OnClockTimeChanged(float HourHandAngle, float MinuteHandAngle, bool bInstant);

	// 바늘을 클릭해서 돌리기 시작할 때 (태엽 소리 등). bHourHand: 시침이면 true
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Clock")
	void OnHandTurned(bool bHourHand);

	// 시계 보기 중 바늘 / 확정 외의 부품을 클릭했을 때 (예: InputID "Door" → 뚜껑 열고 닫기). F키 상호작용은 OnPuzzleInteracted
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Clock")
	void OnClockPartClicked(FName InputID, ULCOInteractComponent* Part);

	// 시계 보기 시작 / 종료 (안내 문구, 소리 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Clock")
	void OnClockViewEntered();

	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle|Clock")
	void OnClockViewExited();

	// ---------- 설정값 ----------

	// 정답 시각 (시: 1 ~ 12, 분: 0 ~ 59)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "1", ClampMax = "12"))
	int32 AnswerHour = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "0", ClampMax = "59"))
	int32 AnswerMinute = 0;

	// 시작 시각
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "1", ClampMax = "12"))
	int32 StartHour = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "0", ClampMax = "59"))
	int32 StartMinute = 0;

	// 분침을 한 번 클릭할 때 움직이는 분 (정답 분 / 시작 분이 이 값의 배수여야 맞출 수 있음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "1", ClampMax = "30"))
	int32 MinuteStep = 5;

	// 바늘 회전 속도 (도/초). 0이면 애니메이션 없이 바로 이동
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "0.0", Units = "deg/s"))
	float HandRotateSpeed = 120.f;

	// 바늘 / 확정 버튼 구분용 InputID (각 LCOInteract의 InputID와 같게)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock")
	FName HourHandID = TEXT("Hour");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock")
	FName MinuteHandID = TEXT("Minute");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock")
	FName ConfirmID = TEXT("Confirm");

	// 카메라 전환 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "0.0", Units = "s"))
	float ViewBlendTime = 0.5f;

	// 해결 후 시계 보기가 자동으로 끝나기까지 시간 (해결 연출을 보여주는 시간)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock", meta = (ClampMin = "0.0", Units = "s"))
	float ExitDelayAfterSolve = 1.5f;

	// ---------- 진자 (X축 = Roll 기준으로 흔들림) ----------

	// 흔들 컴포넌트: "Pendulum" 태그가 붙은 컴포넌트, 없으면 InputID가 PendulumID인 LCOInteract
	// 진자 메시의 회전 중심이 위쪽(매달린 점)이 아니면, 매달린 점에 Scene을 두고 진자를 그 아래에 붙인 뒤 Scene에 태그를 달 것
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock|Pendulum")
	FName PendulumTag = TEXT("Pendulum");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock|Pendulum")
	FName PendulumID = TEXT("Confirm");

	// 오답: 이 각도로 짧게 떨리다가 멈춤
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock|Pendulum", meta = (ClampMin = "0.0", Units = "deg"))
	float FailShakeAngle = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock|Pendulum", meta = (ClampMin = "0.0", Units = "s"))
	float FailShakeDuration = 1.f;

	// 오답 떨림 속도 (초당 왕복 횟수)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock|Pendulum", meta = (ClampMin = "0.1"))
	float FailShakeFrequency = 6.f;

	// 정답: 이 각도로 계속 좌우로 흔들림
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock|Pendulum", meta = (ClampMin = "0.0", Units = "deg"))
	float SolvedSwingAngle = 5.f;

	// 정답 흔들림 한 번 왕복 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|Clock|Pendulum", meta = (ClampMin = "0.1", Units = "s"))
	float SolvedSwingPeriod = 2.f;

private:
	void EnterClockView(AActor* Interactor);

	void HandleLeftClick();
	void HandleExitInput();

	// ESC(옵션 키) → LCOPlayerController::OnPuzzleExitRequested
	UFUNCTION()
	void HandlePuzzleExitRequested(AActor* PuzzleActor);

	// 커서 아래에 있는 이 시계의 조작 부품(바늘 / 확정 버튼). 없으면 nullptr
	ULCOInteractComponent* GetComponentUnderCursor() const;

	// 시침 / 분침 한 칸 이동 (Direction: +1 시계 방향, -1 반대)
	void TurnHourHand(int32 Direction);
	void TurnMinuteHand(int32 Direction);

	// 시각을 바로 지정 (시작 / 리셋 / 복원). bInstant면 애니메이션 없이 바로 맞춤
	void SetTime(int32 Hour, int32 Minute, bool bInstant);

	void Confirm();

	void SetHoveredComponent(ULCOInteractComponent* NewHovered);

	// 표시 각도를 목표 각도로 보간. 아직 움직이는 중이면 true
	bool UpdateHandAnimation(float DeltaTime);

	void BroadcastHandAngles(bool bInstant);
	void UpdateTickEnabled();

	// 진자
	void CachePendulum();
	bool IsPendulumMoving() const;
	void StartPendulumShake();
	void StartPendulumSwing();
	void UpdatePendulum(float DeltaTime);

	UPROPERTY()
	TObjectPtr<USceneComponent> PendulumComponent;

	FRotator PendulumRestRotation = FRotator::ZeroRotator;
	bool bPendulumSwinging = false;
	float PendulumSwingElapsed = 0.f;

	// 오답 떨림 경과 시간 (FailShakeDuration 이상이면 떨리지 않는 상태)
	float PendulumShakeElapsed = TNumericLimits<float>::Max();

	UPROPERTY()
	TObjectPtr<APlayerController> ViewingController;

	UPROPERTY()
	TObjectPtr<ULCOInteractComponent> HoveredComponent;

	// 논리 시각 (정답 판정용). 시: 0 ~ 11 (0 = 12시), 분: 0 ~ 59
	int32 CurrentHour = 0;
	int32 CurrentMinute = 0;

	// 바늘 각도. 목표(Target)는 한 칸씩 누적되어 360을 넘을 수 있음 → 0시를 지날 때도 같은 방향으로 이어서 회전
	float TargetHourAngle = 0.f;
	float TargetMinuteAngle = 0.f;
	float DisplayHourAngle = 0.f;
	float DisplayMinuteAngle = 0.f;

	FTimerHandle ExitTimerHandle;
};
