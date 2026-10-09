#include "LCOGameInstance.h"
#include "LastCheckOut.h"
#include "LastCheckOut/Story/LCOStorySubsystem.h"
#include "Notebook/LCONotebookSubsystem.h"

void ULCOGameInstance::Init()
{
	Super::Init();
	// Log Test 
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameInstance] Init"));
}

void ULCOGameInstance::SetStartMode(ELCOGameStartMode NewMode)
{
	StartMode = NewMode;
	
	// Log Test 
	// UEnum::GetValueAsString: enum 값을 "ELCOGameStartMode::NewGame" 같은 문자열로 변환
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameInstance] SetStartMode: %s"), *UEnum::GetValueAsString(StartMode));
}

void ULCOGameInstance::ResetStartMode()
{
	StartMode = ELCOGameStartMode::None;
	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameInstance] ResetStartMode"));
}

ULCOStorySubsystem* ULCOGameInstance::GetStorySubsystem() const
{
	return GetSubsystem<ULCOStorySubsystem>();
}

ULCONotebookSubsystem* ULCOGameInstance::GetNotebookSubsystem() const
{
	return GetSubsystem<ULCONotebookSubsystem>();
}

void ULCOGameInstance::StoreCarryData(const FLCOPlayerCarryData& Data)
{
	CarryData = Data;
	CarryData.bHasData = true;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameInstance] StoreCarryData: Items = %d"), CarryData.InventorySlots.Num());
}

bool ULCOGameInstance::ConsumeCarryData(FLCOPlayerCarryData& OutData)
{
	if (!CarryData.bHasData)
	{
		return false;
	}

	OutData = CarryData;

	// 꺼낸 즉시 비움 (원본은 다시 캐릭터의 컴포넌트가 가짐 → 중복 보관 방지)
	CarryData = FLCOPlayerCarryData();
	return true;
}

void ULCOGameInstance::ClearCarryData()
{
	CarryData = FLCOPlayerCarryData();

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameInstance] ClearCarryData"));
}
