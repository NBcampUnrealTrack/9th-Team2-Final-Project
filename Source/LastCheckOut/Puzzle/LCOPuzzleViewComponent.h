#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LCOPuzzleViewComponent.generated.h"

class APlayerController;
class UInputComponent;
class ULCOInteractComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOPuzzleViewChanged);

// 퍼즐 확대 보기 컴포넌트 (키패드, 배전반, 금고 다이얼 등)
// - 퍼즐(ALCOPuzzleBase) BP에 Camera 컴포넌트와 함께 추가하면, 부품에 F키 → 그 카메라로 화면 고정 + 마우스 커서 표시 (이동 / 시점 입력 차단)
//   F키는 보기 시작만 하고 퍼즐 입력으로 처리하지 않음
// - 보기 중에는 부품(LCOInteract, InputID 있음)을 마우스로 클릭 → F키 상호작용과 같은 경로로 퍼즐에 전달 (OnPuzzleInteracted → HandlePuzzleInput)
// - 커서를 올린 부품은 외곽선 표시
// - 나가기: ESC(옵션 키, LCOPlayerController의 퍼즐 보기 상태 사용) 또는 우클릭. 퍼즐이 해결되면 ExitDelayAfterSolve 뒤에 자동으로 나감
// - 보기 카메라: 액터의 첫 번째 Camera 컴포넌트 (SetViewTarget 기본 동작)
// - 괘종시계(ALCOPuzzle_Clock)는 자체 보기 기능이 있으므로 이 컴포넌트를 붙이지 말 것
UCLASS(ClassGroup = (Puzzle), meta = (BlueprintSpawnableComponent))
class LASTCHECKOUT_API ULCOPuzzleViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULCOPuzzleViewComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 보기 시작. Interactor가 플레이어 컨트롤러로 조종되는 Pawn이 아니거나 이미 보기 중이면 false
	UFUNCTION(BlueprintCallable, Category = "Puzzle|View")
	bool EnterView(AActor* Interactor);

	// 보기 종료 (이미 나와 있으면 무시)
	UFUNCTION(BlueprintCallable, Category = "Puzzle|View")
	void ExitView();

	UFUNCTION(BlueprintPure, Category = "Puzzle|View")
	bool IsViewing() const { return ViewingController != nullptr; }

	// 퍼즐이 해결됐을 때 (ALCOPuzzleBase에서 호출). 보기 중이면 ExitDelayAfterSolve 뒤에 나감
	void HandlePuzzleSolved();

	// 보기를 시작했을 때 (BP 연출, 안내 UI 등)
	UPROPERTY(BlueprintAssignable, Category = "Puzzle|View")
	FOnLCOPuzzleViewChanged OnViewEntered;

	// 보기를 끝냈을 때
	UPROPERTY(BlueprintAssignable, Category = "Puzzle|View")
	FOnLCOPuzzleViewChanged OnViewExited;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 보기 카메라로 전환 / 복귀하는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|View", meta = (ClampMin = "0.0", Units = "s"))
	float ViewBlendTime = 0.5f;

	// 해결 후 자동으로 나가기까지 기다리는 시간 (해결 연출을 보여줄 시간)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle|View", meta = (ClampMin = "0.0", Units = "s"))
	float ExitDelayAfterSolve = 1.5f;

private:
	void HandleLeftClick();
	void HandleExitInput();

	// ESC(옵션 키) → LCOPlayerController::OnPuzzleExitRequested
	UFUNCTION()
	void HandlePuzzleExitRequested(AActor* PuzzleActor);

	// 커서 아래에 있는 이 액터의 부품(InputID가 있는 LCOInteract). 없으면 nullptr
	ULCOInteractComponent* GetComponentUnderCursor() const;

	void SetHoveredComponent(ULCOInteractComponent* NewHovered);

	UPROPERTY()
	TObjectPtr<APlayerController> ViewingController;

	UPROPERTY()
	TObjectPtr<ULCOInteractComponent> HoveredComponent;

	// 보기 중에만 컨트롤러 입력 스택에 올리는 클릭 / 나가기 입력
	UPROPERTY()
	TObjectPtr<UInputComponent> ViewInputComponent;

	FTimerHandle ExitTimerHandle;
};
