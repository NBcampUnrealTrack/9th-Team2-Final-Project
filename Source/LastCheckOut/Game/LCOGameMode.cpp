#include "LCOGameMode.h"
#include "LastCheckOut.h"
#include "LastCheckOut/Core/LCOGameInstance.h"
#include "LastCheckOut/Story/LCOStorySubsystem.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Player/LCOPlayerCharacter.h"
#include "Inventory/InventoryComponent.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "Save/LCOSaveManager.h"

ALCOGameMode::ALCOGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALCOGameMode::BeginPlay()
{
	Super::BeginPlay();

	InitializeCurrentMap();
	StartGame();
}

void ALCOGameMode::InitializeCurrentMap()
{
	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] InitializeCurrentMap: %s"), 
		*GetWorld()->GetMapName()); 
}

void ALCOGameMode::StartGame()
{
	// GameInstance를 LCOGameInstance 타입으로 가져옴
	// Project Settings의 Game Instance Class가 다르면 nullptr이 반환
	ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>();
	if (!LCOGameInstance)
	{
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StartGame: LCOGameInstance not found"));
		return;
	}
	
	// GameInstance에 붙어 있는 StorySubsystem을 가져옴 (엔진이 자동 생성)
	ULCOStorySubsystem* StorySubsystem = LCOGameInstance->GetSubsystem<ULCOStorySubsystem>();
	if (!StorySubsystem)
	{
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StartGame: LCOStorySubsystem not found"));
		return;
	}
	
	// 현재 시작 방식 확인
	// Log Test
	const ELCOGameStartMode StartMode = LCOGameInstance->GetStartMode();
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: StartMode = %s"), 
		*UEnum::GetValueAsString(StartMode));
	
	switch (StartMode)
	{
	case ELCOGameStartMode::NewGame:
		// 새 게임: 수첩 / 이동 보관함 / 스토리 초기화 + Chapter1 시작
		StartNewGameProgress(LCOGameInstance);

		// 다음 맵 이동 시 다시 새 게임으로 처리되지 않도록 None으로 되돌림
		LCOGameInstance->ResetStartMode();
		break;

	case ELCOGameStartMode::Continue:
	{
		// 이어하기: SaveManager가 읽어 둔 세이브 데이터를 적용
		// (플레이어 데이터는 보관함에 들어가고, HandleMatchHasStarted에서 캐릭터에 복원됨)
		ULCOSaveManager* SaveManager = LCOGameInstance->GetSubsystem<ULCOSaveManager>();

		// StartContinue를 거치지 않고 Continue로 들어온 경우(메뉴에서 StartMode만 설정 후 이동 등)
		// 세이브 파일을 아직 읽지 않았으므로 여기서 먼저 읽음
		if (SaveManager && !SaveManager->HasLoadedData())
		{
			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: Continue without loaded data -> LoadSaveFile"));
			SaveManager->LoadSaveFile();
		}

		if (SaveManager && SaveManager->ApplyLoadedData())
		{
			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: Continue -> save data applied"));
		}
		else
		{
			// 세이브 적용 실패 시 새 게임으로 시작 (빈 상태로 멈추지 않도록)
			UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StartGame: Continue failed -> StartNewGameProgress"));
			StartNewGameProgress(LCOGameInstance);
		}

		// 다음 맵 이동 시 다시 이어하기로 처리되지 않도록 None으로 되돌림
		LCOGameInstance->ResetStartMode();
		break;
	}

	case ELCOGameStartMode::None:
	default:
		// 메뉴 없이 맵을 바로 실행했거나, 맵 이동으로 들어온 경우
		// - 진행 중인 챕터가 없으면 → 새 스토리(Chapter1) 자동 시작 (개발/테스트 편의)
		// - 진행 중인 챕터가 있으면 → 기존 진행 상태 유지 (맵 이동 시)
		// 자동 시작을 끄고 메인 메뉴의 "새 게임"을 거쳐야만 스토리가 시작되도록 하려면?
		// 아래 if / else 블록 전체를 삭제하면 됨
		if (StorySubsystem->GetCurrentChapterID().IsNone())
		{
			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: No chapter in progress -> StartNewGameProgress"));
			StartNewGameProgress(LCOGameInstance);
		}
		else
		{
			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartGame: Keep current chapter = %s"),
				*StorySubsystem->GetCurrentChapterID().ToString());
		}
		break;
	}
}

