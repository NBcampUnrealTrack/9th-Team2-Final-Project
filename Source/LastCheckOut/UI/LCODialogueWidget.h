#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCODialogueWidget.generated.h"

class ALCONPCBase;
class APlayerController;
class UBorder;
class UImage;
class UPanelWidget;
class USoundBase;
class UTextBlock;
class UTexture2D;
class ULCODialogueChoiceWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnLCODialogueWidgetClosed, ALCONPCBase*, NPC, FName, DialogueID, bool, bCompletedNormally, bool, bCompletedFirstConversation);

USTRUCT(BlueprintType)
struct LASTCHECKOUT_API FLCODialogueVoiceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice", meta = (ClampMin = "0.0"))
	float BasePitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice", meta = (ClampMin = "0.0"))
	float PitchRandomMin = 0.92f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice", meta = (ClampMin = "0.0"))
	float PitchRandomMax = 1.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice", meta = (ClampMin = "0.0"))
	float MinPlaybackInterval = 0.06f;
};

struct FLCODialogueChoiceRow
{
	FName ChoiceID = NAME_None;
	FName ChoiceTextKey = NAME_None;
	FName ChoiceTargetNodeID = NAME_None;
	FName Condition = NAME_None;
	// Optional alternate destination; evaluated on selection using the same single-ID condition policy.
	FName AlternateCondition = NAME_None;
	FName AlternateTargetNodeID = NAME_None;
	bool bEndConversation = false;
};

struct FLCODialogueNode
{
	FName NodeID = NAME_None;
	FName RowType = NAME_None;
	FName SpeakerID = NAME_None;
	FName TextKey = NAME_None;
	FName NextNodeID = NAME_None;
	bool bEndConversation = false;
	// 이 대사가 나올 때 획득하는 증언 / 키워드 (CSV의 UnlockFlag 열, 예: T04). 비어 있으면 없음
	FName UnlockFlag = NAME_None;
	// Optional CSV action on normal completion. Only Manager K05 is connected.
	FName ReportKeyword = NAME_None;
	// 선택 CSV 열. 0이면 기존 수동 진행, 양수이면 입력 없이 지정 시간 후 진행한다.
	float AutoAdvanceSeconds = 0.f;
	TArray<FLCODialogueChoiceRow> Choices;
};

struct FLCODialogueFlow
{
	FString SourcePath;
	TMap<FName, FLCODialogueNode> Nodes;
};

UCLASS(Abstract)
class LASTCHECKOUT_API ULCODialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULCODialogueWidget(const FObjectInitializer& ObjectInitializer);

	// NodePrefix: 챕터별 대화 시작 노드 앞글자 (예: "C2_" → C2_FIRST_01 / C2_DEFAULT_01). 비우면 FIRST_01 / DEFAULT_01
	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	bool OpenDialogue(ALCONPCBase* InNPC, AActor* InInteractor, bool bPlayFirstConversation, const FString& NodePrefix = TEXT(""));

	// NPC 없이 대화 열기 (전화기 등). DialogueFlow_<DialogueID>.csv를 StartNodeID부터 재생
	// 이미 대화 중이거나 CSV / 노드가 없으면 false
	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	bool OpenDialogueByID(FName DialogueID, FName StartNodeID, AActor* InInteractor, bool bPlayFirstConversation = false);

	UFUNCTION(BlueprintPure, Category = "LCO|Dialogue")
	bool IsDialogueActive() const { return !ActiveDialogueID.IsNone(); }

	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	void CloseDialogue(bool bEndNPCConversation = true);

	void UnbindFromOptionsInput();

	UPROPERTY(BlueprintAssignable, Category = "LCO|Dialogue")
	FOnLCODialogueWidgetClosed OnDialogueWidgetClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue")
	TSoftClassPtr<ULCODialogueChoiceWidget> ChoiceWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue")
	int32 ViewportZOrder = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue")
	bool bRestoreGameInputOnClose = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue")
	FText FallbackSpeakerName;

	// CSV의 SpeakerID를 키로 사용한다. DialogueID와는 별개다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Portrait")
	TMap<FName, TObjectPtr<UTexture2D>> SpeakerPortraitsBySpeakerID;

	// 미등록/빈 초상화는 이 이미지로 대체한다. 지정하지 않으면 초상화 영역을 숨긴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Portrait")
	TObjectPtr<UTexture2D> DefaultSpeakerPortrait;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Typewriter", meta = (ClampMin = "0.0"))
	float TypewriterInterval = 0.035f;

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Dialogue|Typewriter")
	void OnDialogueCharacterRevealed(const FString& RevealedCharacter);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice")
	TMap<FName, FLCODialogueVoiceSettings> VoiceSettingsByDialogueID;

	// General dialogue: SpeakerID override. An entry with no Sound intentionally stays silent.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice")
	TMap<FName, FLCODialogueVoiceSettings> VoiceSettingsBySpeakerID;

	// WorkSenior phone dialogue only: keys are CSV SpeakerIDs (WorkSenior / Protagonist).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice")
	TMap<FName, FLCODialogueVoiceSettings> WorkSeniorVoiceSettingsBySpeakerID;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Border_Choices;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ChoicesPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Border_Dialogue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SpeakerName;

	// WBP_Dialogue의 Image_75를 이 이름으로 변경하면 자동 연결된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_SpeakerPortrait;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Dialogue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ContinueHint;

