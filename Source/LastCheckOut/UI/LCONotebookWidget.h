#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Notebook/LCONotebookTypes.h"
#include "Story/LCOStoryTypes.h"
#include "LCONotebookWidget.generated.h"

class UButton;
class UImage;
class UPanelWidget;
class UScrollBox;
class UTextBlock;
class UWidgetSwitcher;
class ULCONotebookListItemWidget;
class ULCONotebookSubsystem;

UENUM(BlueprintType)
enum class ELCONotebookTab : uint8
{
	Objective  UMETA(DisplayName = "목표"),
	Clue       UMETA(DisplayName = "단서·증거"),
	Testimony  UMETA(DisplayName = "증언"),
	Photo      UMETA(DisplayName = "사진·도면"),
	Map        UMETA(DisplayName = "지도"),
	Deduction  UMETA(DisplayName = "추리")
};

/**
 * 수첩 위젯 베이스 (WBP_Notebook의 부모)
 * - J 키(LCOPlayerController::OnNotebookRequested)로 열기/닫기
 * - 기본은 시간 정지 없음 (bPauseGameWhileOpen으로 변경 가능), 이동/시점만 막힘
 * - 위젯 생성은 ULCONotebookUISubsystem이 자동 처리 (DefaultGame.ini에 클래스 지정)
 *
 * [디자이너 배치 - 이름 그대로]
 * 탭(필수)    : Btn_Tab_Objective / Btn_Tab_Clue / Btn_Tab_Testimony / Btn_Tab_Photo / Btn_Tab_Map
 * 페이지(필수) : Switcher_Pages  (0번 = 목록/상세, 1번 = 지도)
 * 목록/상세   : Panel_EntryList(필수, ScrollBox 권장), Txt_DetailTitle, Txt_DetailDescription, Img_DetailImage
 *              Txt_EmptyList(선택), Txt_PageTitle(선택)
 * 지도        : Panel_MapAreaList(필수), Img_Map(필수), Txt_MapAreaName(선택)
 * 기타        : Btn_Close(선택)
 *
 * [추리 탭 - 전부 선택 사항, 없으면 추리 탭만 동작 안 함]
 * 탭          : Btn_Tab_Deduction
 * 페이지      : Switcher_Pages의 2번 (DeductionPageIndex로 변경 가능)
 * 후보 목록   : Panel_DeductionCandidates (ScrollBox 권장, 단서·증언·추리 항목이 나옴. 클릭 = 슬롯에 넣기/빼기)
 * 슬롯        : Txt_DeductionSlot1~3 (고른 항목 이름), Btn_DeductionSlot1~3 (클릭 시 해당 칸 비우기)
 * 버튼        : Btn_DeductionCombine (연결하기), Btn_DeductionClear (비우기)
 * 결과        : Txt_DeductionResult (클릭한 항목 설명 / 조합 결과 독백)
 *              Txt_DeductionResultTitle (선택, 결과 제목), Scroll_DeductionResult (선택, 결과 스크롤)
 * 후보 머리말  : 분류별 머리말(증거 / 사진 / 증언 / 추리)을 코드에서 자동 생성 (글꼴·색은 Class Defaults)
 */
