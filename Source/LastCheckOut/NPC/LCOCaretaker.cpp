#include "NPC/LCOCaretaker.h"
#include "AI/LCOCaretakerAIController.h"
#include "Engine/TargetPoint.h"

ALCOCaretaker::ALCOCaretaker()
{
	AIControllerClass = ALCOCaretakerAIController::StaticClass();

	NPCId = TEXT("Caretaker");

	// 임시 값. 플레이어 달리기 속도 확정 후 조정
	WalkSpeed = 150.f;
	RunSpeed = 500.f;
}

bool ALCOCaretaker::GetNextPatrolLocation(FVector& OutLocation)
{
	if (PatrolPoints.IsEmpty())
	{
		return false;
	}

	PatrolIndex %= PatrolPoints.Num();

	const ATargetPoint* Point = PatrolPoints[PatrolIndex];
	PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();

	if (!Point)
	{
		return false;
	}

	OutLocation = Point->GetActorLocation();
	return true;
}

void ALCOCaretaker::BeginPlay()
{
	Super::BeginPlay();

	OnConversationStarted.AddDynamic(this, &ALCOCaretaker::HandleConversationStarted);
	OnConversationEnded.AddDynamic(this, &ALCOCaretaker::HandleConversationEnded);
}

void ALCOCaretaker::HandleConversationStarted(AActor* Partner)
{
	// 대화가 끝나고 숙임을 다시 펴는 동안에도 Tick이 필요하므로 켠다.
	SetActorTickEnabled(true);
}

void ALCOCaretaker::HandleConversationEnded()
{
	SetActorTickEnabled(true);
}

void ALCOCaretaker::EnterDoorBendZone()
{
	++DoorBendZoneCount;

	// 대화가 없으면 Tick이 꺼져 있을 수 있으므로 켠다.
	SetActorTickEnabled(true);
}

void ALCOCaretaker::ExitDoorBendZone()
{
	DoorBendZoneCount = FMath::Max(DoorBendZoneCount - 1, 0);
}

void ALCOCaretaker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 구간 안이면 숙이고, 나오면 편다. 숙인 정도만큼 느려진다.
	const float Target = DoorBendZoneCount > 0 ? 1.f : 0.f;
	DoorBendAlpha = FMath::FInterpConstantTo(DoorBendAlpha, Target, DeltaSeconds, DoorBendBlendSpeed);
	SetMoveSpeedScale(FMath::Lerp(1.f, DoorBendSpeedScale, DoorBendAlpha));

	// 대화 중이면 숙이고, 끝나면 편다.
	ConversationBendAlpha = FMath::FInterpConstantTo(ConversationBendAlpha, IsInConversation() ? 1.f : 0.f, DeltaSeconds, ConversationBendBlendSpeed);

	// 베이스 Tick이 대화/디버그가 없으면 Tick을 꺼 버리므로, 숙이는 동안과 펴는 동안은 다시 켠다.
	if (DoorBendZoneCount > 0 || DoorBendAlpha > 0.f || ConversationBendAlpha > 0.f)
	{
		SetActorTickEnabled(true);
	}
}
