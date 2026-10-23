#include "AI/Chasing/LCOChasingManager.h"

#include "AI/LCOCaretakerAIController.h"
#include "AIController.h"
#include "AI/SuspicionComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Interaction/LCOHidingSpot.h"
#include "Kismet/GameplayStatics.h"
#include "NPC/LCOCaretaker.h"
#include "NPC/LCOSigolYouth.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

#pragma region 초기화와 의심도 바인딩

ALCOChasingManager::ALCOChasingManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALCOChasingManager::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어가 아직 생성되지 않았을 수 있어 바인딩될 때까지 재시도한다.
	TryBindSuspicion();
	if (!BoundSuspicion.IsValid())
	{
		GetWorldTimerManager().SetTimer(SuspicionBindTimer, this, &ALCOChasingManager::TryBindSuspicion, 0.5f, true);
	}

	// 레벨의 모든 은신처에 바인딩한다.
	TArray<AActor*> HidingSpots;
	UGameplayStatics::GetAllActorsOfClass(this, ALCOHidingSpot::StaticClass(), HidingSpots);
	for (AActor* Actor : HidingSpots)
	{
		if (ALCOHidingSpot* HidingSpot = Cast<ALCOHidingSpot>(Actor))
		{
			HidingSpot->OnEnteredDelegate.AddUniqueDynamic(this, &ALCOChasingManager::HandleHidingSpotEntered);
		}
	}
}

void ALCOChasingManager::TryBindSuspicion()
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	USuspicionComponent* Suspicion = PlayerPawn ? PlayerPawn->FindComponentByClass<USuspicionComponent>() : nullptr;
	if (!Suspicion)
	{
		return;
	}

	BoundSuspicion = Suspicion;
	Suspicion->OnSuspicionMax.AddDynamic(this, &ALCOChasingManager::HandleSuspicionMax);
	GetWorldTimerManager().ClearTimer(SuspicionBindTimer);
}

void ALCOChasingManager::HandleSuspicionMax()
{
	StartChasing();
}

#pragma endregion

#pragma region 체이싱 시작과 종료

void ALCOChasingManager::StartChasing()
{
	if (bIsChasing)	return;

	// 안전지대(로비) 안에서는 체이싱을 시작하지 않고 의심도를 되돌린다.
	// 의심도 변경 알림 안에서 다시 변경하지 않도록 다음 틱에 처리한다.
	if (IsPlayerInSafeZone())
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (BoundSuspicion.IsValid())
			{
				BoundSuspicion->ResetSuspicion();
			}
		}));
		return;
	}

	bIsChasing = true;

	// 이전 체이싱에서 남은 청년이 있으면 정리한다.
	CleanupYouth();

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	FVector SpawnLocation;
	if (PlayerPawn && FindSpawnLocation(PlayerPawn->GetActorLocation(), SpawnLocation))
	{
		SpawnYouth(SpawnLocation);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOChasingManager] No valid spawn location. Chasing starts without the youth."));
	}

	GetWorldTimerManager().SetTimer(FoundCheckTimer, this, &ALCOChasingManager::CheckFoundYouth, FoundCheckInterval, true);

	// 플레이어에게 청년까지의 최단 경로를 발자국으로 안내한다.
	if (bShowPathGuide && SpawnedYouth.IsValid())
	{
		ResetPathGuide();
		UpdatePathGuide();
		GetWorldTimerManager().SetTimer(PathGuideTimer, this, &ALCOChasingManager::UpdatePathGuide, GuideUpdateInterval, true);
		GetWorldTimerManager().SetTimer(PassCheckTimer, this, &ALCOChasingManager::CheckPassedFootprints, FootprintPassCheckInterval, true);
	}
	OnChasingStarted.Broadcast(SpawnedYouth.Get());
}

void ALCOChasingManager::EndChasing(ELCOChasingEndReason Reason)
{
	if (!bIsChasing)
	{
		return;
	}

	bIsChasing = false;
	GetWorldTimerManager().ClearTimer(FoundCheckTimer);
	GetWorldTimerManager().ClearTimer(PathGuideTimer);
	GetWorldTimerManager().ClearTimer(PassCheckTimer);
	ResetPathGuide();

	// 시골청년을 발견한 경우에는 청년을 지우지 않고 대화 흐름으로 넘긴다. 그 외에는 바로 제거한다.
	if (Reason == ELCOChasingEndReason::FoundYouth && SpawnedYouth.IsValid())
	{
		BeginYouthEncounter();
	}
	else if (ALCOSigolYouth* Youth = SpawnedYouth.Get())
	{
		Youth->Destroy();
	}
	SpawnedYouth.Reset();

	// 관리인의 추격을 멈추고 순찰로 되돌린다.
	TArray<AActor*> Caretakers;
	UGameplayStatics::GetAllActorsOfClass(this, ALCOCaretaker::StaticClass(), Caretakers);
	for (const AActor* Caretaker : Caretakers)
	{
		if (const APawn* CaretakerPawn = Cast<APawn>(Caretaker))
		{
			if (ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(CaretakerPawn->GetController()))
			{
				Controller->StopChase();
			}
		}
	}

	// 의심도를 초기화하고, 잠시 고정해 바로 다시 추격되지 않게 한다.
	if (USuspicionComponent* Suspicion = BoundSuspicion.Get())
	{
		Suspicion->ResetSuspicion();

		if (PostChaseCooldown > 0.f)
		{
			Suspicion->SetFrozen(true);
			GetWorldTimerManager().SetTimer(CooldownTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (BoundSuspicion.IsValid())
				{
					BoundSuspicion->SetFrozen(false);
				}
			}), PostChaseCooldown, false);
		}
	}

	OnChasingEnded.Broadcast(Reason);
}

void ALCOChasingManager::NotifyPlayerEnteredSafeZone()
{
	++SafeZoneCount;

	// 체이싱 중 안전지대에 들어가면 시골청년을 찾지 않아도 즉시 종료한다.
	if (bIsChasing)
	{
		EndChasing(ELCOChasingEndReason::LobbyEntry);
	}
}

void ALCOChasingManager::NotifyPlayerExitedSafeZone()
{
	SafeZoneCount = FMath::Max(0, SafeZoneCount - 1);
}

