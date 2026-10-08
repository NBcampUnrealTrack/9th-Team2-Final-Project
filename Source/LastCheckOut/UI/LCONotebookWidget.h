#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Notebook/LCONotebookTypes.h"
#include "LCONotebookWidget.generated.h"

class UButton;
class UImage;
class UPanelWidget;
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
	Map        UMETA(DisplayName = "지도")
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
	void HandleCloseClicked();

	// 목록 클릭
	UFUNCTION()
	void HandleEntryClicked(int32 ItemIndex);

	UFUNCTION()
	void HandleMapAreaClicked(int32 ItemIndex);

	// 수첩 내용 변경 (열려 있을 때만 갱신)
	UFUNCTION()
	void HandleNotebookChanged();

	void RebuildEntryList();
	void SelectEntry(int32 ItemIndex);
	void ClearDetail();

	void RebuildMapAreaList();
	void SelectMapArea(int32 ItemIndex);

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

	ELCONotebookTab CurrentTab = ELCONotebookTab::Objective;
	int32 SelectedEntryIndex = INDEX_NONE;
	int32 SelectedMapAreaIndex = INDEX_NONE;

	// 목록 갱신 중 읽음 처리로 인한 재귀 갱신 방지
	bool bIsRefreshing = false;
	bool bDebugEntriesAdded = false;
};
