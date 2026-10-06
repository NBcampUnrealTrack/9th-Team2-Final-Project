#include "NPC/LCOCaretaker.h"
#include "AI/LCOCaretakerAIController.h"

ALCOCaretaker::ALCOCaretaker()
{
	AIControllerClass = ALCOCaretakerAIController::StaticClass();

	NPCId = TEXT("Caretaker");

	// 임시 값. 플레이어 달리기 속도 확정 후 조정
	WalkSpeed = 150.f;
	RunSpeed = 500.f;
}