bool ALCOChasingManager::FindSpawnLocation(const FVector& PlayerLocation, FVector& OutLocation) const
{
	// 후보 중 플레이어까지 NavMesh 경로가 가장 긴 위치를 고른다. 도달할 수 없는 후보는 제외한다.
	auto PickFarthest = [this, &PlayerLocation, &OutLocation](const TArray<TObjectPtr<ATargetPoint>>& Points, float MinPathLength)
	{
		double BestLength = -1.0;
		for (const ATargetPoint* Point : Points)
		{
			if (!Point)
			{
				continue;
			}

			double PathLength = 0.0;
			if (UNavigationSystemV1::GetPathLength(GetWorld(), PlayerLocation, Point->GetActorLocation(), PathLength) != ENavigationQueryResult::Success)
			{
				continue;
			}

			if (PathLength >= MinPathLength && PathLength > BestLength)
			{
				BestLength = PathLength;
				OutLocation = Point->GetActorLocation();
			}
		}
		return BestLength >= 0.0;
	};

	if (PickFarthest(SpawnCandidates, MinSpawnPathLength) || PickFarthest(FallbackPoints, 0.f))
	{
		return true;
	}

	// 플레이어가 막힌 곳(닫힌 문 안쪽 등)에 있어 어떤 후보까지도 NavMesh 경로가 없을 때는, 도달 가능 여부를 따지지 않고
	// 직선 거리가 가장 먼 후보에 스폰한다. 청년은 길이 열렸을 때 갈 수 있는 위치에 있으면 되고, 길 안내는 길이 열릴 때까지 기다린다.
	double BestDistance = -1.0;
	auto PickFarthestStraight = [&PlayerLocation, &OutLocation, &BestDistance](const TArray<TObjectPtr<ATargetPoint>>& Points)
	{
		for (const ATargetPoint* Point : Points)
		{
			if (!Point)
			{
				continue;
			}

			const double Distance = FVector::Dist(PlayerLocation, Point->GetActorLocation());
			if (Distance > BestDistance)
			{
				BestDistance = Distance;
				OutLocation = Point->GetActorLocation();
			}
		}
	};
	PickFarthestStraight(SpawnCandidates);
	PickFarthestStraight(FallbackPoints);

	if (BestDistance >= 0.0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOChasingManager] No reachable spawn candidate (the player may be in a blocked area). Spawning the youth at the farthest candidate by straight distance."));
		return true;
	}

	return false;
}

void ALCOChasingManager::SpawnYouth(const FVector& Location)
{
	if (!SigolYouthClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOChasingManager] SigolYouthClass is not set."));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnedYouth = GetWorld()->SpawnActor<ALCOSigolYouth>(SigolYouthClass, Location, FRotator::ZeroRotator, Params);
}

void ALCOChasingManager::CheckFoundYouth()
{
	const ALCOSigolYouth* Youth = SpawnedYouth.Get();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Youth || !PlayerPawn)
	{
		return;
	}

	if (FVector::Dist(PlayerPawn->GetActorLocation(), Youth->GetActorLocation()) > FoundDistance)
	{
		return;
	}

	// 사이에 벽 등이 있으면 발견으로 보지 않는다.
	if (bRequireLineOfSight)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOFoundYouthTrace), false, PlayerPawn);
		Params.AddIgnoredActor(Youth);

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, PlayerPawn->GetPawnViewLocation(), Youth->GetActorLocation(), ECC_Visibility, Params))
		{
			return;
		}
	}

	EndChasing(ELCOChasingEndReason::FoundYouth);
}

#pragma endregion

#pragma region 옷장 은신과 끌어내기

void ALCOChasingManager::HandleHidingSpotEntered(ALCOHidingSpot* HidingSpot, AActor* Hider)
{
	// 플레이어가 숨은 경우만 처리한다.
	const APawn* HiderPawn = Cast<APawn>(Hider);
	if (!HiderPawn || !HiderPawn->IsPlayerControlled())	return;

	// 따돌리지 못했으면(관리인에게 보이는 상태로 숨음) 관리인이 은신처로 와서 끌어낸 뒤 잡는다.
	if (bIsChasing && !TryEndChasingByHiding(HidingSpot))
	{
		StartExtractByCaretakers(HidingSpot);
	}
}

void ALCOChasingManager::StartExtractByCaretakers(ALCOHidingSpot* HidingSpot)
{
	TArray<AActor*> Caretakers;
	UGameplayStatics::GetAllActorsOfClass(this, ALCOCaretaker::StaticClass(), Caretakers);
	for (const AActor* Caretaker : Caretakers)
	{
		if (const APawn* CaretakerPawn = Cast<APawn>(Caretaker))
		{
			if (ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(CaretakerPawn->GetController()))
			{
				Controller->StartExtractHider(HidingSpot);
			}
		}
	}
}

bool ALCOChasingManager::TryEndChasingByHiding(AActor* HidingSpot)
{
	if (!bIsChasing)
	{
		return false;
	}

	// 관리인이 지켜보는 앞에서 숨은 것은 따돌린 것이 아니다.
	if (IsPlayerSpottedByCaretaker(HidingSpot))
	{
		return false;
	}

	EndChasing(ELCOChasingEndReason::Hidden);
	return true;
}

bool ALCOChasingManager::IsPlayerSpottedByCaretaker(const AActor* HidingSpot) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	TArray<AActor*> Caretakers;
	UGameplayStatics::GetAllActorsOfClass(this, ALCOCaretaker::StaticClass(), Caretakers);
	for (const AActor* Caretaker : Caretakers)
	{
		const APawn* CaretakerPawn = Cast<APawn>(Caretaker);
		if (!CaretakerPawn)
		{
			continue;
		}

		// 시야 거리와 시야각은 관리인 컨트롤러의 SightRadius, PeripheralVisionAngle을 따른다.
		const ALCOCaretakerAIController* CaretakerController = Cast<ALCOCaretakerAIController>(CaretakerPawn->GetController());
		const float ViewDistance = CaretakerController ? CaretakerController->GetViewDistance() : 1500.f;
		const float ViewHalfAngle = CaretakerController ? CaretakerController->GetViewHalfAngle() : 70.f;

		// 거리가 멀면 보이지 않는다.
		if (FVector::Dist(CaretakerPawn->GetActorLocation(), PlayerPawn->GetActorLocation()) > ViewDistance)
		{
			continue;
		}

		// 관리인의 정면 시야각 밖이면 보이지 않는다.
		const FVector ToPlayer = (PlayerPawn->GetActorLocation() - CaretakerPawn->GetActorLocation()).GetSafeNormal();
		const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(CaretakerPawn->GetActorForwardVector(), ToPlayer)));
		if (AngleDeg > ViewHalfAngle)
		{
			continue;
		}

		// 사이에 벽 등이 있으면 보이지 않는다. 은신처 자체는 가리개로 보지 않는다.
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOHideSpotTrace), false, CaretakerPawn);
		Params.AddIgnoredActor(PlayerPawn);
		if (HidingSpot)
		{
			Params.AddIgnoredActor(HidingSpot);
		}

		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, CaretakerPawn->GetPawnViewLocation(), PlayerPawn->GetActorLocation(), ECC_Visibility, Params))
		{
			return true;
		}
	}

	return false;
}

