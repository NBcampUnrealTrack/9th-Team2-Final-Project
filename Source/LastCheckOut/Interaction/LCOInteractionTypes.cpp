#include "Interaction/LCOInteractionTypes.h"
#include "Story/LCOStorySubsystem.h"
#include "Engine/GameInstance.h"

void FLCOStoryResult::Apply(const UGameInstance* GameInstance) const
{
	ULCOStorySubsystem* Story = GameInstance ? GameInstance->GetSubsystem<ULCOStorySubsystem>() : nullptr;
	if (!Story || IsEmpty())
	{
		return;
	}

	if (!ObjectiveToComplete.IsNone())
	{
		Story->CompleteObjective(ObjectiveToComplete);
	}

	for (const ELCOStoryFlag Flag : FlagsToUnlock)
	{
		Story->TryUnlock(Flag);
	}
}
