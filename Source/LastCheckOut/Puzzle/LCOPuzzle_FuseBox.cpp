#include "Puzzle/LCOPuzzle_FuseBox.h"
#include "Interaction/LCOElevator.h"
#include "Interaction/LCOInteractComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"

ALCOPuzzle_FuseBox::ALCOPuzzle_FuseBox()
{
	MissingFusePrompt = NSLOCTEXT("LCOInteraction", "FuseBoxMissingFusePrompt", "퓨즈가 필요하다");
}

void ALCOPuzzle_FuseBox::BeginPlay()
{
	Super::BeginPlay();

	if (FuseSlotIDs.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_FuseBox] %s has no FuseSlotIDs"), *GetName());
	}

	for (const FName SlotID : FuseSlotIDs)
	{
		if (!FilledSlots.Contains(SlotID))
		{
			SetSlotLight(SlotID, false);
		}

		ULCOInteractComponent* Slot = FindSlot(SlotID);
		if (!Slot)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCOPuzzle_FuseBox] %s: no LCOInteract with InputID %s"), *GetName(), *SlotID.ToString());
			continue;
		}

		// 빈 칸은 고스트 / 숨김 (Super::BeginPlay에서 해결 상태로 복원됐으면 이미 채워져 있음)
		if (bAutoShowFuseMesh && !FilledSlots.Contains(SlotID))
		{
			ShowEmptySlot(SlotID, Slot);
		}
	}
}

bool ALCOPuzzle_FuseBox::IsInteractionLocked_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	if (Super::IsInteractionLocked_Implementation(SourceComponent, Interactor))
	{
		return true;
	}

	return SourceComponent && FilledSlots.Contains(SourceComponent->GetInputID());
}

FText ALCOPuzzle_FuseBox::GetInteractionPrompt_Implementation(ULCOInteractComponent* SourceComponent, AActor* Interactor) const
{
	if (!bIsActive)
	{
		return Super::GetInteractionPrompt_Implementation(SourceComponent, Interactor);
	}

	const FName InputID = SourceComponent ? SourceComponent->GetInputID() : NAME_None;
	if (FuseSlotIDs.Contains(InputID) && !FilledSlots.Contains(InputID) && !HasFuse(Interactor))
	{
		return MissingFusePrompt;
	}

	// 그 외에는 칸 컴포넌트의 기본 문구 (예: "퓨즈 끼우기")
	return FText::GetEmpty();
}

FLCOActorSaveState ALCOPuzzle_FuseBox::SaveActorState_Implementation() const
{
	FLCOActorSaveState State = Super::SaveActorState_Implementation();

	for (const FName SlotID : FilledSlots)
	{
		FLCOSavedItem& Saved = State.Items.AddDefaulted_GetRef();
		Saved.ItemID = SlotID;
		Saved.Quantity = 1;
	}

	return State;
}

void ALCOPuzzle_FuseBox::LoadActorState_Implementation(const FLCOActorSaveState& State)
{
	Super::LoadActorState_Implementation(State);

	// 해결된 퍼즐은 HandleRestoredAsSolved에서 모든 칸을 채움
	if (bIsSolved)
	{
		return;
	}

	for (const FLCOSavedItem& Saved : State.Items)
	{
		if (FuseSlotIDs.Contains(Saved.ItemID) && !FilledSlots.Contains(Saved.ItemID))
		{
			FillSlot(Saved.ItemID, true);
		}
	}
}

void ALCOPuzzle_FuseBox::HandlePuzzleInput_Implementation(ULCOInteractComponent* SourceComponent, FName InputID, AActor* Interactor)
{
	// 퓨즈 칸이 아닌 부품(문 등)은 OnPuzzleInteracted에서 BP가 처리
	if (!FuseSlotIDs.Contains(InputID) || FilledSlots.Contains(InputID))
	{
		return;
	}

	UInventoryComponent* Inventory = IsValid(Interactor) ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	if (!Inventory || !Inventory->HasItem(FuseItemID))
	{
		UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_FuseBox] %s: no %s for %s"), *GetName(), *FuseItemID.ToString(), *InputID.ToString());
		OnFuseMissing(InputID, SourceComponent);
		return;
	}

	if (!Inventory->RemoveItem(FuseItemID, 1))
	{
		return;
	}

	FillSlot(InputID, false);

	UE_LOG(LogTemp, Log, TEXT("[LCOPuzzle_FuseBox] %s Fuse inserted: %s (%d / %d)"), *GetName(), *InputID.ToString(), FilledSlots.Num(), FuseSlotIDs.Num());

	if (FilledSlots.Num() >= FuseSlotIDs.Num())
	{
		SolvePuzzle();
		PowerElevators();
	}
}

