#include "UI/LCONotebookWidget.h"

#include "UI/LCONotebookListItemWidget.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "Story/LCOStorySubsystem.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Player/LCOPlayerController.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "UI/LCOContainerWidget.h"
#include "UI/LCOMonologueSubsystem.h"

namespace LCONotebookWidgetPrivate
{
	// 화면에 열려 있는 상자(서랍장) 창을 모두 닫음 (상자 위에 인벤토리 / 수첩을 겹쳐 열지 않도록)
	void CloseOpenContainerWidgets(UObject* WorldContextObject)
	{
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContextObject, FoundWidgets, ULCOContainerWidget::StaticClass(), true);

		for (UUserWidget* Widget : FoundWidgets)
		{
			ULCOContainerWidget* ContainerWidget = Cast<ULCOContainerWidget>(Widget);
			if (ContainerWidget && ContainerWidget->IsInViewport())
			{
				ContainerWidget->CloseContainer();
			}
		}
	}
}

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

	// 추리 탭 (선택)
	if (Btn_Tab_Deduction)
	{
		Btn_Tab_Deduction->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTabDeductionClicked);
	}
	if (Btn_DeductionSlot1)
	{
		Btn_DeductionSlot1->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeductionSlot1Clicked);
	}
	if (Btn_DeductionSlot2)
	{
		Btn_DeductionSlot2->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeductionSlot2Clicked);
	}
	if (Btn_DeductionSlot3)
	{
		Btn_DeductionSlot3->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeductionSlot3Clicked);
	}
	if (Btn_DeductionCombine)
	{
		Btn_DeductionCombine->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeductionCombineClicked);
	}
	if (Btn_DeductionClear)
	{
		Btn_DeductionClear->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDeductionClearClicked);
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

	// 상자 창이 열려 있으면 먼저 닫음 (닫은 뒤 일시정지 + UI 모드 적용)
	LCONotebookWidgetPrivate::CloseOpenContainerWidgets(this);

	AddDebugEntriesOnce();

	AddToViewport(ViewportZOrder);
	if (bPauseGameWhileOpen)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
	SetPlayerUIMode(true);

	// 마지막으로 보던 탭으로 열기 (힌트 독백은 추리 탭에서만 - SelectTab 참고)
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
void ULCONotebookWidget::HandleTabDeductionClicked() { SelectTab(ELCONotebookTab::Deduction); }

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
	else if (CurrentTab == ELCONotebookTab::Deduction)
	{
		if (Switcher_Pages)
		{
			Switcher_Pages->SetActiveWidgetIndex(DeductionPageIndex);
		}
		RebuildDeductionCandidates();
		RefreshDeductionSlots();
		ShowDeductionHintOrClear();
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
	case ELCONotebookTab::Deduction: return ELCONotebookCategory::Deduction;
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

	const ELCONotebookCategory TabCategory = TabToCategory(CurrentTab);
	const bool bUseGroups = GroupedCategories.Contains(TabCategory);

	if (const ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
	{
		CurrentEntries = Notebook->GetEntriesByCategory(TabCategory);
	}

	// 묶음 순서대로 정렬 (목록 인덱스 = CurrentEntries 인덱스 유지)
	if (bUseGroups)
	{
		SortEntriesByGroup(CurrentEntries);
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
		// 묶음이 바뀌는 곳에 머리말 (GroupID 없는 항목은 머리말 없음)
		if (bUseGroups)
		{
			const FName GroupID = CurrentEntries[i].GroupID;
			const bool bNewGroup = (i == 0) || (CurrentEntries[i - 1].GroupID != GroupID);
			if (bNewGroup && !GroupID.IsNone())
			{
				AddListHeader(Panel_EntryList, GetEntryGroupHeaderText(GroupID));
			}
		}

		ULCONotebookListItemWidget* Item = CreateWidget<ULCONotebookListItemWidget>(GetOwningPlayer(), ListItemWidgetClass);
		if (!Item)
		{
			// 인덱스 맞추기용 빈 칸
			EntryItemWidgets.Add(nullptr);
			continue;
		}

		Item->SetItemData(i, GetEntryTitle(CurrentEntries[i]), CurrentEntries[i].bIsNew);
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
		Txt_DetailTitle->SetText(GetEntryTitle(Entry));
	}

	if (Txt_DetailDescription)
	{
		Txt_DetailDescription->SetText(GetEntryDescription(Entry));
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

	// 다른 항목을 열면 본문 스크롤을 맨 위로
	if (Scroll_Detail)
	{
		Scroll_Detail->ScrollToStart();
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

	if (Scroll_Detail)
	{
		Scroll_Detail->ScrollToStart();
	}
}

void ULCONotebookWidget::HandleNotebookChanged()
{
	if (bIsRefreshing || !IsInViewport() || CurrentTab == ELCONotebookTab::Map)
	{
		return;
	}

	TGuardValue<bool> Guard(bIsRefreshing, true);
	if (CurrentTab == ELCONotebookTab::Deduction)
	{
		RebuildDeductionCandidates();
		RefreshDeductionSlots();
	}
	else
	{
		RebuildEntryList();
	}
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

// ======================================================================
// 추리 (단서·증언·추리 항목을 2~3개 골라 연결)
// ======================================================================

FText ULCONotebookWidget::GetUIText(FName Key) const
{
	if (Key.IsNone())
	{
		return FText::GetEmpty();
	}

	const UGameInstance* GameInstance = GetGameInstance();
	if (const ULCOLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr)
	{
		return Localization->GetUIText(Key);
	}

	return FText::FromName(Key);
}

FText ULCONotebookWidget::GetUITextOrDefault(FName Key, const FText& DefaultText) const
{
	// UI.csv에 키가 없으면 키 이름이 그대로 오므로 기본 문구로 대체
	const FText Text = GetUIText(Key);
	if (Text.IsEmpty() || Text.ToString() == Key.ToString())
	{
		return DefaultText;
	}
	return Text;
}

FText ULCONotebookWidget::GetEntryTitle(const FLCONotebookEntry& Entry) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (const ULCOLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr)
	{
		return Localization->GetNotebookEntryTitle(Entry.EntryID, Entry.Title);
	}
	return Entry.Title;
}

FText ULCONotebookWidget::GetEntryDescription(const FLCONotebookEntry& Entry) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (const ULCOLocalizationSubsystem* Localization = GameInstance ? GameInstance->GetSubsystem<ULCOLocalizationSubsystem>() : nullptr)
	{
		return Localization->GetNotebookEntryDescription(Entry.EntryID, Entry.Description);
	}
	return Entry.Description;
}

void ULCONotebookWidget::RefreshLocalizedEntries()
{
	if (IsInViewport())
	{
		SelectTab(CurrentTab);
	}
}

FText ULCONotebookWidget::GetDeductionHeaderText(ELCONotebookCategory Category) const
{
	FText DefaultText;
	switch (Category)
	{
	case ELCONotebookCategory::Clue:      DefaultText = FText::FromString(TEXT("증거")); break;
	case ELCONotebookCategory::Photo:     DefaultText = FText::FromString(TEXT("사진")); break;
	case ELCONotebookCategory::Testimony: DefaultText = FText::FromString(TEXT("증언")); break;
	case ELCONotebookCategory::Deduction: DefaultText = FText::FromString(TEXT("추리")); break;
	default:                              DefaultText = StaticEnum<ELCONotebookCategory>()->GetDisplayNameTextByValue(static_cast<int64>(Category)); break;
	}

	const FName* Key = DeductionHeaderTextKeys.Find(Category);
	return Key ? GetUITextOrDefault(*Key, DefaultText) : DefaultText;
}

void ULCONotebookWidget::AddListHeader(UPanelWidget* Panel, const FText& HeaderText)
{
	if (!Panel || !WidgetTree)
	{
		return;
	}

	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!Header)
	{
		return;
	}

	Header->SetText(HeaderText);
	Header->SetColorAndOpacity(DeductionHeaderColor);
	if (DeductionHeaderFont.HasValidFont())
	{
		Header->SetFont(DeductionHeaderFont);
	}
	Header->SetVisibility(ESlateVisibility::HitTestInvisible);

	UPanelSlot* HeaderSlot = Panel->AddChild(Header);
	if (UScrollBoxSlot* ScrollSlot = Cast<UScrollBoxSlot>(HeaderSlot))
	{
		ScrollSlot->SetPadding(DeductionHeaderPadding);
	}
	else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(HeaderSlot))
	{
		VerticalSlot->SetPadding(DeductionHeaderPadding);
	}
}