#pragma endregion

#pragma region 잡기 연출

bool ALCOChasingManager::BeginCaughtSequence(AActor* InCaughtPlayer)
{
	if (bCaughtSequenceActive)
	{
		return true;
	}

	bCaughtSequenceActive = true;
	CaughtPlayer = InCaughtPlayer;

	// 관리인을 멈추고 의심도를 고정해 연출 중에 상태가 바뀌지 않게 한다.
	TArray<AActor*> Caretakers;
	UGameplayStatics::GetAllActorsOfClass(this, ALCOCaretaker::StaticClass(), Caretakers);
	for (const AActor* Caretaker : Caretakers)
	{
		if (const APawn* CaretakerPawn = Cast<APawn>(Caretaker))
		{
			if (ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(CaretakerPawn->GetController()))
			{
				Controller->HoldForCaught();
			}
		}
	}

	if (BoundSuspicion.IsValid())
	{
		BoundSuspicion->SetFrozen(true);
	}

	// 연출이 연결되어 있으면 FinishCaughtSequence()가 호출되기를 기다린다. 없으면 바로 진행한다.
	if (OnPlayerCaught.IsBound())
	{
		GetWorldTimerManager().SetTimer(CaughtTimeoutTimer, this, &ALCOChasingManager::FinishCaughtSequence, CaughtSequenceTimeout, false);
		OnPlayerCaught.Broadcast(InCaughtPlayer);
	}
	else
	{
		FinishCaughtSequence();
	}

	return true;
}

void ALCOChasingManager::FinishCaughtSequence()
{
	if (!bCaughtSequenceActive)
	{
		return;
	}

	bCaughtSequenceActive = false;
	GetWorldTimerManager().ClearTimer(CaughtTimeoutTimer);

	// 플레이어를 시작 지점으로 이동
	AActor* Player = CaughtPlayer.Get();
	CaughtPlayer.Reset();
	TeleportPlayerToStart(this, Cast<APawn>(Player));

	// 관리인을 최초 위치로 되돌리고 순찰로 복귀
	TArray<AActor*> Caretakers;
	UGameplayStatics::GetAllActorsOfClass(this, ALCOCaretaker::StaticClass(), Caretakers);
	for (const AActor* Caretaker : Caretakers)
	{
		if (const APawn* CaretakerPawn = Cast<APawn>(Caretaker))
		{
			if (ALCOCaretakerAIController* Controller = Cast<ALCOCaretakerAIController>(CaretakerPawn->GetController()))
			{
				Controller->ResetAfterCaught();
			}
		}
	}

	// 의심도 초기화. 체이싱 중이었다면 종료 처리(시골청년 제거 등)를 하고, 아니면 의심도만 초기화한다.
	if (USuspicionComponent* Suspicion = BoundSuspicion.Get())
	{
		Suspicion->SetFrozen(false);
		Suspicion->ResetSuspicion();
	}
	EndChasing(ELCOChasingEndReason::Caught);

	OnCaughtSequenceFinished.Broadcast(Player);
}

void ALCOChasingManager::TeleportPlayerToStart(UObject* WorldContextObject, APawn* PlayerPawn)
{
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(WorldContextObject);
	const AActor* PlayerStart = (GameMode && PlayerPawn) ? GameMode->FindPlayerStart(PlayerPawn->GetController()) : nullptr;
	if (!PlayerPawn || !PlayerStart)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOChasingManager] Player Start not found. Player was not reset."));
		return;
	}

	PlayerPawn->TeleportTo(PlayerStart->GetActorLocation(), PlayerStart->GetActorRotation());
	if (UPawnMovementComponent* PlayerMovement = PlayerPawn->GetMovementComponent())
	{
		PlayerMovement->StopMovementImmediately();
	}
}

#pragma endregion

#pragma region 시골청년 발견 후 흐름

void ALCOChasingManager::BeginYouthEncounter()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	ALCOSigolYouth* Youth = SpawnedYouth.Get();
	if (!PlayerPawn || !Youth)
	{
		return;
	}

	// 한 번에 한 명만 대화 흐름을 타는다.
	CleanupYouth();
	EncounterYouth = Youth;

	// 청년이 플레이어에게 다가온다.
	Youth->SetRunSpeed();
	if (AAIController* AI = Cast<AAIController>(Youth->GetController()))
	{
		AI->MoveToActor(PlayerPawn, YouthTalkDistance * 0.5f);
	}

	YouthPhaseStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(YouthApproachTimer, this, &ALCOChasingManager::CheckYouthApproach, 0.2f, true);
}

void ALCOChasingManager::CheckYouthApproach()
{
	const ALCOSigolYouth* Youth = EncounterYouth.Get();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Youth || !PlayerPawn)
	{
		CleanupYouth();
		return;
	}

	// 가까워졌거나, 이동에 실패해 시간이 지나면 그 자리에서 대화를 시작한다.
	if (FVector::Dist(Youth->GetActorLocation(), PlayerPawn->GetActorLocation()) <= YouthTalkDistance
		|| GetWorld()->GetTimeSeconds() - YouthPhaseStartTime >= YouthApproachTimeout)
	{
		GetWorldTimerManager().ClearTimer(YouthApproachTimer);
		StartYouthTalk();
	}
}

void ALCOChasingManager::StartYouthTalk()
{
	ALCOSigolYouth* Youth = EncounterYouth.Get();
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Youth || !PlayerPawn)
	{
		CleanupYouth();
		return;
	}

	if (AAIController* AI = Cast<AAIController>(Youth->GetController()))
	{
		AI->StopMovement();
	}
	Youth->SetWalkSpeed();

	Youth->OnConversationEnded.AddUniqueDynamic(this, &ALCOChasingManager::HandleYouthConversationEnded);
	Youth->BeginConversation(PlayerPawn);

	// 대화 UI가 연결되기 전 테스트용 자동 종료
	if (YouthTestAutoEndTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(YouthAutoEndTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (ALCOSigolYouth* EncounterNPC = EncounterYouth.Get())
			{
				EncounterNPC->EndConversation();
			}
		}), YouthTestAutoEndTime, false);
	}
}

