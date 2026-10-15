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
#include "Save/LCOWorldStateSubsystem.h"
#include "Puzzle/LCOPuzzleSubsystem.h"
#include "UI/LCODialogueSubsystem.h"
#include "Engine/LocalPlayer.h"

ALCOGameMode::ALCOGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALCOGameMode::StartPlay()
{
	// 맵 액터들의 BeginPlay보다 먼저 진행 데이터를 준비 (새 게임 초기화 / 이어하기 적용)
	// → 퍼즐처럼 BeginPlay에서 서브시스템 기록을 읽는 액터가 정리된 값을 보게 됨
	InitializeCurrentMap();
	StartGame();

	// 지배인 보고 결과 알림 받기 (1막 클리어 처리)
	if (ULCOStorySubsystem* StorySubsystem = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>())
	{
		StorySubsystem->OnReportResolved.AddUniqueDynamic(this, &ThisClass::HandleReportResolved);
	}

	// 여기서 HandleMatchHasStarted → 플레이어 생성 → 모든 액터 BeginPlay 진행
	Super::StartPlay();
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

	// 3) 맵별 상태 기록 삭제 (이전 플레이의 문 / 서랍장 상태가 복원되지 않도록)
	if (ULCOWorldStateSubsystem* WorldState = LCOGameInstance->GetSubsystem<ULCOWorldStateSubsystem>())
	{
		WorldState->ClearAll();
	}

	// 4) 퍼즐 해결 기록 삭제
	if (ULCOPuzzleSubsystem* PuzzleSubsystem = LCOGameInstance->GetSubsystem<ULCOPuzzleSubsystem>())
	{
		PuzzleSubsystem->ClearAll();
	}

	// 5) 스토리 초기화 + Chapter1 시작 (완료 목표도 초기화)
	if (ULCOStorySubsystem* StorySubsystem = LCOGameInstance->GetStorySubsystem())
	{
		StorySubsystem->StartNewStory();
	}

	// 6) NPC 첫 대화 완료 기록 삭제 (대화 서브시스템은 LocalPlayer 단위)
	if (const ULocalPlayer* LocalPlayer = LCOGameInstance->GetFirstGamePlayer())
	{
		if (ULCODialogueSubsystem* DialogueSubsystem = LocalPlayer->GetSubsystem<ULCODialogueSubsystem>())
		{
			DialogueSubsystem->ResetDialogueProgress();
		}
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartNewGameProgress: Notebook / Carry / WorldState / Puzzle / Story / Dialogue reset"));
}

void ALCOGameMode::TravelToLevel(TSoftObjectPtr<UWorld> Level, FName PlayerStartTag, bool bAutoSaveOnArrival)
{
	if (Level.IsNull())
	{
		// Log Test
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] TravelToLevel: Level is None"));
		return;
	}

	// 캐릭터가 사라지기 전에 먼저 보관
	StorePlayerCarryData();

	// 이 맵의 문 / 서랍장 / 트리거 상태 기록 (다시 돌아왔을 때 복원)
	if (ULCOWorldStateSubsystem* WorldState = GetGameInstance()->GetSubsystem<ULCOWorldStateSubsystem>())
	{
		WorldState->CaptureWorldState(GetWorld());
	}

	// 도착 위치 / 도착 후 자동 저장은 URL 옵션으로 전달 → 새 맵 GameMode가 읽음
	// 예: "Entry=Room407?AutoSave"
	FString Options;
	if (!PlayerStartTag.IsNone())
	{
		Options = FString::Printf(TEXT("Entry=%s"), *PlayerStartTag.ToString());
	}
	if (bAutoSaveOnArrival)
	{
		Options += Options.IsEmpty() ? TEXT("AutoSave") : TEXT("?AutoSave");
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] TravelToLevel: %s (Entry = %s, AutoSave = %s)"),
		*Level.ToString(), *PlayerStartTag.ToString(), bAutoSaveOnArrival ? TEXT("true") : TEXT("false"));
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level, true, Options);
}

