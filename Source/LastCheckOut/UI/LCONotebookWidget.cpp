#include "UI/LCONotebookWidget.h"

#include "UI/LCONotebookListItemWidget.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Player/LCOPlayerController.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

namespace LCONotebookPage
{
	constexpr int32 EntryPage = 0;
	constexpr int32 MapPage = 1;
}

// ======================================================================
// 초기화
// ======================================================================

void ULCONotebookWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Btn_Tab_Objective)
	{
		Btn_Tab_Objective->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabObjectiveClicked);
	}
	if (Btn_Tab_Clue)
	{
		Btn_Tab_Clue->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabClueClicked);
	}
	if (Btn_Tab_Testimony)
	{
		Btn_Tab_Testimony->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabTestimonyClicked);
	}
	if (Btn_Tab_Photo)
	{
		Btn_Tab_Photo->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabPhotoClicked);
	}
	if (Btn_Tab_Map)
	{
		Btn_Tab_Map->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabMapClicked);
	}
	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	// J 키 → 수첩 토글
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnNotebookRequested.AddUniqueDynamic(this, &ThisClass::HandleNotebookRequested);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebook] Owning Player가 LCOPlayerController가 아님 - J 키 바인딩 안 됨"));
	}

	// 수첩 내용 변경 감지
	if (ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
	{
		Notebook->OnNotebookChanged.AddUniqueDynamic(this, &ThisClass::HandleNotebookChanged);
	}
}

void ULCONotebookWidget::NativeDestruct()
{
	if (ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
	{
		Notebook->OnNotebookChanged.RemoveDynamic(this, &ThisClass::HandleNotebookChanged);
	}

	Super::NativeDestruct();
}

void ULCONotebookWidget::UnbindFromNotebookInput()
{
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnNotebookRequested.RemoveDynamic(this, &ThisClass::HandleNotebookRequested);
	}
}

ULCONotebookSubsystem* ULCONotebookWidget::GetNotebookSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<ULCONotebookSubsystem>() : nullptr;
}

// ======================================================================
// 열기 / 닫기
// ======================================================================

void ULCONotebookWidget::OpenNotebook()
{
	if (IsInViewport())
	{
		return;
	}

	// 다른 메뉴(일시정지, 인벤토리 등)가 열려 있으면 열지 않음
	if (UGameplayStatics::IsGamePaused(this))
	{
		return;
	}

	if (const ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		if (LCOPC->IsInUIMode())
		{
			return;
		}
	}

	AddDebugEntriesOnce();

	AddToViewport(ViewportZOrder);
	if (bPauseGameWhileOpen)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
	SetPlayerUIMode(true);

	// 마지막으로 보던 탭으로 열기
	SelectTab(CurrentTab);
	OnNotebookOpened();
}

void ULCONotebookWidget::CloseNotebook()
{
	if (!IsInViewport())
	{
		return;
	}

	RemoveFromParent();
	if (bPauseGameWhileOpen)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}
	SetPlayerUIMode(false);
	OnNotebookClosed();
}

void ULCONotebookWidget::ToggleNotebook()
{
	if (IsInViewport())
	{
		CloseNotebook();
	}
	else
	{
		OpenNotebook();
	}
}

void ULCONotebookWidget::HandleNotebookRequested()
{
	ToggleNotebook();
}

void ULCONotebookWidget::HandleCloseClicked()
{
	CloseNotebook();
}

void ULCONotebookWidget::SetPlayerUIMode(bool bEnable)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(PC))
	{
		LCOPC->SetUIMode(bEnable);
		return;
	}

	if (bEnable)
	{
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC, this, EMouseLockMode::DoNotLock, false);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
		PC->SetShowMouseCursor(false);
	}
}

