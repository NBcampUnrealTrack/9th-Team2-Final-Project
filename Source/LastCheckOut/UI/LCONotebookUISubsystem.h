#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "LCONotebookUISubsystem.generated.h"

class ULCONotebookWidget;
class UWorld;

/**
 * 수첩 위젯 자동 생성 (LCOPauseMenuSubsystem과 같은 방식)
 * 게임 맵마다 WBP_Notebook을 한 번 만들어 두면, 위젯이 J 키에 스스로 바인딩됨
 * 설정: Config/DefaultGame.ini
 *   [/Script/LastCheckOut.LCONotebookUISubsystem]
 *   NotebookWidgetClass=/Game/.../WBP_Notebook.WBP_Notebook_C
 *   +ExcludedMapNames=MainMenu_Test
 */
UCLASS(Config = Game)
class LASTCHECKOUT_API ULCONotebookUISubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	ULCONotebookWidget* GetNotebookWidget() const { return NotebookWidget; }

private:
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);

	void TryCreateNotebook(APlayerController* PlayerController);
	void CleanupNotebook();
	bool ShouldCreateNotebookForWorld(const UWorld* World) const;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Notebook")
	TSoftClassPtr<ULCONotebookWidget> NotebookWidgetClass;

	UPROPERTY(Config, EditDefaultsOnly, Category = "LCO|Notebook")
	TArray<FName> ExcludedMapNames;

	UPROPERTY(Transient)
	TObjectPtr<ULCONotebookWidget> NotebookWidget;
};