void ALCOGameMode::HandleMatchHasStarted()
{
	// 이 안에서 플레이어 캐릭터 생성 → 모든 액터 BeginPlay까지 끝남
	Super::HandleMatchHasStarted();

	// 이 맵에 기록된 문 / 서랍장 / 트리거 상태 복원 (BeginPlay의 초기 상태 적용 이후여야 덮어써지지 않음)
	if (ULCOWorldStateSubsystem* WorldState = GetGameInstance()->GetSubsystem<ULCOWorldStateSubsystem>())
	{
		WorldState->ApplyWorldState(GetWorld());
	}

	// 캐릭터 BeginPlay의 SetViewMode(DefaultViewMode) 이후에 복원해야 시점이 덮어써지지 않음
	RestorePlayerCarryData();

	// 챕터 전환 등으로 AutoSave 옵션이 붙어 왔으면, 모든 복원이 끝난 지금 저장
	TryAutoSaveOnArrival();
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
	// 에디터 Play From Here로 시작하면 엔진 기본 방식 사용 (카메라 위치에서 시작)
	// (엔진이 임시로 만드는 PlayerStartPIE가 있으면 그 경우. 헤더가 공개되지 않아 클래스 이름으로 확인)
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetFName() == FName(TEXT("PlayerStartPIE")))
		{
			return Super::ChoosePlayerStart_Implementation(Player);
		}
	}

	// 맵 이동 시 전달된 도착 태그 (예시: ?Entry=FromMain)
	const FString EntryTag = UGameplayStatics::ParseOption(OptionsString, TEXT("Entry"));

	// 도착 태그가 없으면(새 게임 / 이어하기 / 메뉴 없이 Play) 기본 시작 위치 태그 "Start" 사용
	const FName SearchTag = EntryTag.IsEmpty() ? FName(TEXT("Start")) : FName(*EntryTag);

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag == SearchTag)
		{
			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ChoosePlayerStart: Tag = %s"), *SearchTag.ToString());
			return *It;
		}
	}

	// 맵 이동인데 태그를 못 찾은 경우만 경고 (Start 태그는 없어도 정상)
	if (!EntryTag.IsEmpty())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] ChoosePlayerStart: no PlayerStart with tag %s -> default"), *EntryTag);
	}

	// 태그가 없거나 못 찾으면 엔진 기본 방식
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ALCOGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 스토리 서브시스템은 맵을 옮겨도 남아 있으므로, 사라지는 GameMode의 연결은 끊어 둠
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (ULCOStorySubsystem* StorySubsystem = GameInstance->GetSubsystem<ULCOStorySubsystem>())
		{
			StorySubsystem->OnReportResolved.RemoveDynamic(this, &ThisClass::HandleReportResolved);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ALCOGameMode::HandleReportResolved(ELCOStoryFlag PresentedKeyword, ELCOReportResult Result)
{
	switch (Result)
	{
	case ELCOReportResult::Chapter1Clear:
		if (!Chapter2StartLevel.IsNull())
		{
			// 2막 시작 맵(별관 407호 앞)으로 이동 → 도착 후 자동 저장
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] Chapter1 clear -> Travel to Chapter2 start"));
			TravelToLevel(Chapter2StartLevel, Chapter2StartTag, true);
		}
		else
		{
			// 2막 시작 맵 미설정: 이동 없이 그 자리에서 자동 저장
			UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] Chapter1 clear: Chapter2StartLevel not set -> AutoSave here"));
			if (ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>())
			{
				SaveManager->SaveGame();
			}
		}
		break;

	case ELCOReportResult::End3:
		// 엔딩 화면은 UI가 OnReportResolved로 처리
		// 진행 상태를 바꾸지 않았으므로 엔딩 후 그대로 이어서 진행 = 보고 직전 상태
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] END 3 reached (state unchanged)"));
		break;

	default:
		break;
	}
}

void ALCOGameMode::TryAutoSaveOnArrival()
{
	if (!UGameplayStatics::HasOption(OptionsString, TEXT("AutoSave")))
	{
		return;
	}

	if (ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>())
	{
		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] AutoSave on arrival"));
		SaveManager->SaveGame();
	}
}
