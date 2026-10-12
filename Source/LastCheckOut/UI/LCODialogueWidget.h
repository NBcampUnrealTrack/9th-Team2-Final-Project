#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LCODialogueWidget.generated.h"

class ALCONPCBase;
class UBorder;
class UPanelWidget;
class USoundBase;
class UTextBlock;
class ULCODialogueChoiceWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCODialogueWidgetClosed, ALCONPCBase*, NPC);

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

	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	bool OpenDialogue(ALCONPCBase* InNPC, AActor* InInteractor, bool bPlayFirstConversation);

	UFUNCTION(BlueprintCallable, Category = "LCO|Dialogue")
	void CloseDialogue(bool bEndNPCConversation = true);

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
	bool bBlockLookInputWhileOpen = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue")
	FText FallbackSpeakerName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Typewriter", meta = (ClampMin = "0.0"))
	float TypewriterInterval = 0.035f;

	UFUNCTION(BlueprintImplementableEvent, Category = "LCO|Dialogue|Typewriter")
	void OnDialogueCharacterRevealed(const FString& RevealedCharacter);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LCO|Dialogue|Voice")
	TMap<FName, FLCODialogueVoiceSettings> VoiceSettingsByDialogueID;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Border_Choices;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ChoicesPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> Border_Dialogue;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_SpeakerName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Dialogue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ContinueHint;

private:
	UFUNCTION()
	void HandleLanguageChanged(FName NewLanguage);

	UFUNCTION()
	void HandleChoiceSelected(FName ChoiceID);

	bool LoadFlowForDialogueID(FName DialogueID);
	void StartNode(FName NodeID);
	void ShowCurrentLine();
	void AdvanceDialogue();
	void EndActiveConversation();
	void StartTypewriter(const FText& FullText);
	void RevealNextTypewriterCharacter();
	void FinishTypewriter();
	void HandleTypewriterCompleted();
	void ClearTypewriterTimer();
	int32 GetNextTypewriterIndex() const;
	void RefreshChoices();
	void RefreshVisibleText();
	void SetChoicesVisible(bool bVisible);
	bool IsEndNode(FName NodeID) const;
	bool IsConditionMet(FName Condition) const;
	bool ValidateDialogueTextKey(FName TextKey, const FString& Context) const;
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
	FText ResolveSpeakerName(FName SpeakerID) const;

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

	FLCODialogueFlow ActiveFlow;

	UPROPERTY(Transient)
	TMap<FName, FName> VisibleChoiceTargets;

	UPROPERTY(Transient)
	bool bDialogueInputModeApplied = false;

	UPROPERTY(Transient)
	bool bPreviousShowMouseCursor = false;

	UPROPERTY(Transient)
	bool bLookInputBlockedByThisWidget = false;
};
