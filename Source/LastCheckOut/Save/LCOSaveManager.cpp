#include "Save/LCOSaveManager.h"
#include "LastCheckOut.h"
#include "Save/LCOSaveGame.h"
#include "Core/LCOGameInstance.h"
#include "Game/LCOGameMode.h"
#include "Story/LCOStorySubsystem.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

const FString ULCOSaveManager::SaveSlotName(TEXT("LCO_Save_0"));

void ULCOSaveManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOSaveManager] Initialize (HasSaveFile = %s)"),
		HasSaveFile() ? TEXT("true") : TEXT("false"));
}

bool ULCOSaveManager::HasSaveFile() const
{
	return UGameplayStatics::DoesSaveGameExist(SaveSlotName, SaveUserIndex);
}

// 저장
bool ULCOSaveManager::SaveGame()
{
	// 저장 중에 또 저장 요청이 오면 무시 (전화기 연타 등)
	if (bIsSaving)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOSaveManager] SaveGame ignored: already saving"));
		return false;
	}

	ULCOSaveGame* SaveData = Cast<ULCOSaveGame>(UGameplayStatics::CreateSaveGameObject(ULCOSaveGame::StaticClass()));
	if (!SaveData)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOSaveManager] SaveGame failed: CreateSaveGameObject"));
		return false;
	}

	SaveData->SavedTime = FDateTime::Now();

	UWorld* World = GetGameInstance()->GetWorld();

	// 현재 맵 경로 (에디터 Play 시 붙는 UEDPIE_0_ 접두어 제거)
	if (World)
	{
		SaveData->MapName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	}

	// [스토리] 챕터 / 목표 / 완료 목표
	if (const ULCOStorySubsystem* StorySubsystem = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>())
	{
		SaveData->ChapterID = StorySubsystem->GetCurrentChapterID();
		SaveData->ObjectiveID = StorySubsystem->GetCurrentObjectiveID();
		SaveData->CompletedObjectives = StorySubsystem->GetCompletedObjectives();
	}

	// [수첩] 분류별로 전부 모으기 (수첩에 전체 조회 함수가 없어 분류 4개를 순서대로 조회)
	if (const ULCONotebookSubsystem* NotebookSubsystem = GetGameInstance()->GetSubsystem<ULCONotebookSubsystem>())
	{
		const ELCONotebookCategory Categories[] =
		{
			ELCONotebookCategory::Objective,
			ELCONotebookCategory::Clue,
			ELCONotebookCategory::Testimony,
			ELCONotebookCategory::Photo
		};

		for (const ELCONotebookCategory Category : Categories)
		{
			SaveData->NotebookEntries.Append(NotebookSubsystem->GetEntriesByCategory(Category));
		}
	}

	// [Player] 인벤토리 / 퀵슬롯 / 시점 (GameMode의 맵 이동 보관과 같은 함수 사용)
	if (World)
	{
		if (const ALCOGameMode* LCOGameMode = World->GetAuthGameMode<ALCOGameMode>())
		{
			SaveData->bHasPlayerData = LCOGameMode->CapturePlayerCarryData(SaveData->PlayerData);
		}
	}

	bIsSaving = true;
	OnSaveStarted.Broadcast();

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOSaveManager] SaveGame start: Map = %s, Chapter = %s, Objective = %s, Completed = %d, Notebook = %d, PlayerData = %s, Items = %d"),
		*SaveData->MapName, *SaveData->ChapterID.ToString(), *SaveData->ObjectiveID.ToString(),
		SaveData->CompletedObjectives.Num(), SaveData->NotebookEntries.Num(),
		SaveData->bHasPlayerData ? TEXT("true") : TEXT("false"), SaveData->PlayerData.InventorySlots.Num());

	// 비동기 저장 (게임이 멈추지 않음). 완료되면 HandleAsyncSaveFinished 호출
	FAsyncSaveGameToSlotDelegate SavedDelegate;
	SavedDelegate.BindUObject(this, &ULCOSaveManager::HandleAsyncSaveFinished);
	UGameplayStatics::AsyncSaveGameToSlot(SaveData, SaveSlotName, SaveUserIndex, SavedDelegate);

	return true;
}

