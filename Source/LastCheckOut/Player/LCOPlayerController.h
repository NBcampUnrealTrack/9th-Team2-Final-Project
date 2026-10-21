#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LCOPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOMenuRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOPuzzleExitRequested, AActor*, PuzzleActor);

// 플레이어 컨트롤러
// 입력 매핑(IMC) 등록, 메뉴 입력(아이템 I, 수첩 J, 옵션 O) 처리
// 메뉴 위젯은 UI 담당이 아래 델리게이트에 바인딩해서 열고 닫는다
UCLASS()
class LASTCHECKOUT_API ALCOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 메뉴를 열 때 true, 닫을 때 false로 호출
	// true: 마우스 커서 표시 + 이동/시점 입력 차단 (메뉴 키는 계속 동작)
	UFUNCTION(BlueprintCallable, Category = "LCO|UI")
	void SetUIMode(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "LCO|UI")
	bool IsInUIMode() const { return bInUIMode; }

	// 아이템 키(I) 입력 시
	UPROPERTY(BlueprintAssignable, Category = "LCO|UI")
	FOnLCOMenuRequested OnInventoryRequested;

	// 수첩 키(J) 입력 시
	UPROPERTY(BlueprintAssignable, Category = "LCO|UI")
	FOnLCOMenuRequested OnNotebookRequested;

	// 옵션 키(O, 추후 ESC) 입력 시
	UPROPERTY(BlueprintAssignable, Category = "LCO|UI")
	FOnLCOMenuRequested OnOptionsRequested;
	
	// ---------- 퍼즐 보기 ----------

	// 퍼즐 보기 시작 (카메라 고정 / 퍼즐 UI 표시 시 퍼즐 쪽에서 호출)
	// 보기 중에는 인벤토리 / 수첩 키 무시, 옵션 키(O / ESC)는 일시정지 대신 OnPuzzleExitRequested 방송
	UFUNCTION(BlueprintCallable, Category = "LCO|Puzzle")
	void BeginPuzzleView(AActor* PuzzleActor);

	// 퍼즐 보기 종료 (퍼즐에서 나갈 때 퍼즐 쪽에서 호출)
	UFUNCTION(BlueprintCallable, Category = "LCO|Puzzle")
	void EndPuzzleView();

	// 퍼즐 액터가 사라지면 자동으로 false
	UFUNCTION(BlueprintPure, Category = "LCO|Puzzle")
	bool IsInPuzzleView() const { return CurrentPuzzleActor.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "LCO|Puzzle")
	AActor* GetCurrentPuzzleActor() const { return CurrentPuzzleActor.Get(); }

	// 퍼즐 보기 중 옵션 키(O / ESC)를 눌렀을 때. 퍼즐은 여기서 나가기 처리
	UPROPERTY(BlueprintAssignable, Category = "LCO|Puzzle")
	FOnLCOPuzzleExitRequested OnPuzzleExitRequested;


protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void RequestInventory();
	void RequestNotebook();
	void RequestOptions();

	// 기본 입력 매핑 (BP에서 IMC 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
	// 카메라 상하 각도 제한 (바닥 / 하늘을 수직으로 보는 것 방지)
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MinViewPitch = -70.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float MaxViewPitch = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> NotebookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> OptionsAction;

private:
	bool bInUIMode = false;
	// 현재 보고 있는 퍼즐 (없으면 퍼즐 보기 아님)
	TWeakObjectPtr<AActor> CurrentPuzzleActor;
};