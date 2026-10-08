#include "UI/LCONotebookUISubsystem.h"

#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "UI/LCONotebookWidget.h"
#include "UObject/UObjectGlobals.h"

void ULCONotebookUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMapWithWorld);

	BindLocalizationEvents();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		TryCreateNotebook(LocalPlayer->GetPlayerController(GetWorld()));
	}
}

void ULCONotebookUISubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	UnbindLocalizationEvents();

	CleanupNotebook();

	Super::Deinitialize();
}

void ULCONotebookUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	CleanupNotebook();
	TryCreateNotebook(NewPlayerController);
}

void ULCONotebookUISubsystem::HandleLanguageChanged(FName NewLanguage)
{
	RefreshNotebookLocalizedTexts();
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
		RefreshNotebookLocalizedTexts();
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
	RefreshNotebookLocalizedTexts();
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

void ULCONotebookUISubsystem::BindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCONotebookUISubsystem::UnbindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCONotebookUISubsystem::RefreshNotebookLocalizedTexts() const
{
	if (!NotebookWidget || !GetLocalizationSubsystem())
	{
		return;
	}

	SetNotebookButtonText(TEXT("Btn_Tab_Objective"), TEXT("NOTEBOOK_OBJECTIVES"));
	SetNotebookButtonText(TEXT("Btn_Tab_Clue"), TEXT("NOTEBOOK_CLUES"));
	SetNotebookButtonText(TEXT("Btn_Tab_Testimony"), TEXT("NOTEBOOK_TESTIMONIES"));
	SetNotebookButtonText(TEXT("Btn_Tab_Photo"), TEXT("NOTEBOOK_PHOTOS"));
	SetNotebookButtonText(TEXT("Btn_Tab_Map"), TEXT("NOTEBOOK_MAPS"));
	SetNotebookTextBlockText(TEXT("Txt_EmptyList"), TEXT("NOTEBOOK_EMPTY"));
}

void ULCONotebookUISubsystem::SetNotebookButtonText(FName ButtonName, FName TextKey) const
{
	if (!NotebookWidget)
	{
		return;
	}

	UButton* Button = Cast<UButton>(NotebookWidget->GetWidgetFromName(ButtonName));
	UTextBlock* TextBlock = Button ? FindTextBlock(Button->GetContent()) : nullptr;
	if (!TextBlock)
	{
		return;
	}

	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
	{
		TextBlock->SetText(LocalizationSubsystem->GetUIText(TextKey));
	}
}

void ULCONotebookUISubsystem::SetNotebookTextBlockText(FName TextBlockName, FName TextKey) const
{
	if (!NotebookWidget)
	{
		return;
	}

	UTextBlock* TextBlock = Cast<UTextBlock>(NotebookWidget->GetWidgetFromName(TextBlockName));
	if (!TextBlock)
	{
		return;
	}

	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetLocalizationSubsystem())
	{
		TextBlock->SetText(LocalizationSubsystem->GetUIText(TextKey));
	}
}

UTextBlock* ULCONotebookUISubsystem::FindTextBlock(UWidget* Widget) const
{
	if (!Widget)
	{
		return nullptr;
	}

	if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
	{
		return TextBlock;
	}

	if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
	{
		for (int32 ChildIndex = 0; ChildIndex < PanelWidget->GetChildrenCount(); ++ChildIndex)
		{
			if (UTextBlock* TextBlock = FindTextBlock(PanelWidget->GetChildAt(ChildIndex)))
			{
				return TextBlock;
			}
		}
	}

	if (UContentWidget* ContentWidget = Cast<UContentWidget>(Widget))
	{
		return FindTextBlock(ContentWidget->GetContent());
	}

	return nullptr;
}

ULCOLocalizationSubsystem* ULCONotebookUISubsystem::GetLocalizationSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer && LocalPlayer->GetGameInstance()
		? LocalPlayer->GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>()
		: nullptr;
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
