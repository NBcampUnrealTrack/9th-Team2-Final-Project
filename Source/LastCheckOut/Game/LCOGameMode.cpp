#include "LCOGameMode.h"
#include "LastCheckOut.h"
#include "LastCheckOut/Core/LCOGameInstance.h"
#include "LastCheckOut/Story/LCOStorySubsystem.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Player/LCOPlayerCharacter.h"
#include "Player/LCOStaminaComponent.h"
#include "Inventory/InventoryComponent.h"
#include "QuickSlot/LCOQuickSlotComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "EngineUtils.h"
#include "Save/LCOSaveManager.h"
#include "Save/LCOWorldStateSubsystem.h"
#include "Puzzle/LCOPuzzleSubsystem.h"
#include "UI/LCODialogueSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "UI/LCOLoadingSubsystem.h"
#include "Core/LCOGameSettingsSubsystem.h"
#include "Game/LCOTimeOfDayManager.h"
#include "AI/Chasing/LCOChasingManager.h"
#include "Game/LCOTimeOfDayPresetAsset.h"
#include "Story/LCOStoryIDs.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 맵 이동 URL 옵션 (TravelToLevel에서 붙이고, 도착한 맵에서 읽음)
	const TCHAR* const EntryOption = TEXT("Entry");
	const TCHAR* const AutoSaveOption = TEXT("AutoSave");

	// 도착 태그가 없을 때 찾는 PlayerStart 태그
	const FName DefaultPlayerStartTag(TEXT("Start"));
}

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

	// 지배인 보고 결과 / 챕터 클리어 알림 받기
	if (ULCOStorySubsystem* StorySubsystem = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>())
	{
		StorySubsystem->OnReportResolved.AddUniqueDynamic(this, &ThisClass::HandleReportResolved);
		StorySubsystem->OnChapterAdvanced.AddUniqueDynamic(this, &ThisClass::HandleChapterAdvanced);
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

	// 7) 시간대 초기화 (정오 이후부터 다시 시작)
	LCOGameInstance->SetCurrentTimeOfDay(ELCOTimeOfDay::Noon);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartNewGameProgress: Notebook / Carry / WorldState / Puzzle / Story / Dialogue / TimeOfDay reset"));
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
		Options = FString::Printf(TEXT("%s=%s"), EntryOption, *PlayerStartTag.ToString());
	}
	if (bAutoSaveOnArrival)
	{
		Options += Options.IsEmpty() ? FString(AutoSaveOption) : FString::Printf(TEXT("?%s"), AutoSaveOption);
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

	// 이어하기면 저장한 자리로 이동 (전화기 저장 → 전화기 앞)
	RestoreSavedPlayerTransform();

	// 저장된 게임플레이 설정(감도 / Y축 반전)을 새 캐릭터에 적용 (새 게임 / 맵 이동 / 이어하기 공통)
	if (const ULCOGameSettingsSubsystem* GameSettings = GetGameInstance()->GetSubsystem<ULCOGameSettingsSubsystem>())
	{
		GameSettings->ApplyGameplaySettingsToPlayer(Cast<ALCOPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)));
	}

	// 잡혔을 때 수첩 압수
	BindChasingManager();

	// 이 맵 도착으로 시간대 갱신 → 조명 적용 (아래 자동 저장에 갱신된 시간대가 들어가도록 먼저 처리)
	RefreshTimeOfDay();

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

	// 버프 (수치 + 남은 시간). 적용 중이 아니면 0으로 남음
	// 도착 직후 로딩 중이라 아직 적용 대기 중인 버프가 있으면 그 값을 그대로 보관 (도착 자동 저장 등)
	if (PendingMoveSpeedBuffTime > 0.f)
	{
		OutData.MoveSpeedBuffAmount = PendingMoveSpeedBuffAmount;
		OutData.MoveSpeedBuffRemainingTime = PendingMoveSpeedBuffTime;
	}
	else if (Player->IsMoveSpeedBuffActive())
	{
		OutData.MoveSpeedBuffAmount = Player->GetMoveSpeedBuffAmount();
		OutData.MoveSpeedBuffRemainingTime = Player->GetMoveSpeedBuffRemainingTime();
	}

	if (PendingMaxStaminaBuffTime > 0.f)
	{
		OutData.MaxStaminaBuffAmount = PendingMaxStaminaBuffAmount;
		OutData.MaxStaminaBuffRemainingTime = PendingMaxStaminaBuffTime;
	}
	else if (Player->IsMaxStaminaBuffActive())
	{
		OutData.MaxStaminaBuffAmount = Player->GetMaxStaminaBuffAmount();
		OutData.MaxStaminaBuffRemainingTime = Player->GetMaxStaminaBuffRemainingTime();
	}

	// 직감 남은 사용 횟수 (세이브 시에는 SaveManager가 먼저 전체 회복 → 가득 찬 값이 저장됨)
	OutData.SenseCharges = Player->GetCurrentSenseCharges();

	// 패시브 아이템 영구 효과 배율
	OutData.PermanentMoveSpeedMultiplier = Player->GetPermanentMoveSpeedMultiplier();
	if (const ULCOStaminaComponent* Stamina = Player->GetStamina())
	{
		OutData.PermanentRegenMultiplier = Stamina->GetPermanentRegenMultiplier();
	}

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

	// 4) 버프: 로딩 화면이 끝난 뒤 남은 시간만큼 다시 적용 (로딩 중에는 줄어들지 않음)
	PendingMoveSpeedBuffAmount = Data.MoveSpeedBuffAmount;
	PendingMoveSpeedBuffTime = Data.MoveSpeedBuffRemainingTime;
	PendingMaxStaminaBuffAmount = Data.MaxStaminaBuffAmount;
	PendingMaxStaminaBuffTime = Data.MaxStaminaBuffRemainingTime;
	ApplyPendingBuffsWhenLoaded();

	// 5) 직감 남은 사용 횟수 (기록이 없는 예전 세이브면 새 캐릭터 기본값 그대로)
	if (Data.SenseCharges >= 0)
	{
		Player->SetSenseCharges(Data.SenseCharges);
	}

	// 6) 패시브 아이템 영구 효과 (인벤토리는 그대로 복사해서 획득 효과가 다시 적용되지 않으므로 직접 복원)
	Player->SetPermanentMoveSpeedMultiplier(Data.PermanentMoveSpeedMultiplier);
	if (ULCOStaminaComponent* Stamina = Player->GetStamina())
	{
		Stamina->SetPermanentRegenMultiplier(Data.PermanentRegenMultiplier);
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] RestorePlayerCarryData: Items = %d, MoveSpeedBuff = %.1fs, MaxStaminaBuff = %.1fs, SenseCharges = %d, PermMoveSpeed = %.2f, PermRegen = %.2f"),
		Data.InventorySlots.Num(), Data.MoveSpeedBuffRemainingTime, Data.MaxStaminaBuffRemainingTime,
		Data.SenseCharges, Data.PermanentMoveSpeedMultiplier, Data.PermanentRegenMultiplier);
}