void ALCOChasingManager::HandleYouthConversationEnded()
{
	GetWorldTimerManager().ClearTimer(YouthAutoEndTimer);

	if (ALCOSigolYouth* Youth = EncounterYouth.Get())
	{
		Youth->OnConversationEnded.RemoveDynamic(this, &ALCOChasingManager::HandleYouthConversationEnded);
	}

	StartYouthLeave();
}

void ALCOChasingManager::StartYouthLeave()
{
	ALCOSigolYouth* Youth = EncounterYouth.Get();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Youth || !PlayerPawn)
	{
		CleanupYouth();
		return;
	}

	// 플레이어 반대 방향으로 걸어갈 목적지를 고른다. 청년 위치에서 여러 방향으로 NavMesh 위 직선을 쏘아, 벽에 막히기 전까지 가장 멀리 가면서
	// 플레이어에게서 가장 멀어지는 곳을 택한다. (복도를 따라 걷고, 플레이어 쪽으로 되돌아오지 않는다)
	const FVector YouthLocation = Youth->GetActorLocation();
	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const FVector ProjectExtent(50.f, 50.f, 100.f);
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());

	constexpr int32 DirectionCount = 16;
	bHasYouthLeaveDestination = false;
	double BestScore = -1.0;
	for (int32 i = 0; i < DirectionCount; ++i)
	{
		const FVector Direction = FRotator(0.f, i * (360.f / DirectionCount), 0.f).Vector();

		FVector HitLocation = YouthLocation;
		FVector End = YouthLocation + Direction * YouthLeaveWalkDistance;
		if (UNavigationSystemV1::NavigationRaycast(this, YouthLocation, End, HitLocation))
		{
			End = HitLocation; // 벽에 막히면 그 앞까지
		}

		// 벽 가장자리에 붙지 않도록 조금 안쪽으로 당긴다.
		End -= Direction * 50.f;

		FNavLocation Projected;
		if (!NavSys || !NavSys->ProjectPointToNavigation(End, Projected, ProjectExtent))
		{
			continue;
		}

		// 너무 가까운 곳(막다른 방향)은 제외하고, 플레이어에게서 가장 멀어지는 곳을 고른다.
		if (FVector::Dist2D(YouthLocation, Projected.Location) < 300.f)
		{
			continue;
		}

		double PathLength = 0.0;
		if (UNavigationSystemV1::GetPathLength(GetWorld(), YouthLocation, Projected.Location, PathLength) != ENavigationQueryResult::Success)
		{
			continue;
		}

		const double Score = FVector::Dist2D(Projected.Location, PlayerLocation);
		if (Score > BestScore)
		{
			BestScore = Score;
			YouthLeaveDestination = Projected.Location;
			bHasYouthLeaveDestination = true;
		}
	}

	if (bHasYouthLeaveDestination)
	{
		if (AAIController* AI = Cast<AAIController>(Youth->GetController()))
		{
			AI->MoveToLocation(YouthLeaveDestination, 100.f);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOChasingManager] No valid direction to walk away. The youth disappears after the timeout."));
	}

	// 목적지 도착 / 시야 밖 / 시간 초과 중 먼저 만족하는 조건으로 제거한다.
	YouthPhaseStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(YouthLeaveTimer, this, &ALCOChasingManager::CheckYouthLeave, 0.3f, true);
}

void ALCOChasingManager::CheckYouthLeave()
{
	const ALCOSigolYouth* Youth = EncounterYouth.Get();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Youth || !PlayerPawn)
	{
		CleanupYouth();
		return;
	}

	// 1) 시간 초과
	if (GetWorld()->GetTimeSeconds() - YouthPhaseStartTime >= YouthLeaveTimeout)
	{
		CleanupYouth();
		return;
	}

	// 2) 목적지 도착
	if (bHasYouthLeaveDestination && FVector::Dist2D(Youth->GetActorLocation(), YouthLeaveDestination) <= 150.f)
	{
		CleanupYouth();
		return;
	}

	// 3) 플레이어 시야 밖에서 충분히 멀리 떨어짐 (눈앞에서 사라지는 어색함 방지)
	if (FVector::Dist(Youth->GetActorLocation(), PlayerPawn->GetActorLocation()) >= YouthDespawnDistance && IsOutOfPlayerView(Youth))
	{
		CleanupYouth();
	}
}

void ALCOChasingManager::CleanupYouth()
{
	GetWorldTimerManager().ClearTimer(YouthApproachTimer);
	GetWorldTimerManager().ClearTimer(YouthAutoEndTimer);
	GetWorldTimerManager().ClearTimer(YouthLeaveTimer);
	bHasYouthLeaveDestination = false;

	if (ALCOSigolYouth* Youth = EncounterYouth.Get())
	{
		Youth->OnConversationEnded.RemoveDynamic(this, &ALCOChasingManager::HandleYouthConversationEnded);
		if (Youth->IsInConversation())
		{
			Youth->EndConversation();
		}
		Youth->Destroy();
	}
	EncounterYouth.Reset();
}

bool ALCOChasingManager::IsOutOfPlayerView(const AActor* Target) const
{
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Camera || !PlayerPawn || !Target)
	{
		return true;
	}

	// 카메라 정면에서 크게 벗어난 각도면 화면 밖이다.
	const FVector ToTarget = (Target->GetActorLocation() - Camera->GetCameraLocation()).GetSafeNormal();
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Camera->GetCameraRotation().Vector(), ToTarget)));
	if (AngleDeg > 70.f)
	{
		return true;
	}

	// 화면 안이라도 사이에 벽이 있으면 보이지 않는다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LCOYouthViewTrace), false, PlayerPawn);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Camera->GetCameraLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
}

#pragma endregion

#pragma region 길 안내 발자국