UCLASS(Abstract)
class LASTCHECKOUT_API ULCONotebookWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void OpenNotebook();

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void CloseNotebook();

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void ToggleNotebook();

	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	bool IsNotebookOpen() const { return IsInViewport(); }

	UFUNCTION(BlueprintCallable, Category = "LCO|Notebook")
	void SelectTab(ELCONotebookTab NewTab);

	UFUNCTION(BlueprintPure, Category = "LCO|Notebook")
	ELCONotebookTab GetCurrentTab() const { return CurrentTab; }

	void UnbindFromNotebookInput();

	// 언어 변경 시 열려 있는 수첩 내용을 다시 그림 (LCONotebookUISubsystem에서 호출)
	void RefreshLocalizedEntries();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	// ---------- BP 연출용 이벤트 ----------

	// 책장 넘기는 소리 등
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Notebook")
	void OnTabChanged(ELCONotebookTab NewTab);

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Notebook")
	void OnNotebookOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Notebook")
	void OnNotebookClosed();

	// 조합 결과 연출 (성공 효과음, 실패 시 흔들림 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Notebook|Deduction")
	void OnDeductionResult(ELCODeductionResult Result);

	// ---------- 설정 (WBP 클래스 디폴트) ----------

	// 목록 한 줄 위젯 (WBP_NotebookListItem)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook")
	TSubclassOf<ULCONotebookListItemWidget> ListItemWidgetClass;

	// 지도 탭 데이터 (DA_NotebookMap)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook")
	TObjectPtr<ULCONotebookMapData> MapData;

	// 탭별 페이지 제목 (Txt_PageTitle에 표시, 비우면 표시 안 함)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook")
	TMap<ELCONotebookTab, FText> TabTitles;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook")
	int32 ViewportZOrder = 6;

	// 수첩을 여는 동안 게임 일시정지 여부 (false = 시간이 계속 흐름, 관리인 접근 가능)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook")
	bool bPauseGameWhileOpen = false;

	// 처음 열 때 테스트 항목을 수첩에 넣음 (데이터 연결 전 UI 확인용, 다 되면 끄기)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Debug")
	bool bAddDebugEntries = false;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Debug", meta = (EditCondition = "bAddDebugEntries"))
	TArray<FLCONotebookEntry> DebugEntries;

	// ---------- 추리 탭 설정 ----------

	// Switcher_Pages에서 추리 페이지 번호
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	int32 DeductionPageIndex = 2;

	// 후보 목록에 나올 분류 (위에서부터 순서대로)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	TArray<ELCONotebookCategory> DeductionCandidateCategories = { ELCONotebookCategory::Clue, ELCONotebookCategory::Photo, ELCONotebookCategory::Testimony, ELCONotebookCategory::Deduction };

	// 한 번에 고를 수 있는 최대 개수 (슬롯 위젯 수와 맞추기)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction", meta = (ClampMin = "2", ClampMax = "3"))
	int32 MaxDeductionSlots = 3;

	// 실패 독백 (UI.csv 키, 무작위 1개)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	TArray<FName> DeductionFailTextKeys = { TEXT("NOTEBOOK_DEDUCTION_FAIL_01"), TEXT("NOTEBOOK_DEDUCTION_FAIL_02"), TEXT("NOTEBOOK_DEDUCTION_FAIL_03") };

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	FName DeductionNotEnoughTextKey = TEXT("NOTEBOOK_DEDUCTION_NOT_ENOUGH");

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	FName DeductionAlreadyKnownTextKey = TEXT("NOTEBOOK_DEDUCTION_ALREADY");

	// 빈 슬롯 표시 (UI.csv 키)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	FName DeductionEmptySlotTextKey = TEXT("NOTEBOOK_DEDUCTION_EMPTY_SLOT");

	// 조합 성공 시 결과 제목 (UI.csv 키, {0} = 새 추리 제목. 키가 없으면 "새로운 추리 — {0}")
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	FName DeductionSuccessTitleTextKey = TEXT("NOTEBOOK_DEDUCTION_SUCCESS_TITLE");

	// 수첩 / 추리 탭을 열었을 때 지금 조합 가능한 추리가 있으면 독백 UI로 띄우는 문구 (UI.csv 키, 없으면 기본 문구)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction")
	FName DeductionHintTextKey = TEXT("NOTEBOOK_DEDUCTION_HINT");

	// ---------- 추리 후보 목록 분류 머리말 ----------

	// 분류별 머리말 (UI.csv 키. 키가 없으면 증거 / 사진 / 증언 / 추리로 표시)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction|Header")
	TMap<ELCONotebookCategory, FName> DeductionHeaderTextKeys = {
		{ ELCONotebookCategory::Clue,      FName(TEXT("NOTEBOOK_DEDUCTION_HEADER_CLUE")) },
		{ ELCONotebookCategory::Photo,     FName(TEXT("NOTEBOOK_DEDUCTION_HEADER_PHOTO")) },
		{ ELCONotebookCategory::Testimony, FName(TEXT("NOTEBOOK_DEDUCTION_HEADER_TESTIMONY")) },
		{ ELCONotebookCategory::Deduction, FName(TEXT("NOTEBOOK_DEDUCTION_HEADER_DEDUCTION")) }
	};

	// 머리말 글꼴 (비워두면 기본 글꼴)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction|Header")
	FSlateFontInfo DeductionHeaderFont;

	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction|Header")
	FSlateColor DeductionHeaderColor = FSlateColor(FLinearColor(0.32f, 0.22f, 0.15f, 1.0f));

	// 머리말 여백 (위쪽을 넓게 줘서 분류 사이를 띄움)
	UPROPERTY(EditDefaultsOnly, Category = "LCO|Notebook|Deduction|Header")
	FMargin DeductionHeaderPadding = FMargin(4.0f, 12.0f, 4.0f, 4.0f);

	// ---------- 바인딩 위젯 ----------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Tab_Objective;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Tab_Clue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Tab_Testimony;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Tab_Photo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Tab_Map;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> Switcher_Pages;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_EntryList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_DetailTitle;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Txt_DetailDescription;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_DetailImage;

	// 상세 본문 스크롤 (선택, 항목을 바꾸면 맨 위로 이동)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> Scroll_Detail;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_MapAreaList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Img_Map;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_EmptyList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_PageTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_MapAreaName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Close;

	// 추리 탭 (전부 선택)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Tab_Deduction;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_DeductionCandidates;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DeductionSlot1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DeductionSlot2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DeductionSlot3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_DeductionSlot1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_DeductionSlot2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_DeductionSlot3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_DeductionCombine;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_DeductionClear;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DeductionResult;

	// 추리 결과 제목 (선택. 클릭한 항목 제목 / "새로운 추리 — ..." 표시, 없으면 본문 맨 위에 붙임)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_DeductionResultTitle;

	// 추리 결과 / 설명 스크롤 (선택, 문구가 바뀌면 맨 위로 이동)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> Scroll_DeductionResult;