void ULCOSaveManager::HandleAsyncSaveFinished(const FString& SlotName, const int32 UserIndex, bool bSuccess)
{
	bIsSaving = false;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOSaveManager] SaveGame finished: Slot = %s, Success = %s"),
		*SlotName, bSuccess ? TEXT("true") : TEXT("false"));

	OnSaveFinished.Broadcast(bSuccess);
}

// 이어하기 / 불러오기
bool ULCOSaveManager::StartContinue()
{
	// 세이브 파일 읽기
	if (!LoadSaveFile())
	{
		return false;
	}

	if (LoadedSaveGame->MapName.IsEmpty())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOSaveManager] StartContinue failed: saved map is empty"));
		return false;
	}

	ULCOGameInstance* LCOGameInstance = Cast<ULCOGameInstance>(GetGameInstance());
	if (!LCOGameInstance)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOSaveManager] StartContinue failed: LCOGameInstance not found"));
		return false;
	}

	// 이동한 맵의 GameMode가 이어하기로 처리하도록 표시
	LCOGameInstance->SetStartMode(ELCOGameStartMode::Continue);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOSaveManager] StartContinue: Open %s"), *LoadedSaveGame->MapName);

	// 저장한 맵으로 이동 → 새 맵의 LCOGameMode::StartGame()에서 ApplyLoadedData 호출
	UGameplayStatics::OpenLevel(LCOGameInstance->GetWorld(), FName(*LoadedSaveGame->MapName));
	return true;
}

bool ULCOSaveManager::LoadSaveFile()
{
	if (!HasSaveFile())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOSaveManager] LoadSaveFile failed: no save file"));
		return false;
	}

	LoadedSaveGame = Cast<ULCOSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, SaveUserIndex));
	if (!LoadedSaveGame)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOSaveManager] LoadSaveFile failed: invalid save data"));
		return false;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOSaveManager] LoadSaveFile: Map = %s, Chapter = %s, Objective = %s, SavedTime = %s"),
		*LoadedSaveGame->MapName, *LoadedSaveGame->ChapterID.ToString(), *LoadedSaveGame->ObjectiveID.ToString(),
		*LoadedSaveGame->SavedTime.ToString());

	return true;
}

bool ULCOSaveManager::ApplyLoadedData()
{
	if (!LoadedSaveGame)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOSaveManager] ApplyLoadedData failed: call LoadSaveFile first"));
		return false;
	}

	// [스토리] 진행 상태 복원
	if (ULCOStorySubsystem* StorySubsystem = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>())
	{
		StorySubsystem->RestoreStoryState(LoadedSaveGame->ChapterID, LoadedSaveGame->ObjectiveID, LoadedSaveGame->CompletedObjectives);
	}

	// [수첩] 기존 기록을 비우고 저장된 기록으로 다시 채움
	if (ULCONotebookSubsystem* NotebookSubsystem = GetGameInstance()->GetSubsystem<ULCONotebookSubsystem>())
	{
		NotebookSubsystem->ClearAll();

		for (const FLCONotebookEntry& Entry : LoadedSaveGame->NotebookEntries)
		{
			NotebookSubsystem->AddEntry(Entry);
		}
	}

	// [Player] 맵 이동용 보관함에 넣어 둠 → 캐릭터 생성 후 LCOGameMode가 복원
	if (LoadedSaveGame->bHasPlayerData)
	{
		if (ULCOGameInstance* LCOGameInstance = Cast<ULCOGameInstance>(GetGameInstance()))
		{
			LCOGameInstance->StoreCarryData(LoadedSaveGame->PlayerData);
		}
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOSaveManager] ApplyLoadedData done: Notebook = %d, PlayerData = %s"),
		LoadedSaveGame->NotebookEntries.Num(), LoadedSaveGame->bHasPlayerData ? TEXT("true") : TEXT("false"));
	return true;
}

FString ULCOSaveManager::GetLoadedMapName() const
{
	return LoadedSaveGame ? LoadedSaveGame->MapName : FString();
}
