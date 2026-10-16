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
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

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

	return PickFarthest(SpawnCandidates, MinSpawnPathLength) || PickFarthest(FallbackPoints, 0.f);
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

	// 1) 플레이어가 지나간(가까이 간) 발자국은 지운다. 끝 위치(GuideTip)는 유지된다.
	const float PassDistance = 80.f;
	PlacedFootprints.RemoveAll([&PlayerLocation, PassDistance](const FPlacedFootprint& Footprint)
	{
		return FVector::Dist2D(Footprint.Location, PlayerLocation) < PassDistance;
	});

	// 2) 발자국이 끝나는 위치에서 이어서 그린다. 플레이어가 끝에서 GuideRange보다 멀면 새로 그리지 않는다.
	const bool bCanExtend = !bGuideReachedYouth && (!bGuideStarted || FVector::Dist2D(PlayerLocation, GuideTip) <= GuideRange);
	if (bCanExtend)
	{
		// 시작 전에는 플레이어 위치에서, 시작한 뒤에는 끝 위치에서 청년까지의 NavMesh 최단 경로를 구한다.
		const FVector StartLocation = bGuideStarted ? GuideTip : PlayerLocation;
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), StartLocation, Youth->GetActorLocation());
		if (Path && Path->IsValid() && Path->PathPoints.Num() >= 2)
		{
			const TArray<FVector>& Points = Path->PathPoints;
			TArray<float> Cumulative;
			Cumulative.Add(0.f);
			for (int32 i = 1; i < Points.Num(); ++i)
			{
				Cumulative.Add(Cumulative.Last() + FVector::Dist(Points[i - 1], Points[i]));
			}
			const float TotalLength = Cumulative.Last();

			// 경로상 거리 D에 해당하는 위치와 진행 방향
			auto GetPointAtDistance = [&Points, &Cumulative](float Distance, FVector& OutLocation, FVector& OutDirection)
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

			// 처음에는 플레이어 바로 앞(MinAheadDistance)부터, 이후에는 끝에서 간격 만큼 떨어진 곳부터 찍는다.
			const float MinAheadDistance = 100.f;
			float Distance = bGuideStarted ? FootprintSpacing : MinAheadDistance;

			for (int32 Guard = 0; Guard < 100; ++Guard)
			{
				// 청년 근처에 도달하면 그만 그린다.
				if (Distance > TotalLength - FootprintSpacing * 0.5f)
				{
					bGuideReachedYouth = true;
					break;
				}

				FVector Location, Direction;
				GetPointAtDistance(Distance, Location, Direction);

				// 플레이어에게서 GuideRange보다 멀어지면 여기서 멈춘다. (플레이어가 다가오면 이어서 그린다)
				if (FVector::Dist2D(PlayerLocation, Location) > GuideRange)
				{
					break;
				}

				// Direction x Up은 진행 방향의 왼쪽이다. (언리얼은 앞 +X, 오른쪽 +Y, 위 +Z라서 X x Z = -Y = 왼쪽)
				const FVector Left = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();

				FPlacedFootprint Footprint;
				Footprint.Location = Location + Left * (bGuideNextLeft ? FootprintSideOffset : -FootprintSideOffset);
				Footprint.Yaw = Direction.Rotation().Yaw + FootprintYawOffset;
				Footprint.bLeft = bGuideNextLeft;
				PlacedFootprints.Add(Footprint);

				GuideTip = Location;
				bGuideStarted = true;
				bGuideNextLeft = !bGuideNextLeft;
				Distance += FootprintSpacing;
			}
		}
	}

	// 머티리얼이 없으면 디버그 표시로 대신한다. (에셋이 나오기 전 확인용, 왼발 초록 / 오른발 파랑)
	if (!FootprintMaterialLeft && !FootprintMaterialRight)
	{
#if ENABLE_DRAW_DEBUG
		for (const FPlacedFootprint& Footprint : PlacedFootprints)
		{
			DrawDebugSphere(GetWorld(), Footprint.Location + FVector(0.f, 0.f, 5.f), 8.f, 8, Footprint.bLeft ? FColor::Green : FColor::Cyan, false, GuideUpdateInterval + 0.05f);
		}
#endif
		return;
	}

	// 데칼을 필요한 만큼 만들어 재사용한다.
	while (FootprintPool.Num() < PlacedFootprints.Num())
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

		if (i < PlacedFootprints.Num())
		{
			const FPlacedFootprint& Footprint = PlacedFootprints[i];

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
	GuideTip = FVector::ZeroVector;
	bGuideStarted = false;
	bGuideNextLeft = true;
	bGuideReachedYouth = false;
	HidePathGuide();
}

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

	// 퇴장 지점 중 플레이어에게서 가장 멀고 청년이 도달할 수 있는 곳을 고른다.
	ATargetPoint* BestPoint = nullptr;
	double BestDistance = -1.0;
	for (ATargetPoint* Point : LeavePoints)
	{
		if (!Point)
		{
			continue;
		}

		double PathLength = 0.0;
		if (UNavigationSystemV1::GetPathLength(GetWorld(), Youth->GetActorLocation(), Point->GetActorLocation(), PathLength) != ENavigationQueryResult::Success)
		{
			continue;
		}

		const double Distance = FVector::Dist(Point->GetActorLocation(), PlayerPawn->GetActorLocation());
		if (Distance > BestDistance)
		{
			BestDistance = Distance;
			BestPoint = Point;
		}
	}

	YouthLeaveTarget = BestPoint;
	if (BestPoint)
	{
		if (AAIController* AI = Cast<AAIController>(Youth->GetController()))
		{
			AI->MoveToLocation(BestPoint->GetActorLocation(), 100.f);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOChasingManager] No valid LeavePoint. The youth disappears after the timeout."));
	}

	// 퇴장 지점 도착 / 시야 밖 / 시간 초과 중 먼저 만족하는 조건으로 제거한다.
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

	// 2) 퇴장 지점 도착
	if (const ATargetPoint* Target = YouthLeaveTarget.Get())
	{
		if (FVector::Dist(Youth->GetActorLocation(), Target->GetActorLocation()) <= 150.f)
		{
			CleanupYouth();
			return;
		}
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
	YouthLeaveTarget.Reset();

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
