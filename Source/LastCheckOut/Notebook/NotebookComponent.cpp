#include "NotebookComponent.h"

UNotebookComponent::UNotebookComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNotebookComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UNotebookComponent::AddEvidence(FName EvidenceID)
{
	// 유효하지 않은 증거 ID라면 추가하지 않음
	if (EvidenceID.IsNone())
	{
		return;
	}

	// 이미 등록된 증거라면 중복 추가하지 않음
	if (HasEvidence(EvidenceID))
	{
		return;
	}

	// 새로운 증거 등록
	CollectedEvidence.Add(EvidenceID);
}

bool UNotebookComponent::HasEvidence(FName EvidenceID) const
{
	return CollectedEvidence.Contains(EvidenceID);
}
