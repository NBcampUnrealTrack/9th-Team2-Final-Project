#include "LCOGameInstance.h"
#include "LastCheckOut.h"

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