void ALCOGameMode::StartNewGameProgress(ULCOGameInstance* LCOGameInstance)
{
	// 1) 수첩 기록 전체 삭제
	if (ULCONotebookSubsystem* NotebookSubsystem = LCOGameInstance->GetNotebookSubsystem())
	{
		NotebookSubsystem->ClearAll();
	}

	// 2) 맵 이동용 보관함 비우기 (이전 플레이의 인벤토리가 복원되지 않도록)
	LCOGameInstance->ClearCarryData();

	// 3) 스토리 초기화 + Chapter1 시작 (완료 목표도 초기화)
	if (ULCOStorySubsystem* StorySubsystem = LCOGameInstance->GetStorySubsystem())
	{
		StorySubsystem->StartNewStory();
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartNewGameProgress: Notebook / Carry / Story reset"));
}

void ALCOGameMode::TravelToLevel(TSoftObjectPtr<UWorld> Level, FName PlayerStartTag)
{
	if (Level.IsNull())
	{
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] TravelToLevel: Level is None"));
		return;
	}

	// 캐릭터가 사라지기 전에 먼저 보관
	StorePlayerCarryData();

	// 도착 위치는 URL 옵션으로 전달 → 새 맵 GameMode의 ChoosePlayerStart에서 읽음
	const FString Options = PlayerStartTag.IsNone() ? FString() : FString::Printf(TEXT("Entry=%s"), *PlayerStartTag.ToString());

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] TravelToLevel: %s (Entry = %s)"), *Level.ToString(), *PlayerStartTag.ToString());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level, true, Options);
}

void ALCOGameMode::HandleMatchHasStarted()
{
	// 이 안에서 플레이어 캐릭터 생성 → 모든 액터 BeginPlay(StartGame 포함)까지 끝남
	Super::HandleMatchHasStarted();

	// 캐릭터 BeginPlay의 SetViewMode(DefaultViewMode) 이후에 복원해야 시점이 덮어써지지 않음
	RestorePlayerCarryData();
}

bool ALCOGameMode::CapturePlayerCarryData(FLCOPlayerCarryData& OutData) const
{
	ALCOPlayerCharacter* Player = Cast<ALCOPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player)
	{
		return false;
	}

	OutData = FLCOPlayerCarryData();

	// 인벤토리
	if (UInventoryComponent* Inventory = Player->GetInventory())
	{
		OutData.InventorySlots = Inventory->InventorySlots;
	}

	// 퀵슬롯 (슬롯 순서대로)
	if (ULCOQuickSlotComponent* QuickSlot = Player->GetQuickSlot())
	{
		for (int32 i = 0; i < ULCOQuickSlotComponent::NumSlots; ++i)
		{
			OutData.QuickSlotItemIDs.Add(QuickSlot->GetSlotItem(i));
		}
	}

	// 시점
	OutData.ViewMode = Player->GetViewMode();

	return true;
}

void ALCOGameMode::StorePlayerCarryData()
{
	ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>();

	FLCOPlayerCarryData Data;
	if (!LCOGameInstance || !CapturePlayerCarryData(Data))
	{
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StorePlayerCarryData: GameInstance or Player not found"));
		return;
	}

	LCOGameInstance->StoreCarryData(Data);
}

void ALCOGameMode::RestorePlayerCarryData()
{
	ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>();
	ALCOPlayerCharacter* Player = Cast<ALCOPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!LCOGameInstance || !Player)
	{
		// 캐릭터를 못 찾으면 꺼내지 않음 (보관 데이터 유실 방지)
		return;
	}

	FLCOPlayerCarryData Data;
	if (!LCOGameInstance->ConsumeCarryData(Data))
	{
		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] RestorePlayerCarryData: No carry data"));
		return;
	}

	// 1) 인벤토리: AddItem을 쓰지 않고 그대로 넣음 (수첩 재등록 방지) → UI 갱신 이벤트만 호출
	if (UInventoryComponent* Inventory = Player->GetInventory())
	{
		Inventory->InventorySlots = Data.InventorySlots;
		Inventory->OnInventoryChanged.Broadcast();
	}

	// 2) 퀵슬롯: 인벤토리 복원 후에 등록 (AssignSlot이 아이템 데이터를 확인함)
	if (ULCOQuickSlotComponent* QuickSlot = Player->GetQuickSlot())
	{
		for (int32 i = 0; i < Data.QuickSlotItemIDs.Num() && i < ULCOQuickSlotComponent::NumSlots; ++i)
		{
			QuickSlot->AssignSlot(i, Data.QuickSlotItemIDs[i]);
		}
	}

	// 3) 시점
	Player->SetViewMode(Data.ViewMode);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] RestorePlayerCarryData: Items = %d"), Data.InventorySlots.Num());
}

AActor* ALCOGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// 맵 이동 시 전달된 도착 태그 (예: ?Entry=FromMain)
	const FString EntryTag = UGameplayStatics::ParseOption(OptionsString, TEXT("Entry"));

	if (!EntryTag.IsEmpty())
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == FName(*EntryTag))
			{
				// Log Test
				UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ChoosePlayerStart: Entry = %s"), *EntryTag);
				return *It;
			}
		}

		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] ChoosePlayerStart: no PlayerStart with tag %s -> default"), *EntryTag);
	}

	// 태그가 없거나 못 찾으면 엔진 기본 방식
	return Super::ChoosePlayerStart_Implementation(Player);
}