void ALCOGameMode::RestoreSavedPlayerTransform()
{
	ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>();
	FVector SavedLocation;
	FRotator SavedRotation;
	if (!SaveManager || !SaveManager->ConsumePendingPlayerTransform(SavedLocation, SavedRotation))
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] RestoreSavedPlayerTransform: Player not found"));
		return;
	}

	// 캐릭터는 좌우 방향만, 카메라는 저장할 때 보던 방향 그대로
	if (!PlayerPawn->TeleportTo(SavedLocation, FRotator(0.f, SavedRotation.Yaw, 0.f)))
	{
		// 저장한 자리가 막혀 있으면 (맵 수정 등) PlayerStart 그대로
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] Continue -> Saved location blocked (%s), keep PlayerStart"), *SavedLocation.ToString());
		return;
	}

	if (AController* PlayerController = PlayerPawn->GetController())
	{
		PlayerController->SetControlRotation(SavedRotation);
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] Continue -> Player moved to saved location (%s)"), *SavedLocation.ToString());
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
	const FString EntryTag = UGameplayStatics::ParseOption(OptionsString, EntryOption);

	// 도착 태그가 없으면(새 게임 / 이어하기 / 메뉴 없이 Play) 기본 시작 위치 태그 "Start" 사용
	const FName SearchTag = EntryTag.IsEmpty() ? DefaultPlayerStartTag : FName(*EntryTag);

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
			StorySubsystem->OnChapterAdvanced.RemoveDynamic(this, &ThisClass::HandleChapterAdvanced);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ALCOGameMode::HandleReportResolved(ELCOStoryFlag PresentedKeyword, ELCOReportResult Result)
{
	switch (Result)
	{
	case ELCOReportResult::Chapter1Clear:
		// 1막 끝 → 2막 시작 (StorySubsystem이 이미 Chapter2로 바꿔 둠)
		if (const ULCOStorySubsystem* StorySubsystem = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>())
		{
			StartChapterTransition(StorySubsystem->GetCurrentChapterID());
		}
		break;

	case ELCOReportResult::End3:
		// 진행 상태를 바꾸지 않았으므로 이어하기 = 마지막 저장 시점
		ReachEnding(LCOStoryIDs::End3);
		break;

	default:
		break;
	}
}