void ALCOChasingManager::UpdatePathGuide()
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const ALCOSigolYouth* Youth = SpawnedYouth.Get();
	if (!bIsChasing || !PlayerPawn || !Youth)
	{
		ResetPathGuide();
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const double Now = GetWorld()->GetTimeSeconds();

	if (!bHasRoute)
	{
		// 경로를 아직 못 구했다. (길이 막혀 청년까지 닿지 못하는 경우 등) 길이 열릴 때까지 갱신마다 다시 시도한다.
		BuildGuideRoute(PlayerLocation, Youth->GetActorLocation());
	}
	else
	{
		// 플레이어가 안내 경로에서 크게 벗어났는지 확인한다.
		float Progress = 0.f;
		float DistanceToRoute = 0.f;
		GetPlayerRouteProgress(PlayerLocation, Progress, DistanceToRoute);

		if (DistanceToRoute > GuideDeviationDistance)
		{
			if (OffRouteStartTime < 0.0)
			{
				OffRouteStartTime = Now;
			}
			else if (Now - OffRouteStartTime >= GuideRerouteDelay)
			{
				// 다른 방향으로 간 채 일정 시간이 지났다. 기존 발자국을 지우고 지금 위치에서 다시 그린다.
				ResetPathGuide();
				BuildGuideRoute(PlayerLocation, Youth->GetActorLocation());
			}
		}
		else
		{
			OffRouteStartTime = -1.0;
		}
	}

	MarkPassedFootprints();
	RefreshFootprintDecals();

	if (bDrawGuideLine)
	{
		DrawGuideLineDebug();
	}
}

