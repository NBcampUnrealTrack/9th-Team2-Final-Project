#include "UI/LCONotebookUISubsystem.h"

#include "UI/LCONotebookWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"

void ULCONotebookUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateNotebook(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCONotebookUISubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	CleanupNotebook();

	Super::Deinitialize();
}

void ULCONotebookUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupNotebook();
	TryCreateNotebook(NewPlayerController);
}

void ULCONotebookUISubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (NotebookWidget && NotebookWidget->GetWorld() == World)
	{
		CleanupNotebook();
	}
}

void ULCONotebookUISubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateNotebook(LocalPlayer->GetPlayerController(LoadedWorld));
	}
}

void ULCONotebookUISubsystem::TryCreateNotebook(APlayerController* PlayerController)
{
	if (!PlayerController || !PlayerController->IsLocalPlayerController())
	{
		return;
	}

	if (!ShouldCreateNotebookForWorld(PlayerController->GetWorld()))
	{
		return;
	}

	if (NotebookWidget && NotebookWidget->GetOwningPlayer() == PlayerController)
	{
		return;
	}

	CleanupNotebook();

	if (NotebookWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebookUISubsystem] NotebookWidgetClass 미설정 - DefaultGame.ini 확인"));
		return;
	}

	TSubclassOf<ULCONotebookWidget> LoadedClass = NotebookWidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebookUISubsystem] NotebookWidgetClass 로드 실패: %s"), *NotebookWidgetClass.ToString());
		return;
	}

	// 생성만 해두면 위젯이 NativeOnInitialized에서 J 키에 바인딩됨 (화면에는 J를 눌러야 표시)
	NotebookWidget = CreateWidget<ULCONotebookWidget>(PlayerController, LoadedClass);
}

void ULCONotebookUISubsystem::CleanupNotebook()
{
	if (!NotebookWidget)
	{
		return;
	}

	NotebookWidget->UnbindFromNotebookInput();

	if (NotebookWidget->IsInViewport())
	{
		NotebookWidget->CloseNotebook();
	}

	NotebookWidget = nullptr;
}

bool ULCONotebookUISubsystem::ShouldCreateNotebookForWorld(const UWorld* World) const
{
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	const FString MapName = UWorld::RemovePIEPrefix(World->GetMapName());
	if (MapName.IsEmpty())
	{
		return false;
	}

	for (const FName& ExcludedMapName : ExcludedMapNames)
	{
		if (MapName.Equals(ExcludedMapName.ToString(), ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	return true;
}