void ALCOGameMode::HandleChapterAdvanced(FName CompletedChapterID, FName NewChapterID)
{
	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] Chapter advanced: %s -> %s"), *CompletedChapterID.ToString(), *NewChapterID.ToString());
	StartChapterTransition(NewChapterID);
}

void ALCOGameMode::BindChasingManager()
{
	for (TActorIterator<ALCOChasingManager> It(GetWorld()); It; ++It)
	{
		It->OnCaughtSequenceFinished.AddUniqueDynamic(this, &ThisClass::HandleCaughtSequenceFinished);
	}
}

void ALCOGameMode::HandleCaughtSequenceFinished(AActor* Player)
{
	ULCOStorySubsystem* StorySubsystem = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>();
	if (!StorySubsystem)
	{
		return;
	}

	// 챕터 설정에서 압수를 켠 챕터만 (목록에 없는 1막은 압수 안 함)
	const FName ChapterID = StorySubsystem->GetCurrentChapterID();
	const FLCOChapterStartInfo* ChapterInfo = ChapterStartInfos.Find(ChapterID);
	if (!ChapterInfo || !ChapterInfo->bConfiscateNotebookOnCaught)
	{
		return;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] Caught -> Notebook confiscated (Chapter = %s)"), *ChapterID.ToString());
	StorySubsystem->SetStateFlag(LCOStoryIDs::NotebookConfiscatedFlag);
}

void ALCOGameMode::StartChapterTransition(FName NewChapterID)
{
	// 아직 없는 챕터 (출시 범위 끝): 시간대 · 이동 · 저장 없이 엔딩 → 이어하기는 마지막 저장 시점부터
	const FLCOChapterStartInfo* EndCheckInfo = ChapterStartInfos.Find(NewChapterID);
	if (EndCheckInfo && EndCheckInfo->bEndOfContent)
	{
		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartChapterTransition: %s is end of content -> To be continued"), *NewChapterID.ToString());
		ReachEnding(LCOStoryIDs::ToBeContinued);
		return;
	}

	// 시간대 먼저 진행. 맵 이동 전에 바꿔야 도착 자동 저장에 같이 저장됨
	AdvanceTimeOfDay(GetTimeOfDayForChapter(NewChapterID));

	const FLCOChapterStartInfo* StartInfo = ChapterStartInfos.Find(NewChapterID);
	if (StartInfo && !StartInfo->Level.IsNull())
	{
		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] StartChapterTransition: %s -> Travel to %s (Tag = %s)"),
			*NewChapterID.ToString(), *StartInfo->Level.ToString(), *StartInfo->PlayerStartTag.ToString());
		TravelToLevel(StartInfo->Level, StartInfo->PlayerStartTag, true);
		return;
	}

	// 시작 맵 미설정: 이동 없이 그 자리에서 자동 저장
	UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] StartChapterTransition: %s start level not set (ChapterStartInfos) -> AutoSave here"),
		*NewChapterID.ToString());
	if (ULCOSaveManager* SaveManager = GetGameInstance()->GetSubsystem<ULCOSaveManager>())
	{
		SaveManager->SaveGame();
	}
}

void ALCOGameMode::ReachEnding(FName EndingID)
{
	if (bEndingInProgress)
	{
		return;
	}
	bEndingInProgress = true;
	PendingEndingID = EndingID;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ReachEnding: %s -> Fade out (%.1fs)"), *EndingID.ToString(), EndingFadeOutDuration);

	// 이동 · 시점 잠금, 퀵슬롯 HUD 숨김 (레벨이 바뀌므로 풀지 않음)
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (PlayerController)
	{
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->SetIgnoreLookInput(true);
	}
	if (ALCOPlayerCharacter* Player = Cast<ALCOPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (ULCOQuickSlotComponent* QuickSlot = Player->GetQuickSlot())
		{
			QuickSlot->SetHUDHiddenByCutscene(true);
		}
	}

	// 화면을 검게 (끝난 뒤에도 검은 화면 유지)
	if (PlayerController && PlayerController->PlayerCameraManager && EndingFadeOutDuration > 0.f)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(0.f, 1.f, EndingFadeOutDuration, FLinearColor::Black, false, true);
		GetWorldTimerManager().SetTimer(EndingFadeTimer, this, &ThisClass::ShowEndingScreen, EndingFadeOutDuration, false);
		return;
	}

	ShowEndingScreen();
}