void ULCONotebookWidget::AddDebugEntriesOnce()
{
	if (!bAddDebugEntries || bDebugEntriesAdded)
	{
		return;
	}

	bDebugEntriesAdded = true;

	if (ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
	{
		for (const FLCONotebookEntry& Entry : DebugEntries)
		{
			Notebook->AddEntry(Entry);
		}
	}
}

// ======================================================================
// 탭
// ======================================================================

void ULCONotebookWidget::HandleTabObjectiveClicked() { SelectTab(ELCONotebookTab::Objective); }
void ULCONotebookWidget::HandleTabClueClicked()      { SelectTab(ELCONotebookTab::Clue); }
void ULCONotebookWidget::HandleTabTestimonyClicked() { SelectTab(ELCONotebookTab::Testimony); }
void ULCONotebookWidget::HandleTabPhotoClicked()     { SelectTab(ELCONotebookTab::Photo); }
void ULCONotebookWidget::HandleTabMapClicked()       { SelectTab(ELCONotebookTab::Map); }

void ULCONotebookWidget::SelectTab(ELCONotebookTab NewTab)
{
	CurrentTab = NewTab;

	if (CurrentTab == ELCONotebookTab::Map)
	{
		if (Switcher_Pages)
		{
			Switcher_Pages->SetActiveWidgetIndex(LCONotebookPage::MapPage);
		}
		RebuildMapAreaList();
	}
	else
	{
		if (Switcher_Pages)
		{
			Switcher_Pages->SetActiveWidgetIndex(LCONotebookPage::EntryPage);
		}
		SelectedEntryIndex = INDEX_NONE;
		RebuildEntryList();
	}

	UpdatePageTitle();
	OnTabChanged(CurrentTab);
}

void ULCONotebookWidget::UpdatePageTitle()
{
	if (!Txt_PageTitle)
	{
		return;
	}

	const FText* Title = TabTitles.Find(CurrentTab);
	Txt_PageTitle->SetText(Title ? *Title : FText::GetEmpty());
}

ELCONotebookCategory ULCONotebookWidget::TabToCategory(ELCONotebookTab Tab)
{
	switch (Tab)
	{
	case ELCONotebookTab::Objective: return ELCONotebookCategory::Objective;
	case ELCONotebookTab::Clue:      return ELCONotebookCategory::Clue;
	case ELCONotebookTab::Testimony: return ELCONotebookCategory::Testimony;
	case ELCONotebookTab::Photo:     return ELCONotebookCategory::Photo;
	default:                         return ELCONotebookCategory::Clue;
	}
}

// ======================================================================
// 목록 / 상세 (목표, 단서, 증언, 사진)
// ======================================================================

void ULCONotebookWidget::RebuildEntryList()
{
	if (!Panel_EntryList)
	{
		return;
	}

	// 갱신 전 선택 항목 기억 (내용이 바뀌어도 같은 항목 유지)
	const FName PrevSelectedID = CurrentEntries.IsValidIndex(SelectedEntryIndex)
		? CurrentEntries[SelectedEntryIndex].EntryID
		: NAME_None;

	Panel_EntryList->ClearChildren();
	EntryItemWidgets.Reset();
	CurrentEntries.Reset();
	SelectedEntryIndex = INDEX_NONE;

	if (const ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
	{
		CurrentEntries = Notebook->GetEntriesByCategory(TabToCategory(CurrentTab));
	}

	if (Txt_EmptyList)
	{
		Txt_EmptyList->SetVisibility(CurrentEntries.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (CurrentEntries.Num() == 0)
	{
		ClearDetail();
		return;
	}

	if (!ListItemWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebook] ListItemWidgetClass가 비어 있음"));
		return;
	}

	int32 IndexToSelect = 0;
	for (int32 i = 0; i < CurrentEntries.Num(); ++i)
	{
		ULCONotebookListItemWidget* Item = CreateWidget<ULCONotebookListItemWidget>(GetOwningPlayer(), ListItemWidgetClass);
		if (!Item)
		{
			continue;
		}

		Item->SetItemData(i, CurrentEntries[i].Title, CurrentEntries[i].bIsNew);
		Item->OnItemClicked.AddUniqueDynamic(this, &ThisClass::HandleEntryClicked);
		Panel_EntryList->AddChild(Item);
		EntryItemWidgets.Add(Item);

		if (CurrentEntries[i].EntryID == PrevSelectedID)
		{
			IndexToSelect = i;
		}
	}

	SelectEntry(IndexToSelect);
}

void ULCONotebookWidget::HandleEntryClicked(int32 ItemIndex)
{
	SelectEntry(ItemIndex);
}

void ULCONotebookWidget::SelectEntry(int32 ItemIndex)
{
	if (!CurrentEntries.IsValidIndex(ItemIndex))
	{
		ClearDetail();
		return;
	}

	SelectedEntryIndex = ItemIndex;

	for (int32 i = 0; i < EntryItemWidgets.Num(); ++i)
	{
		if (EntryItemWidgets[i])
		{
			EntryItemWidgets[i]->SetSelected(i == SelectedEntryIndex);
		}
	}

	FLCONotebookEntry& Entry = CurrentEntries[ItemIndex];

	if (Txt_DetailTitle)
	{
		Txt_DetailTitle->SetText(Entry.Title);
	}

	if (Txt_DetailDescription)
	{
		Txt_DetailDescription->SetText(Entry.Description);
	}

	if (Img_DetailImage)
	{
		if (Entry.Image)
		{
			Img_DetailImage->SetBrushFromTexture(Entry.Image);
			Img_DetailImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Img_DetailImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 읽음 처리 (갱신 이벤트로 목록이 다시 그려지지 않도록 막고 처리)
	if (Entry.bIsNew)
	{
		Entry.bIsNew = false;

		if (EntryItemWidgets.IsValidIndex(ItemIndex) && EntryItemWidgets[ItemIndex])
		{
			EntryItemWidgets[ItemIndex]->SetIsNew(false);
		}

		if (ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
		{
			TGuardValue<bool> Guard(bIsRefreshing, true);
			Notebook->MarkEntryAsRead(Entry.EntryID);
		}
	}
}

void ULCONotebookWidget::ClearDetail()
{
	SelectedEntryIndex = INDEX_NONE;

	if (Txt_DetailTitle)
	{
		Txt_DetailTitle->SetText(FText::GetEmpty());
	}

	if (Txt_DetailDescription)
	{
		Txt_DetailDescription->SetText(FText::GetEmpty());
	}

	if (Img_DetailImage)
	{
		Img_DetailImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ULCONotebookWidget::HandleNotebookChanged()
{
	if (bIsRefreshing || !IsInViewport() || CurrentTab == ELCONotebookTab::Map)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsRefreshing, true);
	RebuildEntryList();
}

// ======================================================================
// 지도
// ======================================================================

void ULCONotebookWidget::RebuildMapAreaList()
{
	if (!Panel_MapAreaList)
	{
		return;
	}

	Panel_MapAreaList->ClearChildren();
	MapAreaItemWidgets.Reset();

	if (!MapData || MapData->Areas.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebook] MapData가 비어 있음 - WBP 클래스 디폴트에서 DA_NotebookMap 지정 필요"));
		if (Img_Map)
		{
			Img_Map->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	if (ListItemWidgetClass)
	{
		for (int32 i = 0; i < MapData->Areas.Num(); ++i)
		{
			ULCONotebookListItemWidget* Item = CreateWidget<ULCONotebookListItemWidget>(GetOwningPlayer(), ListItemWidgetClass);
			if (!Item)
			{
				continue;
			}

			Item->SetItemData(i, MapData->Areas[i].AreaName, false);
			Item->OnItemClicked.AddUniqueDynamic(this, &ThisClass::HandleMapAreaClicked);
			Panel_MapAreaList->AddChild(Item);
			MapAreaItemWidgets.Add(Item);
		}
	}

	// 마지막으로 보던 구역 유지
	SelectMapArea(MapData->Areas.IsValidIndex(SelectedMapAreaIndex) ? SelectedMapAreaIndex : 0);
}

void ULCONotebookWidget::HandleMapAreaClicked(int32 ItemIndex)
{
	SelectMapArea(ItemIndex);
}

void ULCONotebookWidget::SelectMapArea(int32 ItemIndex)
{
	if (!MapData || !MapData->Areas.IsValidIndex(ItemIndex))
	{
		return;
	}

	SelectedMapAreaIndex = ItemIndex;

	for (int32 i = 0; i < MapAreaItemWidgets.Num(); ++i)
	{
		if (MapAreaItemWidgets[i])
		{
			MapAreaItemWidgets[i]->SetSelected(i == SelectedMapAreaIndex);
		}
	}

	const FLCONotebookMapArea& Area = MapData->Areas[ItemIndex];

	if (Img_Map)
	{
		if (Area.MapImage)
		{
			Img_Map->SetBrushFromTexture(Area.MapImage);
			Img_Map->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Img_Map->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (Txt_MapAreaName)
	{
		Txt_MapAreaName->SetText(Area.AreaName);
	}
}