private:
	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	UFUNCTION()
	void HandleOptionsRequested();

	UFUNCTION()
	void HandleChoiceSelected(FName ChoiceID);

	bool LoadFlowForDialogueID(FName DialogueID);
	void StartNode(FName NodeID);
	void ShowCurrentLine();
	void AdvanceDialogue();
	void HandleAutoAdvance();
	void ClearAutoAdvanceTimer();
	void CompleteActiveConversation();
	void EndActiveConversation();
	void StartTypewriter(const FText& FullText);
	void RevealNextTypewriterCharacter();
	void FinishTypewriter();
	void HandleTypewriterCompleted();
	void ClearTypewriterTimer();
	int32 GetNextTypewriterIndex() const;
	void RefreshChoices();
	bool HasAvailableQuestions(const FLCODialogueNode& ChoiceNode) const;
	bool IsExitChoice(const FLCODialogueChoiceRow& Choice) const;
	bool TryStartNoQuestionsDialogue(const FLCODialogueNode& Node);
	void RefreshVisibleText();
	void RefreshSpeakerPresentation(FName SpeakerID);
	void SetChoicesVisible(bool bVisible);
	bool IsEndNode(FName NodeID) const;
	bool IsConditionMet(FName Condition) const;
	bool UsesActOneNotebookConditions() const;
	bool ValidateDialogueTextKey(FName TextKey, const FString& Context) const;
	bool ValidateSpeakerNameKey(FName SpeakerID, const FString& Context) const;
	void PlayDialogueCharacterSound(const FString& RevealedCharacter);
	bool ShouldPlayDialogueCharacterSound(const FString& RevealedCharacter) const;
	const FLCODialogueVoiceSettings* GetActiveVoiceSettings() const;
	void BindLocalizationEvents();
	void UnbindLocalizationEvents();
	void ApplyDialogueInputMode();
	void RestoreGameInputMode();
	bool ShouldRestoreGameOnlyInputMode(APlayerController* PC) const;
	bool HasEvidenceToken(FName EvidenceID) const;
	FText ResolveDialogueText(FName Key) const;
	FText ReplaceDialogueKeyPlaceholders(const FText& LocalizedText) const;
	FString ResolveDialogueKeyBinding(FName BindingName) const;
	FText ResolveSpeakerName(FName SpeakerID) const;

	// OpenDialogue / OpenDialogueByID 공통 처리. InNPC는 NPC 없는 대화면 nullptr
	bool OpenDialogueInternal(ALCONPCBase* InNPC, AActor* InInteractor, FName DialogueID, FName InitialNodeID, bool bPlayFirstConversation);

	UPROPERTY(Transient)
	TWeakObjectPtr<ALCONPCBase> ActiveNPC;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Interactor;

	UPROPERTY(Transient)
	FName ActiveDialogueID = NAME_None;

	UPROPERTY(Transient)
	FName ActiveNodeID = NAME_None;

	UPROPERTY(Transient)
	FString FullTypewriterString;

	UPROPERTY(Transient)
	int32 TypewriterVisibleLength = 0;

	UPROPERTY(Transient)
	bool bIsTypewriting = false;

	UPROPERTY(Transient)
	float LastVoicePlaybackTime = -FLT_MAX;

	FTimerHandle TypewriterTimerHandle;
	FTimerHandle AutoAdvanceTimerHandle;
	bool bAutoAdvanceActive = false;

	FLCODialogueFlow ActiveFlow;

	UPROPERTY(Transient)
	TMap<FName, FName> VisibleChoiceTargets;

	UPROPERTY(Transient)
	bool bDialogueInputModeApplied = false;

	UPROPERTY(Transient)
	bool bPreviousShowMouseCursor = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> InputModePlayerController;

	UPROPERTY(Transient)
	bool bMoveInputBlockedByThisWidget = false;

	UPROPERTY(Transient)
	bool bLookInputBlockedByThisWidget = false;

	UPROPERTY(Transient)
	bool bClosingFromOptionsRequest = false;

	UPROPERTY(Transient)
	bool bIsClosingDialogue = false;

	UPROPERTY(Transient)
	bool bActiveDialogueStartedAsFirstConversation = false;

	UPROPERTY(Transient)
	bool bActiveDialogueCompletedNormally = false;
};