void ALCOGameMode::ShowEndingScreen()
{
	// 1) 엔딩 위젯이 지정되어 있으면 표시 → EndingScreenDuration 뒤 메인 메뉴
	const TSubclassOf<UUserWidget>* WidgetClass = EndingWidgets.Find(PendingEndingID);
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (WidgetClass && *WidgetClass && PlayerController)
	{
		EndingWidget = CreateWidget<UUserWidget>(PlayerController, *WidgetClass);
		if (EndingWidget)
		{
			EndingWidget->AddToViewport(100);

			// Log Test
			UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ShowEndingScreen: %s (%s, %.1fs)"),
				*PendingEndingID.ToString(), *EndingWidget->GetClass()->GetName(), EndingScreenDuration);

			if (EndingScreenDuration > 0.f)
			{
				GetWorldTimerManager().SetTimer(ReturnToMainMenuTimer, this, &ThisClass::ReturnToMainMenu, EndingScreenDuration, false);
			}
			else
			{
				ReturnToMainMenu();
			}
			return;
		}
	}

	// 2) BP에서 엔딩 화면을 직접 연결한 경우: 화면이 FinishEnding을 호출
	if (OnEndingReached.IsBound())
	{
		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ShowEndingScreen: %s -> OnEndingReached"), *PendingEndingID.ToString());
		OnEndingReached.Broadcast(PendingEndingID);
		return;
	}

	// 3) 엔딩 화면 없음: 잠시 후 메인 메뉴로
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ShowEndingScreen: %s -> No ending screen, return to main menu in %.1fs"),
		*PendingEndingID.ToString(), EndingReturnDelay);
	if (EndingReturnDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(ReturnToMainMenuTimer, this, &ThisClass::ReturnToMainMenu, EndingReturnDelay, false);
	}
	else
	{
		ReturnToMainMenu();
	}
}

void ALCOGameMode::FinishEnding()
{
	if (!bEndingInProgress)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] FinishEnding ignored: no ending in progress"));
		return;
	}

	ReturnToMainMenu();
}

void ALCOGameMode::ApplyPendingBuffsWhenLoaded()
{
	// 대기 중인 버프가 없으면 종료
	if (PendingMoveSpeedBuffTime <= 0.f && PendingMaxStaminaBuffTime <= 0.f)
	{
		GetWorldTimerManager().ClearTimer(PendingBuffTimer);
		return;
	}

	// 로딩 화면이 아직 떠 있으면 0.1초 뒤 다시 확인
	const ULCOLoadingSubsystem* LoadingSubsystem = GetGameInstance()->GetSubsystem<ULCOLoadingSubsystem>();
	if (LoadingSubsystem && LoadingSubsystem->IsLoading())
	{
		if (!GetWorldTimerManager().IsTimerActive(PendingBuffTimer))
		{
			GetWorldTimerManager().SetTimer(PendingBuffTimer, this, &ThisClass::ApplyPendingBuffsWhenLoaded, 0.1f, true);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(PendingBuffTimer);

	ALCOPlayerCharacter* Player = Cast<ALCOPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] ApplyPendingBuffsWhenLoaded: Player not found"));
		return;
	}

	if (PendingMoveSpeedBuffTime > 0.f)
	{
		Player->ApplyMoveSpeedBuff(PendingMoveSpeedBuffAmount, PendingMoveSpeedBuffTime);
	}

	if (PendingMaxStaminaBuffTime > 0.f)
	{
		Player->ApplyMaxStaminaBuff(PendingMaxStaminaBuffAmount, PendingMaxStaminaBuffTime);
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ApplyPendingBuffsWhenLoaded: MoveSpeedBuff = %.1fs, MaxStaminaBuff = %.1fs"),
		PendingMoveSpeedBuffTime, PendingMaxStaminaBuffTime);

	PendingMoveSpeedBuffAmount = 0.f;
	PendingMoveSpeedBuffTime = 0.f;
	PendingMaxStaminaBuffAmount = 0;
	PendingMaxStaminaBuffTime = 0.f;
}