void ALCOPuzzle_FuseBox::HandleRestoredAsSolved()
{
	for (const FName SlotID : FuseSlotIDs)
	{
		if (!FilledSlots.Contains(SlotID))
		{
			FillSlot(SlotID, true);
		}
	}
}

ULCOInteractComponent* ALCOPuzzle_FuseBox::FindSlot(FName SlotID) const
{
	TInlineComponentArray<ULCOInteractComponent*> InteractComponents(this);
	for (ULCOInteractComponent* Component : InteractComponents)
	{
		if (Component->GetInputID() == SlotID)
		{
			return Component;
		}
	}
	return nullptr;
}

bool ALCOPuzzle_FuseBox::HasFuse(const AActor* Interactor) const
{
	const UInventoryComponent* Inventory = IsValid(Interactor) ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	return Inventory && Inventory->HasItem(FuseItemID);
}

void ALCOPuzzle_FuseBox::FillSlot(FName SlotID, bool bInstant)
{
	FilledSlots.Add(SlotID);

	ULCOInteractComponent* Slot = FindSlot(SlotID);
	if (Slot && bAutoShowFuseMesh)
	{
		ShowFilledSlot(SlotID, Slot);
	}

	SetSlotLight(SlotID, true);

	OnFuseInserted(SlotID, Slot, bInstant);
}

void ALCOPuzzle_FuseBox::ShowEmptySlot(FName SlotID, ULCOInteractComponent* Slot)
{
	if (!GhostMaterial)
	{
		Slot->SetVisibility(false);
		return;
	}

	if (GhostedSlots.Contains(SlotID))
	{
		return;
	}

	FLCOFuseSlotOriginalLook& Original = GhostedSlots.Add(SlotID);
	Original.bCastShadow = Slot->CastShadow;

	for (int32 Index = 0; Index < Slot->GetNumMaterials(); ++Index)
	{
		Original.Materials.Add(Slot->GetMaterial(Index));
		Slot->SetMaterial(Index, GhostMaterial);
	}

	Slot->SetCastShadow(false);
	Slot->SetVisibility(true);
}

void ALCOPuzzle_FuseBox::ShowFilledSlot(FName SlotID, ULCOInteractComponent* Slot)
{
	FLCOFuseSlotOriginalLook Original;
	if (GhostedSlots.RemoveAndCopyValue(SlotID, Original))
	{
		for (int32 Index = 0; Index < Original.Materials.Num(); ++Index)
		{
			Slot->SetMaterial(Index, Original.Materials[Index]);
		}

		Slot->SetCastShadow(Original.bCastShadow);
	}

	Slot->SetVisibility(true);
}

void ALCOPuzzle_FuseBox::SetSlotLight(FName SlotID, bool bOn)
{
	if (!LightOffMaterial || !LightOnMaterial)
	{
		return;
	}

	UMaterialInterface* const Target = bOn ? LightOnMaterial : LightOffMaterial;

	TArray<UActorComponent*> Tagged = GetComponentsByTag(UMeshComponent::StaticClass(), SlotID);
	for (UActorComponent* Component : Tagged)
	{
		UMeshComponent* Mesh = Cast<UMeshComponent>(Component);
		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			const UMaterialInterface* Current = Mesh->GetMaterial(Index);
			if (Current == LightOffMaterial || Current == LightOnMaterial)
			{
				Mesh->SetMaterial(Index, Target);
			}
		}
	}
}

void ALCOPuzzle_FuseBox::PowerElevators()
{
	for (ALCOElevator* Elevator : PoweredElevators)
	{
		if (IsValid(Elevator))
		{
			Elevator->SetPowered(true);
		}
	}
}
