#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NotebookComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LASTCHECKOUT_API UNotebookComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNotebookComponent();

protected:
	virtual void BeginPlay() override;

public:
	// 플레이어가 발견한 증거 ID 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notebook")
	TArray<FName> CollectedEvidence;

	// 새로운 증거를 수첩에 등록
	UFUNCTION(BlueprintCallable, Category = "Notebook")
	void AddEvidence(FName EvidenceID);

	// 특정 증거를 이미 발견했는지 확인
	UFUNCTION(BlueprintPure, Category = "Notebook")
	bool HasEvidence(FName EvidenceID) const;
};