void ALCOGameMode::ReturnToMainMenu()
{
	if (MainMenuLevel.IsNull())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] ReturnToMainMenu: MainMenuLevel not set (GameMode BP Class Defaults)"));
		return;
	}

	// 일시정지 상태로 레벨을 넘기지 않도록 해제
	UGameplayStatics::SetGamePaused(this, false);

	// 메인 메뉴 복귀 시 시작 방식 초기화 (일시정지 메뉴의 메인 메뉴 버튼과 같은 처리)
	if (ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>())
	{
		LCOGameInstance->ResetStartMode();
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ReturnToMainMenu: Open %s"), *MainMenuLevel.ToString());

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
}

void ALCOGameMode::TryAutoSaveOnArrival()
{
	if (!UGameplayStatics::HasOption(OptionsString, AutoSaveOption))
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

void ALCOGameMode::RefreshTimeOfDay()
{
	ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>();
	if (!LCOGameInstance)
	{
		return;
	}

	// 저장된 시간대가 현재 챕터 시간대보다 이르면 챕터 시간대로 맞춤 (이전 세이브, 테스트로 챕터만 바뀐 경우 등)
	const ELCOTimeOfDay PrevTimeOfDay = LCOGameInstance->GetCurrentTimeOfDay();
	ELCOTimeOfDay NewTimeOfDay = PrevTimeOfDay;
	if (const ULCOStorySubsystem* StorySubsystem = LCOGameInstance->GetSubsystem<ULCOStorySubsystem>())
	{
		NewTimeOfDay = FMath::Max(PrevTimeOfDay, GetTimeOfDayForChapter(StorySubsystem->GetCurrentChapterID()));
	}

	if (NewTimeOfDay != PrevTimeOfDay)
	{
		LCOGameInstance->SetCurrentTimeOfDay(NewTimeOfDay);
	}

	ALCOTimeOfDayManager* Manager = FindTimeOfDayManager();
	if (!Manager)
	{
		// 시간대 매니저가 없는 맵 (메인 메뉴, 택시 등): 맵 조명 그대로
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] RefreshTimeOfDay: No LCOTimeOfDayManager in this map"));
		return;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] RefreshTimeOfDay: %s (Map = %s)"),
		*UEnum::GetValueAsString(NewTimeOfDay), *GetWorld()->GetMapName());

	Manager->ApplyTimeOfDay(NewTimeOfDay);
}

void ALCOGameMode::AdvanceTimeOfDay(ELCOTimeOfDay NewTimeOfDay)
{
	ULCOGameInstance* LCOGameInstance = GetGameInstance<ULCOGameInstance>();
	if (!LCOGameInstance)
	{
		return;
	}

	// 시간대는 앞으로만 진행
	const ELCOTimeOfDay PrevTimeOfDay = LCOGameInstance->GetCurrentTimeOfDay();
	if (NewTimeOfDay <= PrevTimeOfDay)
	{
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] AdvanceTimeOfDay ignored: %s -> %s"),
			*UEnum::GetValueAsString(PrevTimeOfDay), *UEnum::GetValueAsString(NewTimeOfDay));
		return;
	}

	LCOGameInstance->SetCurrentTimeOfDay(NewTimeOfDay);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] AdvanceTimeOfDay: %s -> %s"),
		*UEnum::GetValueAsString(PrevTimeOfDay), *UEnum::GetValueAsString(NewTimeOfDay));

	if (ALCOTimeOfDayManager* Manager = FindTimeOfDayManager())
	{
		Manager->ApplyTimeOfDay(NewTimeOfDay);
	}
}

ELCOTimeOfDay ALCOGameMode::GetTimeOfDayForChapter(FName ChapterID) const
{
	const FLCOChapterStartInfo* ChapterInfo = ChapterStartInfos.Find(ChapterID);
	return ChapterInfo ? ChapterInfo->TimeOfDay : ELCOTimeOfDay::Noon;
}

void ALCOGameMode::ApplyTimeOfDayForTest(ELCOTimeOfDay TimeOfDay)
{
	if (ALCOTimeOfDayManager* Manager = FindTimeOfDayManager())
	{
		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameMode] ApplyTimeOfDayForTest: %s"), *UEnum::GetValueAsString(TimeOfDay));
		Manager->ApplyTimeOfDay(TimeOfDay);
	}
	else
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameMode] ApplyTimeOfDayForTest: No LCOTimeOfDayManager in this map"));
	}
}

ALCOTimeOfDayManager* ALCOGameMode::FindTimeOfDayManager() const
{
	for (TActorIterator<ALCOTimeOfDayManager> It(GetWorld()); It; ++It)
	{
		// 공용 조명 값 전달 (비어 있으면 매니저가 맵 값 사용)
		It->SetSharedPresets(TimeOfDayPresets);
		return *It;
	}
	return nullptr;
}