bool ALCOChasingManager::BuildGuideRoute(const FVector& StartLocation, const FVector& TargetLocation)
{
	bHasRoute = false;
	PlacedFootprints.Reset();
	RoutePoints.Reset();
	RouteCumulative.Reset();

	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), StartLocation, TargetLocation);

	// 길이 막혀 청년까지 닿지 못하는 경로(부분 경로)는 쓰지 않는다. 길이 열리면 다음 갱신에서 다시 구한다.
	if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
	{
		return false;
	}

	// 점 목록의 누적 거리를 구한다.
	auto MakeCumulative = [](const TArray<FVector>& Points, TArray<float>& OutCumulative)
	{
		OutCumulative.Reset();
		OutCumulative.Add(0.f);
		for (int32 i = 1; i < Points.Num(); ++i)
		{
			OutCumulative.Add(OutCumulative.Last() + FVector::Dist(Points[i - 1], Points[i]));
		}
	};

	// 선 위 거리 D에 해당하는 위치와 진행 방향
	auto PointAt = [](const TArray<FVector>& Points, const TArray<float>& Cumulative, float Distance, FVector& OutLocation, FVector& OutDirection)
	{
		int32 Index = 0;
		while (Index < Points.Num() - 2 && Cumulative[Index + 1] < Distance)
		{
			++Index;
		}

		const float SegmentLength = FMath::Max(Cumulative[Index + 1] - Cumulative[Index], KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp((Distance - Cumulative[Index]) / SegmentLength, 0.f, 1.f);
		OutLocation = FMath::Lerp(Points[Index], Points[Index + 1], Alpha);
		OutDirection = (Points[Index + 1] - Points[Index]).GetSafeNormal();
	};

	// ---- 1) 엔진이 구한 최단 경로(꼭짓점 목록)를 일정 간격으로 촘촘하게 나눈다.
	TArray<float> SourceCumulative;
	MakeCumulative(Path->PathPoints, SourceCumulative);
	const float SourceLength = SourceCumulative.Last();

	TArray<FVector> Line;
	const float Step = FMath::Max(GuideLineStep, 20.f);
	for (float Distance = 0.f; Distance < SourceLength; Distance += Step)
	{
		FVector Location, Direction;
		PointAt(Path->PathPoints, SourceCumulative, Distance, Location, Direction);
		Line.Add(Location);
	}
	Line.Add(Path->PathPoints.Last());
	DebugRawLine = Line;

	// ---- 2) 안내선 전체를 벽과의 거리까지 반영해 미리 만든다. (발자국마다 따로 계산하지 않는다)
	//         가장 가까운 벽의 반대쪽으로 미는 대신, 경로의 왼쪽/오른쪽으로 벽까지의 거리를 각각 재서 "옆으로 옮길 거리" 하나(왼쪽이 +)로 다룬다.
	//         양쪽 벽 사이를 오가며 방향이 뒤집히는 일이 없고, 복도 한가운데를 넘어서까지 옮기지 않는다.
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	const FVector ProjectExtent(30.f, 30.f, 100.f);

	// 발자국이 이 위치에서 좌우로 FootprintSideOffset만큼 더 벌어지므로, 그만큼 더 띄운다.
	const float Needed = FootprintWallClearance + FootprintSideOffset;
	const float ProbeRange = FMath::Max(Needed * 2.f, 300.f);

	// 점에서 방향 Dir로 NavMesh 가장자리(벽)까지의 수평 거리. 가장자리가 없으면 ProbeRange.
	auto DistanceToBorder = [this, ProbeRange](const FVector& Origin, const FVector& Dir)
	{
		FVector HitLocation = Origin;
		if (UNavigationSystemV1::NavigationRaycast(this, Origin, Origin + Dir * ProbeRange, HitLocation))
		{
			return static_cast<float>(FVector::Dist2D(Origin, HitLocation));
		}
		return ProbeRange;
	};

	// 2-1) 각 점에서 좌우 벽까지의 거리를 재서 옆으로 옮길 거리를 구한다.
	TArray<FVector> Lefts;
	TArray<float> Offsets;
	Lefts.Init(FVector::LeftVector, Line.Num());
	Offsets.Init(0.f, Line.Num());
	if (FootprintWallClearance > 0.f)
	{
		for (int32 i = 0; i < Line.Num(); ++i)
		{
			FVector Direction = Line[FMath::Min(i + 1, Line.Num() - 1)] - Line[FMath::Max(i - 1, 0)];
			Direction.Z = 0.f;
			Direction = Direction.GetSafeNormal();
			if (Direction.IsNearlyZero())
			{
				continue;
			}

			// Direction x Up은 진행 방향의 왼쪽이다. (언리얼은 앞 +X, 오른쪽 +Y, 위 +Z라서 X x Z = -Y = 왼쪽)
			const FVector Left = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
			Lefts[i] = Left;

			const float DistanceLeft = DistanceToBorder(Line[i], Left);
			const float DistanceRight = DistanceToBorder(Line[i], -Left);

			float Offset = 0.f;
			if (DistanceLeft < Needed && DistanceRight < Needed)
			{
				// 양쪽 다 모자라는 좁은 곳은 가운데로 옮긴다.
				Offset = (DistanceLeft - DistanceRight) * 0.5f;
			}
			else if (DistanceLeft < Needed)
			{
				// 왼쪽 벽이 가깝다: 오른쪽으로 옮기되, 복도 가운데를 넘지 않는다.
				Offset = -FMath::Min(Needed - DistanceLeft, (DistanceRight - DistanceLeft) * 0.5f);
			}
			else if (DistanceRight < Needed)
			{
				// 오른쪽 벽이 가깝다: 왼쪽으로 옮기되, 복도 가운데를 넘지 않는다.
				Offset = FMath::Min(Needed - DistanceRight, (DistanceLeft - DistanceRight) * 0.5f);
			}

			Offsets[i] = FMath::Clamp(Offset, -FootprintMaxWallPush, FootprintMaxWallPush);
		}
	}

	// 2-2) 옮긴 정도를 코너 앞뒤로 완만하게 이어 붙인다. 평균을 내면 희석되어 다시 벽에 붙으므로, 거리에 따라 줄어드는 값 중 절댓값이 가장 큰 것을 쓴다.
	//      (코너에서 가장 많이 옮긴 값은 그대로 두고, 앞뒤로 FootprintWallBlendDistance까지 서서히 줄어든다)
	TArray<float> Blended = Offsets;
	if (FootprintWallBlendDistance > 0.f)
	{
		const int32 BlendSteps = FMath::CeilToInt(FootprintWallBlendDistance / Step);
		for (int32 i = 0; i < Line.Num(); ++i)
		{
			for (int32 k = 1; k <= BlendSteps; ++k)
			{
				const float Weight = 1.f - (k * Step) / FootprintWallBlendDistance;
				if (Weight <= 0.f)
				{
					break;
				}

				for (const int32 Neighbor : {i - k, i + k})
				{
					if (Offsets.IsValidIndex(Neighbor))
					{
						const float Candidate = Offsets[Neighbor] * Weight;
						if (FMath::Abs(Candidate) > FMath::Abs(Blended[i]))
						{
							Blended[i] = Candidate;
						}
					}
				}
			}
		}
	}

	// 2-3) 옮기는 방향이 바뀌는 곳(왼쪽 벽 구간 -> 오른쪽 벽 구간)이 급하게 꺾이지 않도록 옮길 거리를 한 번 다듬는다.
	for (int32 Pass = 0; Pass < GuideLineSmoothPasses; ++Pass)
	{
		TArray<float> Smoothed = Blended;
		for (int32 i = 1; i < Blended.Num() - 1; ++i)
		{
			Smoothed[i] = Blended[i - 1] * 0.25f + Blended[i] * 0.5f + Blended[i + 1] * 0.25f;
		}
		Blended = Smoothed;
	}

	// 2-4') 옮긴 점을 걸을 수 있는 바닥 위로 맞춘다.
	TArray<FVector> Displaced;
	Displaced.SetNum(Line.Num());
	for (int32 i = 0; i < Line.Num(); ++i)
	{
		FVector Moved = Line[i] + Lefts[i] * Blended[i];

		FNavLocation Projected;
		if (NavSys && NavSys->ProjectPointToNavigation(Moved, Projected, ProjectExtent))
		{
			Moved = Projected.Location;
		}
		else
		{
			Moved = Line[i];
		}
		Displaced[i] = Moved;
	}

	// 2-4) 선을 부드럽게 다듬는다. 양 끝은 고정하고, 다듬은 점이 걸을 수 없는 곳으로 나가면 다듬기 전 점을 쓴다.
	for (int32 Pass = 0; Pass < GuideLineSmoothPasses; ++Pass)
	{
		TArray<FVector> Smoothed = Displaced;
		for (int32 i = 1; i < Displaced.Num() - 1; ++i)
		{
			FVector Candidate = Displaced[i - 1] * 0.25f + Displaced[i] * 0.5f + Displaced[i + 1] * 0.25f;

			FNavLocation Projected;
			if (NavSys && NavSys->ProjectPointToNavigation(Candidate, Projected, ProjectExtent))
			{
				Candidate.Z = Projected.Location.Z;
				Smoothed[i] = Candidate;
			}
		}
		Displaced = Smoothed;
	}

	RoutePoints = Displaced;
	MakeCumulative(RoutePoints, RouteCumulative);
	const float TotalLength = RouteCumulative.Last();

	// ---- 3) 완성된 선 위에 발자국을 놓는다.
	//         코너에서 안쪽 발은 짧은 호, 바깥쪽 발은 긴 호를 따라 걸으므로, 안내선을 좌우로 FootprintSideOffset만큼 옮긴 왼발 선/오른발 선을 따로 만들고
	//         각 선 위에서 자기 걸음 간격(2 x FootprintSpacing)마다 찍는다. 플레이어 위치와 무관하게 줄의 모양이 항상 같고, 플레이어 바로 앞(FirstOffset)보다 가까운 곳에는 놓지 않는다.
	TArray<FVector> FootLines[2]; // [0] 왼발, [1] 오른발
	TArray<float> FootCumulative[2];
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Sign = (Side == 0) ? 1.f : -1.f;
		FootLines[Side].SetNum(RoutePoints.Num());
		for (int32 i = 0; i < RoutePoints.Num(); ++i)
		{
			FVector Tangent = RoutePoints[FMath::Min(i + 1, RoutePoints.Num() - 1)] - RoutePoints[FMath::Max(i - 1, 0)];
			Tangent.Z = 0.f;
			Tangent = Tangent.GetSafeNormal();

			// Direction x Up은 진행 방향의 왼쪽이다. (언리얼은 앞 +X, 오른쪽 +Y, 위 +Z라서 X x Z = -Y = 왼쪽)
			const FVector Left = FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
			FootLines[Side][i] = RoutePoints[i] + Left * (Sign * FootprintSideOffset);
		}

		// 안쪽 발 선은 코너에서 좁아져 각질 수 있으므로 양 끝을 고정하고 한 번 더 다듬는다.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			TArray<FVector> Smoothed = FootLines[Side];
			for (int32 i = 1; i < FootLines[Side].Num() - 1; ++i)
			{
				Smoothed[i] = FootLines[Side][i - 1] * 0.25f + FootLines[Side][i] * 0.5f + FootLines[Side][i + 1] * 0.25f;
			}
			FootLines[Side] = Smoothed;
		}
		MakeCumulative(FootLines[Side], FootCumulative[Side]);
	}

	// 선 위에서 점 Location과 가장 가까운 곳의 누적 거리
	auto ArcOfNearest = [](const TArray<FVector>& Points, const TArray<float>& Cumulative, const FVector& Location)
	{
		float BestArc = 0.f;
		float BestDistSq = TNumericLimits<float>::Max();
		for (int32 i = 0; i + 1 < Points.Num(); ++i)
		{
			const FVector Segment = Points[i + 1] - Points[i];
			const float SegmentLengthSq = FMath::Max(Segment.SizeSquared(), KINDA_SMALL_NUMBER);
			const float Alpha = FMath::Clamp(FVector::DotProduct(Location - Points[i], Segment) / SegmentLengthSq, 0.f, 1.f);
			const float DistSq = FVector::DistSquared(Location, Points[i] + Segment * Alpha);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestArc = Cumulative[i] + FMath::Sqrt(SegmentLengthSq) * Alpha;
			}
		}
		return BestArc;
	};

	// 왼발은 왼발 선을 따라 자기 걸음 간격(2 x FootprintSpacing)마다 놓는다. (같은 발끼리의 간격이 코너에서도 일정하다)
	// 오른발은 오른발 선 위에서, 이웃한 왼발 두 개의 한가운데에 가장 가까운 곳에 놓는다. (왼발/오른발이 항상 번갈아 일정하게 걷는다)
	const float FirstOffset = 100.f;
	const float HalfSpacing = FootprintSpacing * 0.5f;
	const float LeftTotal = FootCumulative[0].Last();
	const float RightTotal = FootCumulative[1].Last();

	auto AddFootprint = [&](bool bLeft, const FVector& Location, float FootArc) -> bool
	{
		const TArray<FVector>& Foot = FootLines[bLeft ? 0 : 1];
		const TArray<float>& Cumulative = FootCumulative[bLeft ? 0 : 1];
		const float FootTotal = Cumulative.Last();

		// 이 발자국이 안내선 위 어디쯤인지(지나감 판정용 거리)는 안내선에 수직으로 내려 구한다.
		const float CenterArc = ArcOfNearest(RoutePoints, RouteCumulative, Location);
		if (CenterArc < FirstOffset)
		{
			return false;
		}

		// 코너에서 발끝 방향이 갑자기 꺾이지 않도록, 앞뒤 반 간격 지점을 잇는 방향을 쓴다.
		FVector Before, After, SegmentDirection, Unused;
		PointAt(Foot, Cumulative, FMath::Clamp(FootArc - HalfSpacing, 0.f, FootTotal), Before, SegmentDirection);
		PointAt(Foot, Cumulative, FMath::Clamp(FootArc + HalfSpacing, 0.f, FootTotal), After, Unused);
		FVector Direction = After - Before;
		Direction.Z = 0.f;
		Direction = Direction.GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = SegmentDirection;
		}

		FPlacedFootprint Footprint;
		Footprint.Location = Location;
		Footprint.Yaw = Direction.Rotation().Yaw + FootprintYawOffset;
		Footprint.bLeft = bLeft;
		Footprint.RouteDistance = CenterArc;
		PlacedFootprints.Add(Footprint);
		return true;
	};

	// 청년 쪽 끝의 왼발 위치에서 시작해, 한 걸음(왼발 -> 다음 왼발)마다 오른발 한 번, 왼발 한 번을 놓는다.
	FVector PreviousLeft, Unused;
	PointAt(FootLines[0], FootCumulative[0], LeftTotal, PreviousLeft, Unused);
	for (int32 k = 1; k < 500; ++k)
	{
		const float LeftArc = LeftTotal - k * 2.f * FootprintSpacing;
		if (LeftArc <= 0.f)
		{
			break;
		}

		FVector LeftLocation;
		PointAt(FootLines[0], FootCumulative[0], LeftArc, LeftLocation, Unused);

		// 오른발: 이웃한 왼발 두 개의 한가운데에 가장 가까운 오른발 선 위의 점 (m = 2k - 1)
		const FVector Middle = (PreviousLeft + LeftLocation) * 0.5f;
		const float RightArc = ArcOfNearest(FootLines[1], FootCumulative[1], Middle);
		FVector RightLocation;
		PointAt(FootLines[1], FootCumulative[1], FMath::Clamp(RightArc, 0.f, RightTotal), RightLocation, Unused);
		if (!AddFootprint(false, RightLocation, RightArc))
		{
			break;
		}

		// 왼발 (m = 2k)
		if (!AddFootprint(true, LeftLocation, LeftArc))
		{
			break;
		}

		PreviousLeft = LeftLocation;
	}

	if (bDrawGuideLine)
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOChasingManager] Guide footprints placed: %d (route length %.0f)"), PlacedFootprints.Num(), TotalLength);
	}

	bHasRoute = true;
	OffRouteStartTime = -1.0;
	return true;
}

