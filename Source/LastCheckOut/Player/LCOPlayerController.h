#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LCOPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLCOMenuRequested);

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

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void RequestInventory();
	void RequestNotebook();
	void RequestOptions();

	// 기본 입력 매핑 (BP에서 IMC 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> NotebookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> OptionsAction;

private:
	bool bInUIMode = false;
};