void ULCONotebookWidget::SortEntriesByGroup(TArray<FLCONotebookEntry>& InOutEntries) const
{
	// 묶음 순위: EntryGroupOrder 순서 → 그 외는 처음 나온 순서 → GroupID 없음은 맨 아래
	TMap<FName, int32> GroupRanks;
	for (int32 i = 0; i < EntryGroupOrder.Num(); ++i)
	{
		if (!EntryGroupOrder[i].IsNone() && !GroupRanks.Contains(EntryGroupOrder[i]))
		{
			GroupRanks.Add(EntryGroupOrder[i], GroupRanks.Num());
		}
	}

	for (const FLCONotebookEntry& Entry : InOutEntries)
	{
		if (!Entry.GroupID.IsNone() && !GroupRanks.Contains(Entry.GroupID))
		{
			GroupRanks.Add(Entry.GroupID, GroupRanks.Num());
		}
	}

	InOutEntries.StableSort([&GroupRanks](const FLCONotebookEntry& A, const FLCONotebookEntry& B)
	{
		const int32* RankA = A.GroupID.IsNone() ? nullptr : GroupRanks.Find(A.GroupID);
		const int32* RankB = B.GroupID.IsNone() ? nullptr : GroupRanks.Find(B.GroupID);
		return (RankA ? *RankA : MAX_int32) < (RankB ? *RankB : MAX_int32);
	});
}