bool ALCOChasingManager::GetPlayerRouteProgress(const FVector& PlayerLocation, float& OutProgress, float& OutDistanceToRoute) const
{
	OutProgress = 0.f;
	OutDistanceToRoute = TNumericLimits<float>::Max();
	if (RoutePoints.Num() < 2)
	{
		return false;
	}

	for (int32 i = 0; i < RoutePoints.Num() - 1; ++i)
	{
		const FVector Closest = FMath::ClosestPointOnSegment(PlayerLocation, RoutePoints[i], RoutePoints[i + 1]);

		// 플레이어의 중심은 바닥보다 높으므로 수평 거리로 비교한다.
		const float Distance = FVector::Dist2D(PlayerLocation, Closest);
		if (Distance < OutDistanceToRoute)
		{
			OutDistanceToRoute = Distance;
			OutProgress = RouteCumulative[i] + FVector::Dist(RoutePoints[i], Closest);
		}
	}

	return true;
}

bool ALCOChasingManager::MarkPassedFootprints()
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn || !bHasRoute)
	{
		return false;
	}

	float Progress = 0.f;
	float DistanceToRoute = 0.f;
	GetPlayerRouteProgress(PlayerPawn->GetActorLocation(), Progress, DistanceToRoute);

	// 경로에서 크게 벗어나 있으면(다른 길) 지나갔는지 판단하지 않는다. 오래 벗어나 있으면 다시 그린다.
	if (DistanceToRoute > GuideDeviationDistance)
	{
		return false;
	}

	// 플레이어의 경로상 위치보다 FootprintPassDistance 앞까지의 발자국은 지나간 것으로 본다. 한 번 지나가면 다시 보이지 않는다.
	bool bChanged = false;
	for (FPlacedFootprint& Footprint : PlacedFootprints)
	{
		if (!Footprint.bPassed && Footprint.RouteDistance <= Progress + FootprintPassDistance)
		{
			Footprint.bPassed = true;
			bChanged = true;
		}
	}

	return bChanged;
}