private:
	// 입력
	UFUNCTION()
	void HandleNotebookRequested();

	// 탭 버튼
	UFUNCTION()
	void HandleTabObjectiveClicked();

	UFUNCTION()
	void HandleTabClueClicked();

	UFUNCTION()
	void HandleTabTestimonyClicked();

	UFUNCTION()
	void HandleTabPhotoClicked();

	UFUNCTION()
	void HandleTabMapClicked();

	UFUNCTION()
	void HandleTabDeductionClicked();

	UFUNCTION()
	void HandleCloseClicked();

	// 목록 클릭
	UFUNCTION()
	void HandleEntryClicked(int32 ItemIndex);

	UFUNCTION()
	void HandleMapAreaClicked(int32 ItemIndex);

	// 추리 탭
	UFUNCTION()
	void HandleDeductionCandidateClicked(int32 ItemIndex);

	UFUNCTION()
	void HandleDeductionSlot1Clicked();

	UFUNCTION()
	void HandleDeductionSlot2Clicked();

	UFUNCTION()
	void HandleDeductionSlot3Clicked();

	UFUNCTION()
	void HandleDeductionCombineClicked();

	UFUNCTION()
	void HandleDeductionClearClicked();

	// 수첩 내용 변경 (열려 있을 때만 갱신)
	UFUNCTION()
	void HandleNotebookChanged();

	void RebuildEntryList();
	void SelectEntry(int32 ItemIndex);
	void ClearDetail();

	void RebuildMapAreaList();
	void SelectMapArea(int32 ItemIndex);

	void RebuildDeductionCandidates();
	void RefreshDeductionSlots();
	void RemoveDeductionSlot(int32 SlotIndex);
	void SetDeductionResultText(const FText& InText, const FText& InTitle = FText::GetEmpty());
	FText GetUIText(FName Key) const;
	FText GetUITextOrDefault(FName Key, const FText& DefaultText) const;
	FText GetDeductionHeaderText(ELCONotebookCategory Category) const;

	// 항목 제목 / 본문 (Notebook.csv 번역 우선, 없으면 DataTable 원문)
	FText GetEntryTitle(const FLCONotebookEntry& Entry) const;
	FText GetEntryDescription(const FLCONotebookEntry& Entry) const;

	// 아직 얻지 않은 추리 중 지금 조합 가능한 것이 있는지
	bool HasAvailableDeduction() const;
	// 결과 칸 비우고, 조합 가능하면 힌트 독백
	void ShowDeductionHintOrClear();
	// 조합 가능한 추리가 있으면 독백 UI로 힌트 표시
	void ShowDeductionHintMonologue();

	void UpdatePageTitle();
	void SetPlayerUIMode(bool bEnable);
	void AddDebugEntriesOnce();

	ULCONotebookSubsystem* GetNotebookSubsystem() const;
	static ELCONotebookCategory TabToCategory(ELCONotebookTab Tab);

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULCONotebookListItemWidget>> EntryItemWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULCONotebookListItemWidget>> MapAreaItemWidgets;

	// 현재 탭에 표시 중인 항목 (목록 순서와 동일)
	TArray<FLCONotebookEntry> CurrentEntries;

	// 추리 탭 후보 (목록 순서와 동일)
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULCONotebookListItemWidget>> DeductionItemWidgets;

	TArray<FLCONotebookEntry> DeductionCandidates;

	// 슬롯에 넣은 항목 ID (넣은 순서)
	TArray<FName> SelectedDeductionIDs;

	ELCONotebookTab CurrentTab = ELCONotebookTab::Objective;
	int32 SelectedEntryIndex = INDEX_NONE;
	int32 SelectedMapAreaIndex = INDEX_NONE;

	// 목록 갱신 중 읽음 처리로 인한 재귀 갱신 방지
	bool bIsRefreshing = false;
	bool bDebugEntriesAdded = false;
};