FText ULCONotebookWidget::GetEntryGroupHeaderText(FName GroupID) const
{
	const FName Key(*(EntryGroupHeaderKeyPrefix + GroupID.ToString()));
	return GetUITextOrDefault(Key, FText::FromName(GroupID));
}

void ULCONotebookWidget::SetDeductionResultText(const FText& InText, const FText& InTitle)
{
	const bool bHasTitle = !InTitle.IsEmpty();

	if (Txt_DeductionResultTitle)
	{
		Txt_DeductionResultTitle->SetText(InTitle);
		Txt_DeductionResultTitle->SetVisibility(bHasTitle ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Txt_DeductionResult)
	{
		// 제목 위젯이 없으면 본문 맨 위에 제목을 붙임
		if (!Txt_DeductionResultTitle && bHasTitle && !InText.IsEmpty())
		{
			Txt_DeductionResult->SetText(FText::Format(FText::FromString(TEXT("{0}\n\n{1}")), InTitle, InText));
		}
		else
		{
			Txt_DeductionResult->SetText(InText);
		}
	}

	if (Scroll_DeductionResult)
	{
		Scroll_DeductionResult->ScrollToStart();
	}
}

void ULCONotebookWidget::RebuildDeductionCandidates()
{
	DeductionCandidates.Reset();
	DeductionItemWidgets.Reset();

	// 분류별 묶음 (머리말 표시용)
	struct FCandidateGroup
	{
		ELCONotebookCategory Category;
		int32 StartIndex;
		int32 Count;
	};
	TArray<FCandidateGroup> Groups;

	if (const ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
	{
		for (const ELCONotebookCategory Category : DeductionCandidateCategories)
		{
			const TArray<FLCONotebookEntry> Entries = Notebook->GetEntriesByCategory(Category);
			if (Entries.Num() == 0)
			{
				continue;
			}

			// 단서·증언은 수첩 탭과 같은 묶음 순서로 정렬 (머리말은 분류 단위만)
			TArray<FLCONotebookEntry> SortedEntries = Entries;
			if (GroupedCategories.Contains(Category))
			{
				SortEntriesByGroup(SortedEntries);
			}

			Groups.Add({ Category, DeductionCandidates.Num(), SortedEntries.Num() });
			DeductionCandidates.Append(SortedEntries);
		}
	}

	// 수첩에서 사라진 항목은 슬롯에서도 제거 (불러오기 등)
	SelectedDeductionIDs.RemoveAll([this](const FName& ID)
	{
		return !DeductionCandidates.ContainsByPredicate([ID](const FLCONotebookEntry& Entry) { return Entry.EntryID == ID; });
	});

	if (!Panel_DeductionCandidates)
	{
		return;
	}

	Panel_DeductionCandidates->ClearChildren();

	if (!ListItemWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCONotebook] ListItemWidgetClass가 비어 있음"));
		return;
	}

	for (const FCandidateGroup& Group : Groups)
	{
		// 분류 머리말
		AddListHeader(Panel_DeductionCandidates, GetDeductionHeaderText(Group.Category));

		// 항목 (DeductionItemWidgets 인덱스 = DeductionCandidates 인덱스)
		for (int32 i = Group.StartIndex; i < Group.StartIndex + Group.Count; ++i)
		{
			ULCONotebookListItemWidget* Item = CreateWidget<ULCONotebookListItemWidget>(GetOwningPlayer(), ListItemWidgetClass);
			if (!Item)
			{
				DeductionItemWidgets.Add(nullptr);
				continue;
			}

			Item->SetItemData(i, GetEntryTitle(DeductionCandidates[i]), DeductionCandidates[i].bIsNew);
			Item->SetSelected(SelectedDeductionIDs.Contains(DeductionCandidates[i].EntryID));
			Item->OnItemClicked.AddUniqueDynamic(this, &ThisClass::HandleDeductionCandidateClicked);
			Panel_DeductionCandidates->AddChild(Item);
			DeductionItemWidgets.Add(Item);
		}
	}
}

void ULCONotebookWidget::RefreshDeductionSlots()
{
	UTextBlock* SlotTexts[] = { Txt_DeductionSlot1, Txt_DeductionSlot2, Txt_DeductionSlot3 };
	UButton* SlotButtons[] = { Btn_DeductionSlot1, Btn_DeductionSlot2, Btn_DeductionSlot3 };

	const FText EmptyText = GetUIText(DeductionEmptySlotTextKey);

	for (int32 SlotIndex = 0; SlotIndex < UE_ARRAY_COUNT(SlotTexts); ++SlotIndex)
	{
		const bool bSlotUsed = SlotIndex < MaxDeductionSlots;

		// 사용하지 않는 칸 (MaxDeductionSlots = 2일 때 3번 칸)
		if (SlotButtons[SlotIndex])
		{
			SlotButtons[SlotIndex]->SetVisibility(bSlotUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}

		if (!SlotTexts[SlotIndex])
		{
			continue;
		}

		FText SlotText = EmptyText;
		if (SelectedDeductionIDs.IsValidIndex(SlotIndex))
		{
			const FName ID = SelectedDeductionIDs[SlotIndex];
			if (const FLCONotebookEntry* Entry = DeductionCandidates.FindByPredicate([ID](const FLCONotebookEntry& E) { return E.EntryID == ID; }))
			{
				SlotText = GetEntryTitle(*Entry);
			}
		}
		SlotTexts[SlotIndex]->SetText(SlotText);
	}

	// 목록 선택 표시
	for (int32 i = 0; i < DeductionItemWidgets.Num(); ++i)
	{
		if (DeductionItemWidgets[i] && DeductionCandidates.IsValidIndex(i))
		{
			DeductionItemWidgets[i]->SetSelected(SelectedDeductionIDs.Contains(DeductionCandidates[i].EntryID));
		}
	}
}

void ULCONotebookWidget::HandleDeductionCandidateClicked(int32 ItemIndex)
{
	if (!DeductionCandidates.IsValidIndex(ItemIndex))
	{
		return;
	}

	FLCONotebookEntry& Entry = DeductionCandidates[ItemIndex];

	// 넣기 / 빼기
	if (SelectedDeductionIDs.Contains(Entry.EntryID))
	{
		SelectedDeductionIDs.Remove(Entry.EntryID);
	}
	else if (SelectedDeductionIDs.Num() < MaxDeductionSlots)
	{
		SelectedDeductionIDs.Add(Entry.EntryID);
	}

	// 클릭한 항목 내용 보여주기 (추리 항목이면 예전 독백 다시 읽기)
	SetDeductionResultText(GetEntryDescription(Entry), GetEntryTitle(Entry));

	// 읽음 처리
	if (Entry.bIsNew)
	{
		Entry.bIsNew = false;
		if (DeductionItemWidgets.IsValidIndex(ItemIndex) && DeductionItemWidgets[ItemIndex])
		{
			DeductionItemWidgets[ItemIndex]->SetIsNew(false);
		}

		if (ULCONotebookSubsystem* Notebook = GetNotebookSubsystem())
		{
			TGuardValue<bool> Guard(bIsRefreshing, true);
			Notebook->MarkEntryAsRead(Entry.EntryID);
		}
	}

	RefreshDeductionSlots();
}

void ULCONotebookWidget::RemoveDeductionSlot(int32 SlotIndex)
{
	if (SelectedDeductionIDs.IsValidIndex(SlotIndex))
	{
		SelectedDeductionIDs.RemoveAt(SlotIndex);
		RefreshDeductionSlots();
	}
}

void ULCONotebookWidget::HandleDeductionSlot1Clicked() { RemoveDeductionSlot(0); }
void ULCONotebookWidget::HandleDeductionSlot2Clicked() { RemoveDeductionSlot(1); }
void ULCONotebookWidget::HandleDeductionSlot3Clicked() { RemoveDeductionSlot(2); }

void ULCONotebookWidget::HandleDeductionClearClicked()
{
	SelectedDeductionIDs.Reset();
	RefreshDeductionSlots();
	ShowDeductionHintOrClear();
}

bool ULCONotebookWidget::HasAvailableDeduction() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULCOStorySubsystem* Story = GameInstance ? GameInstance->GetSubsystem<ULCOStorySubsystem>() : nullptr;
	if (!Story)
	{
		return false;
	}

	// GetUnlockableFlags = 아직 없고 조건을 모두 갖춘 플래그 (증언은 대화로 얻으므로 추리(K)만 확인)
	for (const ELCOStoryFlag Flag : Story->GetUnlockableFlags())
	{
		if (ULCOStorySubsystem::IsDeductionFlag(Flag))
		{
			return true;
		}
	}
	return false;
}

void ULCONotebookWidget::ShowDeductionHintOrClear()
{
	// 결과 칸은 비우고, 조합 가능한 추리가 있으면 주인공 독백으로 힌트
	SetDeductionResultText(FText::GetEmpty());
	ShowDeductionHintMonologue();
}

void ULCONotebookWidget::ShowDeductionHintMonologue()
{
	if (HasAvailableDeduction())
	{
		ULCOMonologueSubsystem::ShowMonologue(this, GetUITextOrDefault(DeductionHintTextKey, FText::FromString(TEXT("흠… 뭔가 이어 볼 만한 단서가 있을 것 같은데."))));
	}
}

void ULCONotebookWidget::HandleDeductionCombineClicked()
{
	const UGameInstance* GameInstance = GetGameInstance();
	ULCOStorySubsystem* Story = GameInstance ? GameInstance->GetSubsystem<ULCOStorySubsystem>() : nullptr;
	if (!Story)
	{
		return;
	}

	ELCOStoryFlag MatchedFlag = ELCOStoryFlag::K01;
	ELCODeductionResult Result;
	{
		// 획득 → 수첩 기록 → OnNotebookChanged 로 목록이 중간에 다시 그려지지 않도록 막음
		TGuardValue<bool> Guard(bIsRefreshing, true);
		Result = Story->TryDeduce(SelectedDeductionIDs, MatchedFlag);
	}

	FText ResultText;
	FText ResultTitle;
	FName NewDeductionID = NAME_None;
	switch (Result)
	{
	case ELCODeductionResult::Success:
	{
		// 독백 = 수첩에 방금 기록된 추리 항목의 설명
		NewDeductionID = ULCOStorySubsystem::GetFlagName(MatchedFlag);
		FLCONotebookEntry DeductionEntry;
		if (const ULCONotebookSubsystem* Notebook = GetNotebookSubsystem();
			Notebook && Notebook->FindEntry(NewDeductionID, DeductionEntry))
		{
			ResultText = GetEntryDescription(DeductionEntry);

			// 결과 제목: "새로운 추리 — 제목"
			const FText TitleFormat = GetUITextOrDefault(DeductionSuccessTitleTextKey, FText::FromString(TEXT("새로운 추리 — {0}")));
			ResultTitle = FText::Format(TitleFormat, GetEntryTitle(DeductionEntry));
		}
		SelectedDeductionIDs.Reset();
		break;
	}
	case ELCODeductionResult::AlreadyKnown:
		ResultText = GetUIText(DeductionAlreadyKnownTextKey);
		break;
	case ELCODeductionResult::NotEnough:
		ResultText = GetUIText(DeductionNotEnoughTextKey);
		break;
	default:
		// 실패해도 선택은 유지 (하나만 바꿔서 다시 시도할 수 있게)
		if (DeductionFailTextKeys.Num() > 0)
		{
			ResultText = GetUIText(DeductionFailTextKeys[FMath::RandRange(0, DeductionFailTextKeys.Num() - 1)]);
		}
		break;
	}

	RebuildDeductionCandidates();
	RefreshDeductionSlots();
	SetDeductionResultText(ResultText, ResultTitle);

	// 새 추리 항목이 보이도록 후보 목록 스크롤
	if (!NewDeductionID.IsNone())
	{
		if (UScrollBox* CandidateScroll = Cast<UScrollBox>(Panel_DeductionCandidates))
		{
			const int32 NewIndex = DeductionCandidates.IndexOfByPredicate([NewDeductionID](const FLCONotebookEntry& E) { return E.EntryID == NewDeductionID; });
			if (DeductionItemWidgets.IsValidIndex(NewIndex) && DeductionItemWidgets[NewIndex])
			{
				CandidateScroll->ScrollWidgetIntoView(DeductionItemWidgets[NewIndex], true);
			}
		}
	}

	OnDeductionResult(Result);
}