void ALCOChasingManager::CheckPassedFootprints()
{
	// 경로 계산 없이 거리만 확인하므로 짧은 주기로 돌려도 부담이 적다. 바뀐 것이 있을 때만 데칼을 갱신한다.
	if (bIsChasing && MarkPassedFootprints())
	{
		RefreshFootprintDecals();
	}
}

void ALCOChasingManager::RefreshFootprintDecals()
{
	// 보일 발자국을 고른다: 지나가지 않은 것 중 플레이어 앞쪽 GuideRange 안의 것.
	TArray<const FPlacedFootprint*> Visible;
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		const FVector PlayerLocation = PlayerPawn->GetActorLocation();

		float Progress = 0.f;
		float DistanceToRoute = 0.f;
		const bool bOnRoute = bHasRoute && GetPlayerRouteProgress(PlayerLocation, Progress, DistanceToRoute) && DistanceToRoute <= GuideDeviationDistance;

		for (const FPlacedFootprint& Footprint : PlacedFootprints)
		{
			if (Footprint.bPassed)
			{
				continue;
			}

			// 경로 위에 있으면 경로상 거리로, 벗어나 있으면 직선 거리로 범위를 본다. (벽 너머의 갈래 길이 보이는 것을 막는다)
			const bool bInRange = bOnRoute
				? (Footprint.RouteDistance - Progress) <= GuideRange
				: FVector::Dist2D(PlayerLocation, Footprint.Location) <= GuideRange;
			if (bInRange)
			{
				Visible.Add(&Footprint);
			}
		}
	}

	// 머티리얼이 없으면 디버그 표시로 대신한다. (에셋이 나오기 전 확인용, 왼발 초록 / 오른발 파랑)
	if (!FootprintMaterialLeft && !FootprintMaterialRight)
	{
#if ENABLE_DRAW_DEBUG
		for (const FPlacedFootprint* Footprint : Visible)
		{
			DrawDebugSphere(GetWorld(), Footprint->Location + FVector(0.f, 0.f, 5.f), 8.f, 8, Footprint->bLeft ? FColor::Green : FColor::Cyan, false, GuideUpdateInterval + 0.05f);
		}
#endif
		return;
	}

	// 데칼을 필요한 만큼 만들어 재사용한다.
	while (FootprintPool.Num() < Visible.Num())
	{
		UDecalComponent* Decal = NewObject<UDecalComponent>(this);
		Decal->DecalSize = FootprintDecalSize;
		Decal->RegisterComponent();
		FootprintPool.Add(Decal);
	}

	for (int32 i = 0; i < FootprintPool.Num(); ++i)
	{
		UDecalComponent* Decal = FootprintPool[i];
		if (!Decal)
		{
			continue;
		}

		if (i < Visible.Num())
		{
			const FPlacedFootprint& Footprint = *Visible[i];

			// 왼발/오른발 머티리얼이 바뀔 때만 다시 지정한다. 한쪽만 있으면 그것을 쓴다.
			UMaterialInterface* Material = Footprint.bLeft ? FootprintMaterialLeft.Get() : FootprintMaterialRight.Get();
			if (!Material)
			{
				Material = FootprintMaterialLeft ? FootprintMaterialLeft.Get() : FootprintMaterialRight.Get();
			}
			if (Decal->GetDecalMaterial() != Material)
			{
				Decal->SetDecalMaterial(Material);
			}

			// 바닥에 투영되도록 아래를 향하게 한다.
			Decal->SetWorldLocationAndRotation(Footprint.Location, FRotator(-90.f, Footprint.Yaw, 0.f));
			Decal->SetVisibility(true);
		}
		else
		{
			Decal->SetVisibility(false);
		}
	}
}

void ALCOChasingManager::DrawGuideLineDebug() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World || !bHasRoute)
	{
		return;
	}

	// 갱신 주기보다 조금 길게 그려서 끊김 없이 보이게 한다.
	const float Duration = GuideUpdateInterval + 0.05f;
	const FVector Lift(0.f, 0.f, 15.f);

	// 빨강: 엔진이 구한 원래 최단 경로(코너 안쪽 모서리를 스친다)
	for (int32 i = 0; i + 1 < DebugRawLine.Num(); ++i)
	{
		DrawDebugLine(World, DebugRawLine[i] + Lift, DebugRawLine[i + 1] + Lift, FColor::Red, false, Duration, 0, 3.f);
	}

	// 초록: 벽 거리를 반영해 만든 최종 안내선. 발자국은 이 선 위에 놓이고 좌우로 FootprintSideOffset만큼 벌어진다.
	for (int32 i = 0; i + 1 < RoutePoints.Num(); ++i)
	{
		DrawDebugLine(World, RoutePoints[i] + Lift, RoutePoints[i + 1] + Lift, FColor::Green, false, Duration, 0, 5.f);
	}

	// 노랑: 왼발, 주황: 오른발, 회색: 지나간 발자국
	for (const FPlacedFootprint& Footprint : PlacedFootprints)
	{
		const FColor Color = Footprint.bPassed ? FColor(90, 90, 90) : (Footprint.bLeft ? FColor::Yellow : FColor::Orange);
		DrawDebugSphere(World, Footprint.Location + Lift, 12.f, 8, Color, false, Duration, 0, 1.f);
	}
#endif
}

void ALCOChasingManager::HidePathGuide()
{
	for (UDecalComponent* Decal : FootprintPool)
	{
		if (Decal)
		{
			Decal->SetVisibility(false);
		}
	}
}

void ALCOChasingManager::ResetPathGuide()
{
	PlacedFootprints.Empty();
	RoutePoints.Reset();
	RouteCumulative.Reset();
	bHasRoute = false;
	DebugRawLine.Reset();
	OffRouteStartTime = -1.0;
	HidePathGuide();
}

#pragma endregion
