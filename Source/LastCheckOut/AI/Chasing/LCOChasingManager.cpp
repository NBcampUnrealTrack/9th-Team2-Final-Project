#include "AI/Chasing/LCOChasingManager.h"

#include "AI/LCOCaretakerAIController.h"
#include "AI/SuspicionComponent.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "NPC/LCOCaretaker.h"
#include "NPC/LCOSigolYouth.h"
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

void ALCOChasingManager::StartChasing()
{
	if (bIsChasing)
	{
		return;
	}

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

	if (ALCOSigolYouth* Youth = SpawnedYouth.Get())
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

		// 거리가 멀면 보이지 않는다.
		if (FVector::Dist(CaretakerPawn->GetActorLocation(), PlayerPawn->GetActorLocation()) > HideSpotDistance)
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
