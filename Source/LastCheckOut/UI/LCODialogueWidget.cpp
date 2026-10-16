#include "UI/LCODialogueWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Inventory/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Notebook/NotebookComponent.h"
#include "Notebook/LCONotebookSubsystem.h"
#include "NPC/LCONPCBase.h"
#include "Player/LCOPlayerController.h"
#include "Serialization/Csv/CsvParser.h"
#include "Story/LCOStorySubsystem.h"
#include "TimerManager.h"
#include "UI/LCODialogueChoiceWidget.h"
#include "UI/LCOInteractionPromptSubsystem.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FName RowTypeLine = TEXT("Line");
	const FName RowTypeChoice = TEXT("Choice");
	const FName SystemSpeakerID = TEXT("System");
	const FName WorkSeniorDialogueID = TEXT("WorkSenior");
	const FName ProtagonistSpeakerID = TEXT("Protagonist");
	const FName FirstNodeID = TEXT("FIRST_01");
	const FName DefaultNodeID = TEXT("DEFAULT_01");
	const FName NoQuestionsNodeID = TEXT("__LCO_NO_QUESTIONS__");
	const FName EndNodeID = TEXT("END");

	bool IsHighSurrogate(TCHAR Character)
	{
		return Character >= 0xD800 && Character <= 0xDBFF;
	}

	bool IsLowSurrogate(TCHAR Character)
	{
		return Character >= 0xDC00 && Character <= 0xDFFF;
	}

	bool IsDialogueSilentCharacter(TCHAR Character)
	{
		if (FChar::IsWhitespace(Character))
		{
			return true;
		}

		switch (Character)
		{
		case '.':
		case ',':
		case '!':
		case '?':
		case ';':
		case ':':
		case '"':
		case '\'':
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case '-':
			return true;
		default:
			break;
		}

		const uint32 CodeUnit = static_cast<uint32>(Character);
		return CodeUnit == 0x3001
			|| CodeUnit == 0x3002
			|| CodeUnit == 0xFF0C
			|| CodeUnit == 0xFF01
			|| CodeUnit == 0xFF1F
			|| CodeUnit == 0x2026;
	}

	FName CsvCellAsName(const TArray<const TCHAR*>& Row, const TMap<FString, int32>& ColumnIndices, const TCHAR* ColumnName)
	{
		const int32* ColumnIndex = ColumnIndices.Find(FString(ColumnName));
		if (!ColumnIndex || !Row.IsValidIndex(*ColumnIndex))
		{
			return NAME_None;
		}

		const FString CellValue = FString(Row[*ColumnIndex]).TrimStartAndEnd();
		return CellValue.IsEmpty() ? NAME_None : FName(*CellValue);
	}

	bool CsvCellAsBool(const TArray<const TCHAR*>& Row, const TMap<FString, int32>& ColumnIndices, const TCHAR* ColumnName)
	{
		const int32* ColumnIndex = ColumnIndices.Find(FString(ColumnName));
		if (!ColumnIndex || !Row.IsValidIndex(*ColumnIndex))
		{
			return false;
		}

		const FString CellValue = FString(Row[*ColumnIndex]).TrimStartAndEnd();
		return CellValue.Equals(TEXT("true"), ESearchCase::IgnoreCase) || CellValue == TEXT("1");
	}

	float CsvCellAsDuration(const TArray<const TCHAR*>& Row, const TMap<FString, int32>& ColumnIndices, const TCHAR* ColumnName)
	{
		const int32* ColumnIndex = ColumnIndices.Find(FString(ColumnName));
		if (!ColumnIndex || !Row.IsValidIndex(*ColumnIndex)) return 0.f;
		const float Duration = FCString::Atof(Row[*ColumnIndex]);
		return FMath::IsFinite(Duration) && Duration > 0.f ? Duration : 0.f;
	}
}

ULCODialogueWidget::ULCODialogueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChoiceWidgetClass = TSoftClassPtr<ULCODialogueChoiceWidget>(FSoftObjectPath(TEXT("/Game/HJ/UI_Test/Widget_Test/WBP_DialogueChoice.WBP_DialogueChoice_C")));
	FallbackSpeakerName = NSLOCTEXT("LCODialogue", "FallbackSpeakerName", "NPC");
	WorkSeniorVoiceSettingsBySpeakerID.Add(WorkSeniorDialogueID, FLCODialogueVoiceSettings());
	WorkSeniorVoiceSettingsBySpeakerID.Add(ProtagonistSpeakerID, FLCODialogueVoiceSettings());
}

void ULCODialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
	SetChoicesVisible(false);

	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnOptionsRequested.RemoveDynamic(this, &ThisClass::HandleOptionsRequested);
		LCOPC->OnOptionsRequested.AddDynamic(this, &ThisClass::HandleOptionsRequested);
	}
}

void ULCODialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindLocalizationEvents();
	RefreshVisibleText();
}

void ULCODialogueWidget::NativeDestruct()
{
	ClearAutoAdvanceTimer();
	ClearTypewriterTimer();
	UnbindLocalizationEvents();

	RestoreGameInputMode();

	ActiveNPC.Reset();
	Interactor.Reset();

	Super::NativeDestruct();
}

FReply ULCODialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsInViewport() && (InKeyEvent.GetKey() == EKeys::F || InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::SpaceBar))
	{
		if (bAutoAdvanceActive) return FReply::Handled();
		if (bIsTypewriting)
		{
			FinishTypewriter();
		}
		else
		{
			AdvanceDialogue();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply ULCODialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsInViewport() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !ActiveNodeID.IsNone())
	{
		if (bAutoAdvanceActive) return FReply::Handled();
		if (bIsTypewriting)
		{
			FinishTypewriter();
		}
		else
		{
			AdvanceDialogue();
		}
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

bool ULCODialogueWidget::OpenDialogue(ALCONPCBase* InNPC, AActor* InInteractor, bool bPlayFirstConversation)
{
	if (!IsValid(InNPC))
	{
		return false;
	}

	if (InNPC->GetNPCId().IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] NPC %s has no DialogueID/NPCId."), *GetNameSafe(InNPC));
		return false;
	}

	const FName InitialNodeID = bPlayFirstConversation ? FirstNodeID : DefaultNodeID;
	return OpenDialogueInternal(InNPC, InInteractor, InNPC->GetNPCId(), InitialNodeID, bPlayFirstConversation);
}

bool ULCODialogueWidget::OpenDialogueByID(FName DialogueID, FName StartNodeID, AActor* InInteractor, bool bPlayFirstConversation)
{
	if (DialogueID.IsNone() || StartNodeID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] OpenDialogueByID requires DialogueID and StartNodeID (DialogueID: %s, StartNodeID: %s)"),
			*DialogueID.ToString(),
			*StartNodeID.ToString());
		return false;
	}

	// 진행 중인 대화를 덮어쓰지 않음
	if (IsDialogueActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] OpenDialogueByID ignored: dialogue '%s' is already active"), *ActiveDialogueID.ToString());
		return false;
	}

	return OpenDialogueInternal(nullptr, InInteractor, DialogueID, StartNodeID, bPlayFirstConversation);
}

bool ULCODialogueWidget::OpenDialogueInternal(ALCONPCBase* InNPC, AActor* InInteractor, FName DialogueID, FName InitialNodeID, bool bPlayFirstConversation)
{
	ActiveNPC = InNPC;
	Interactor = InInteractor;
	ActiveDialogueID = DialogueID;
	bActiveDialogueStartedAsFirstConversation = bPlayFirstConversation;
	bActiveDialogueCompletedNormally = false;
	LastVoicePlaybackTime = -FLT_MAX;

	if (!LoadFlowForDialogueID(ActiveDialogueID))
	{
		ActiveNPC.Reset();
		Interactor.Reset();
		ActiveDialogueID = NAME_None;
		bActiveDialogueStartedAsFirstConversation = false;
		return false;
	}

	if (!ActiveFlow.Nodes.Contains(InitialNodeID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Initial node '%s' is missing for DialogueID '%s': %s"),
			*InitialNodeID.ToString(),
			*ActiveDialogueID.ToString(),
			*ActiveFlow.SourcePath);
		ActiveNPC.Reset();
		Interactor.Reset();
		ActiveDialogueID = NAME_None;
		ActiveFlow = FLCODialogueFlow();
		bActiveDialogueStartedAsFirstConversation = false;
		return false;
	}

	if (IsValid(InNPC) && !bPlayFirstConversation)
	{
		// Register before playback so switching to this line never invalidates pointers into Nodes.
		FLCODialogueNode Fallback;
		Fallback.NodeID = NoQuestionsNodeID;
		Fallback.RowType = RowTypeLine;
		Fallback.TextKey = TEXT("NPC_NO_QUESTIONS");
		Fallback.bEndConversation = true;
		ActiveFlow.Nodes.Add(NoQuestionsNodeID, MoveTemp(Fallback));
	}

	if (!IsInViewport())
	{
		AddToViewport(ViewportZOrder);
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, true);
		}
	}

	ApplyDialogueInputMode();

	StartNode(InitialNodeID);
	return true;
}

void ULCODialogueWidget::CloseDialogue(bool bEndNPCConversation)
{
	if (bIsClosingDialogue)
	{
		return;
	}

	TGuardValue<bool> ClosingGuard(bIsClosingDialogue, true);

	ALCONPCBase* NPC = ActiveNPC.Get();
	const FName ClosingDialogueID = ActiveDialogueID;
	const bool bCompletedNormally = bActiveDialogueCompletedNormally;
	const bool bCompletedFirstConversation = bActiveDialogueStartedAsFirstConversation && bCompletedNormally;

	ClearAutoAdvanceTimer();
	ClearTypewriterTimer();

	RestoreGameInputMode();

	ActiveNodeID = NAME_None;
	ActiveDialogueID = NAME_None;
	FullTypewriterString.Empty();
	TypewriterVisibleLength = 0;
	bIsTypewriting = false;
	LastVoicePlaybackTime = -FLT_MAX;
	ActiveFlow = FLCODialogueFlow();
	VisibleChoiceTargets.Empty();
	bActiveDialogueStartedAsFirstConversation = false;
	bActiveDialogueCompletedNormally = false;
	SetChoicesVisible(false);

	if (IsInViewport())
	{
		RemoveFromParent();
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (ULCOInteractionPromptSubsystem* InteractionPromptSubsystem = LocalPlayer->GetSubsystem<ULCOInteractionPromptSubsystem>())
		{
			InteractionPromptSubsystem->NotifyBlockingWidgetVisibilityChanged(this, false);
		}
	}

	if (bEndNPCConversation && IsValid(NPC) && NPC->IsInConversation())
	{
		NPC->EndConversation();
	}

	ActiveNPC.Reset();
	Interactor.Reset();
	OnDialogueWidgetClosed.Broadcast(NPC, ClosingDialogueID, bCompletedNormally, bCompletedFirstConversation);
}

void ULCODialogueWidget::UnbindFromOptionsInput()
{
	if (ALCOPlayerController* LCOPC = Cast<ALCOPlayerController>(GetOwningPlayer()))
	{
		LCOPC->OnOptionsRequested.RemoveDynamic(this, &ThisClass::HandleOptionsRequested);
	}
}

void ULCODialogueWidget::HandleLanguageChanged(FName NewLanguage)
{
	RefreshVisibleText();
}

void ULCODialogueWidget::HandleOptionsRequested()
{
	if (IsInViewport())
	{
		TGuardValue<bool> ClosingFromOptionsGuard(bClosingFromOptionsRequest, true);
		CloseDialogue(true);
	}
}

void ULCODialogueWidget::HandleChoiceSelected(FName ChoiceID)
{
	const FName* TargetNodeID = VisibleChoiceTargets.Find(ChoiceID);
	if (!TargetNodeID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Choice '%s' has no target in node '%s' (%s)."),
			*ChoiceID.ToString(),
			*ActiveNodeID.ToString(),
			*ActiveFlow.SourcePath);
		return;
	}

	FName SelectedTarget = *TargetNodeID;
	const FLCODialogueNode* ChoiceNode = ActiveFlow.Nodes.Find(ActiveNodeID);
	const FLCODialogueChoiceRow* Choice = ChoiceNode ? ChoiceNode->Choices.FindByPredicate([ChoiceID](const FLCODialogueChoiceRow& Row)
	{
		return Row.ChoiceID == ChoiceID;
	}) : nullptr;
	if (Choice)
	{
		if (!Choice->AlternateCondition.IsNone() && IsConditionMet(Choice->AlternateCondition))
		{
			SelectedTarget = Choice->AlternateTargetNodeID;
		}
	}

	if (IsEndNode(SelectedTarget))
	{
		CompleteActiveConversation();
		return;
	}

	StartNode(SelectedTarget);
}

bool ULCODialogueWidget::LoadFlowForDialogueID(FName DialogueID)
{
	ActiveFlow = FLCODialogueFlow();
	VisibleChoiceTargets.Empty();

	const FString FlowPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Localization"),
		FString::Printf(TEXT("DialogueFlow_%s.csv"), *DialogueID.ToString()));
	ActiveFlow.SourcePath = FlowPath;

	FString CsvContent;
	if (!FFileHelper::LoadFileToString(CsvContent, *FlowPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Failed to load dialogue flow CSV for DialogueID '%s': %s"),
			*DialogueID.ToString(),
			*FlowPath);
		return false;
	}

	FCsvParser CsvParser(CsvContent);
	const FCsvParser::FRows& Rows = CsvParser.GetRows();
	if (Rows.Num() <= 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Dialogue flow CSV has no data rows: %s"), *FlowPath);
		return false;
	}

	TMap<FString, int32> ColumnIndices;
	for (int32 ColumnIndex = 0; ColumnIndex < Rows[0].Num(); ++ColumnIndex)
	{
		ColumnIndices.Add(FString(Rows[0][ColumnIndex]).TrimStartAndEnd(), ColumnIndex);
	}

	const TCHAR* RequiredColumns[] =
	{
		TEXT("NodeID"),
		TEXT("RowType"),
		TEXT("SpeakerID"),
		TEXT("TextKey"),
		TEXT("NextNodeID"),
		TEXT("ChoiceID"),
		TEXT("ChoiceTextKey"),
		TEXT("ChoiceTargetNodeID"),
		TEXT("Condition"),
		TEXT("EndConversation")
	};

	for (const TCHAR* RequiredColumn : RequiredColumns)
	{
		if (!ColumnIndices.Contains(FString(RequiredColumn)))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Dialogue flow CSV is missing column '%s': %s"), RequiredColumn, *FlowPath);
			return false;
		}
	}

	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& CsvRow = Rows[RowIndex];
		const FName NodeID = CsvCellAsName(CsvRow, ColumnIndices, TEXT("NodeID"));
		const FName RowType = CsvCellAsName(CsvRow, ColumnIndices, TEXT("RowType"));
		if (NodeID.IsNone() || RowType.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Skipping flow row %d with missing NodeID or RowType: %s"), RowIndex + 1, *FlowPath);
			continue;
		}

		if (RowType == RowTypeLine)
		{
			if (ActiveFlow.Nodes.Contains(NodeID))
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Duplicate node '%s' at row %d: %s. Keeping the first node."),
					*NodeID.ToString(),
					RowIndex + 1,
					*FlowPath);
				continue;
			}

			FLCODialogueNode Node;
			Node.NodeID = NodeID;
			Node.RowType = RowTypeLine;
			Node.SpeakerID = CsvCellAsName(CsvRow, ColumnIndices, TEXT("SpeakerID"));
			Node.TextKey = CsvCellAsName(CsvRow, ColumnIndices, TEXT("TextKey"));
			Node.NextNodeID = CsvCellAsName(CsvRow, ColumnIndices, TEXT("NextNodeID"));
			Node.bEndConversation = CsvCellAsBool(CsvRow, ColumnIndices, TEXT("EndConversation"));
			// 선택 열 (없는 CSV는 그대로 동작)
			Node.UnlockFlag = CsvCellAsName(CsvRow, ColumnIndices, TEXT("UnlockFlag"));
			Node.ReportKeyword = CsvCellAsName(CsvRow, ColumnIndices, TEXT("ReportKeyword"));
			Node.AutoAdvanceSeconds = CsvCellAsDuration(CsvRow, ColumnIndices, TEXT("AutoAdvanceSeconds"));
			ValidateDialogueTextKey(Node.TextKey, FString::Printf(TEXT("%s row %d"), *FlowPath, RowIndex + 1));
			ActiveFlow.Nodes.Add(NodeID, MoveTemp(Node));
		}
		else if (RowType == RowTypeChoice)
		{
			FLCODialogueNode* Node = ActiveFlow.Nodes.Find(NodeID);
			if (!Node)
			{
				FLCODialogueNode NewNode;
				NewNode.NodeID = NodeID;
				NewNode.RowType = RowTypeChoice;
				NewNode.SpeakerID = CsvCellAsName(CsvRow, ColumnIndices, TEXT("SpeakerID"));
				Node = &ActiveFlow.Nodes.Add(NodeID, MoveTemp(NewNode));
			}
			else if (Node->RowType != RowTypeChoice)
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Node '%s' mixes Line and Choice rows at row %d: %s"),
					*NodeID.ToString(),
					RowIndex + 1,
					*FlowPath);
				continue;
			}

			FLCODialogueChoiceRow Choice;
			Choice.ChoiceID = CsvCellAsName(CsvRow, ColumnIndices, TEXT("ChoiceID"));
			Choice.ChoiceTextKey = CsvCellAsName(CsvRow, ColumnIndices, TEXT("ChoiceTextKey"));
			Choice.ChoiceTargetNodeID = CsvCellAsName(CsvRow, ColumnIndices, TEXT("ChoiceTargetNodeID"));
			Choice.Condition = CsvCellAsName(CsvRow, ColumnIndices, TEXT("Condition"));
			Choice.AlternateCondition = CsvCellAsName(CsvRow, ColumnIndices, TEXT("AlternateCondition"));
			Choice.AlternateTargetNodeID = CsvCellAsName(CsvRow, ColumnIndices, TEXT("AlternateTargetNodeID"));
			Choice.bEndConversation = CsvCellAsBool(CsvRow, ColumnIndices, TEXT("EndConversation"));

			if (Choice.ChoiceID.IsNone())
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Choice node '%s' has an empty ChoiceID at row %d: %s"),
					*NodeID.ToString(),
					RowIndex + 1,
					*FlowPath);
				continue;
			}

			if (!Choice.bEndConversation && Choice.ChoiceTargetNodeID.IsNone())
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Choice '%s' in node '%s' has no ChoiceTargetNodeID and does not end conversation at row %d: %s"),
					*Choice.ChoiceID.ToString(),
					*NodeID.ToString(),
					RowIndex + 1,
					*FlowPath);
			}
			if (Choice.AlternateCondition.IsNone() != Choice.AlternateTargetNodeID.IsNone())
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Choice '%s' requires both AlternateCondition and AlternateTargetNodeID: %s"),
				*Choice.ChoiceID.ToString(), *FlowPath);
				return false;
			}

			if (Node->Choices.ContainsByPredicate([&Choice](const FLCODialogueChoiceRow& ExistingChoice)
				{
					return ExistingChoice.ChoiceID == Choice.ChoiceID;
				}))
			{
				UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Duplicate choice '%s' in node '%s' at row %d: %s"),
					*Choice.ChoiceID.ToString(),
					*NodeID.ToString(),
					RowIndex + 1,
					*FlowPath);
				continue;
			}

			ValidateDialogueTextKey(Choice.ChoiceTextKey, FString::Printf(TEXT("%s row %d"), *FlowPath, RowIndex + 1));
			Node->Choices.Add(MoveTemp(Choice));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Unknown RowType '%s' at row %d: %s"),
				*RowType.ToString(),
				RowIndex + 1,
				*FlowPath);
		}
	}

	TSet<FName> ValidatedSpeakerIDs;
	for (const TPair<FName, FLCODialogueNode>& NodePair : ActiveFlow.Nodes)
	{
		const FLCODialogueNode& Node = NodePair.Value;
		if (!ValidatedSpeakerIDs.Contains(Node.SpeakerID))
		{
			ValidateSpeakerNameKey(Node.SpeakerID, FString::Printf(TEXT("%s node '%s'"), *FlowPath, *Node.NodeID.ToString()));
			ValidatedSpeakerIDs.Add(Node.SpeakerID);
		}

		if (Node.RowType == RowTypeLine && !Node.bEndConversation && !Node.NextNodeID.IsNone() && !IsEndNode(Node.NextNodeID) && !ActiveFlow.Nodes.Contains(Node.NextNodeID))
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Line node '%s' points to missing NextNodeID '%s': %s"),
				*Node.NodeID.ToString(),
				*Node.NextNodeID.ToString(),
				*FlowPath);
		}

		if (Node.RowType == RowTypeChoice)
		{
			for (const FLCODialogueChoiceRow& Choice : Node.Choices)
			{
				if (!Choice.bEndConversation && !Choice.ChoiceTargetNodeID.IsNone() && !IsEndNode(Choice.ChoiceTargetNodeID) && !ActiveFlow.Nodes.Contains(Choice.ChoiceTargetNodeID))
				{
					UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Choice '%s' in node '%s' points to missing ChoiceTargetNodeID '%s': %s"),
						*Choice.ChoiceID.ToString(),
						*Node.NodeID.ToString(),
						*Choice.ChoiceTargetNodeID.ToString(),
						*FlowPath);
				}
				if (!Choice.AlternateTargetNodeID.IsNone() && !IsEndNode(Choice.AlternateTargetNodeID)
					&& !ActiveFlow.Nodes.Contains(Choice.AlternateTargetNodeID))
				{
					UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Choice '%s' points to missing alternate node '%s': %s"),
						*Choice.ChoiceID.ToString(), *Choice.AlternateTargetNodeID.ToString(), *FlowPath);
					return false;
				}
			}
		}
	}

	if (!ActiveFlow.Nodes.Contains(FirstNodeID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Dialogue flow has no FIRST_01 node: %s"), *FlowPath);
	}

	if (!ActiveFlow.Nodes.Contains(DefaultNodeID))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Dialogue flow has no DEFAULT_01 node: %s"), *FlowPath);
	}

	return true;
}

bool ULCODialogueWidget::ValidateSpeakerNameKey(FName SpeakerID, const FString& Context) const
{
	if (SpeakerID == SystemSpeakerID) return true;

	if (SpeakerID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Missing SpeakerID in %s"), *Context);
		return false;
	}

	const FName LocalSpeakerNameKey = FName(*(SpeakerID.ToString() + TEXT("_NAME")));
	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		const FText LocalizedName = LocalizationSubsystem->GetDialogueText(LocalSpeakerNameKey);
		if (!LocalizedName.IsEmpty() && LocalizedName.ToString() != LocalSpeakerNameKey.ToString())
		{
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] SpeakerID '%s' expects localized name key '%s', but it is missing or unresolved in %s"),
		*SpeakerID.ToString(),
		*LocalSpeakerNameKey.ToString(),
		*Context);
	return false;
}

void ULCODialogueWidget::StartNode(FName NodeID)
{
	ClearAutoAdvanceTimer();
	if (IsEndNode(NodeID))
	{
		CompleteActiveConversation();
		return;
	}

	FLCODialogueNode* Node = ActiveFlow.Nodes.Find(NodeID);
	if (!Node)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Missing node '%s' in %s"), *NodeID.ToString(), *ActiveFlow.SourcePath);
		EndActiveConversation();
		return;
	}

	if (TryStartNoQuestionsDialogue(*Node)) return;

	ActiveNodeID = NodeID;
	SetChoicesVisible(false);
	VisibleChoiceTargets.Empty();

	if (Node->RowType == RowTypeLine)
	{
		bAutoAdvanceActive = Node->AutoAdvanceSeconds > 0.f;
		ShowCurrentLine();
		if (bAutoAdvanceActive)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(AutoAdvanceTimerHandle, this,
					&ThisClass::HandleAutoAdvance, Node->AutoAdvanceSeconds, false);
			}
		}
	}
	else if (Node->RowType == RowTypeChoice)
	{
		RefreshChoices();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Node '%s' has unsupported RowType '%s' in %s"),
			*NodeID.ToString(),
			*Node->RowType.ToString(),
			*ActiveFlow.SourcePath);
		EndActiveConversation();
	}
}

void ULCODialogueWidget::ShowCurrentLine()
{
	ClearTypewriterTimer();
	SetChoicesVisible(false);

	const FLCODialogueNode* Node = ActiveFlow.Nodes.Find(ActiveNodeID);
	if (!Node || Node->RowType != RowTypeLine)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Cannot show line for node '%s' in %s"), *ActiveNodeID.ToString(), *ActiveFlow.SourcePath);
		EndActiveConversation();
		return;
	}

	RefreshSpeakerPresentation(Node->SpeakerID);
	const FText DisplayText = ReplaceDialogueKeyPlaceholders(ResolveDialogueText(Node->TextKey));

	if (bAutoAdvanceActive)
	{
		// 연결 문구는 타이핑 없이 즉시 표시한다. 언어 변경으로 진행 타이머를 다시 시작하지 않는다.
		bIsTypewriting = false;
		if (Text_Dialogue) Text_Dialogue->SetText(DisplayText);
	}
	else
	{
		StartTypewriter(DisplayText);
	}

	// 증언 획득 (조건 검사 후 수첩 증언 탭에 자동 기록, 이미 있으면 무시)
	if (!Node->UnlockFlag.IsNone())
	{
		if (ULCOStorySubsystem* StorySubsystem = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>())
		{
			StorySubsystem->TryUnlockByName(Node->UnlockFlag);
		}
	}

	if (Text_ContinueHint)
	{
		Text_ContinueHint->SetVisibility(!bAutoAdvanceActive && !ActiveNodeID.IsNone() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ULCODialogueWidget::AdvanceDialogue()
{
	if (bAutoAdvanceActive) return;
	if (bIsTypewriting)
	{
		FinishTypewriter();
		return;
	}

	if (ActiveNodeID.IsNone())
	{
		return;
	}

	const FLCODialogueNode* Node = ActiveFlow.Nodes.Find(ActiveNodeID);
	if (!Node)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Cannot advance missing node '%s' in %s"), *ActiveNodeID.ToString(), *ActiveFlow.SourcePath);
		EndActiveConversation();
		return;
	}

	if (Node->RowType == RowTypeChoice)
	{
		return;
	}

	if (Node->bEndConversation || IsEndNode(Node->NextNodeID))
	{
		CompleteActiveConversation();
		return;
	}

	if (Node->NextNodeID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Line node '%s' has no NextNodeID and does not end conversation: %s"),
			*ActiveNodeID.ToString(),
			*ActiveFlow.SourcePath);
		EndActiveConversation();
		return;
	}

	StartNode(Node->NextNodeID);
}

void ULCODialogueWidget::HandleAutoAdvance()
{
	if (!bAutoAdvanceActive || !IsInViewport()) return;
	bAutoAdvanceActive = false;
	AdvanceDialogue();
}

void ULCODialogueWidget::ClearAutoAdvanceTimer()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	bAutoAdvanceActive = false;
}

void ULCODialogueWidget::CompleteActiveConversation()
{
	const FLCODialogueNode* Node = ActiveFlow.Nodes.Find(ActiveNodeID);
	const FName ReportKeyword = ActiveDialogueID == FName(TEXT("Manager")) && Node
		? Node->ReportKeyword : NAME_None;
	const bool bHasReportEvidence = !ReportKeyword.IsNone() && IsConditionMet(ReportKeyword);
	bActiveDialogueCompletedNormally = true;
	CloseDialogue(true);

	// Finish the dialogue and commit its progress before an existing report API can travel/save.
	if (bHasReportEvidence && ReportKeyword == FName(TEXT("K05")))
	{
		if (ULCOStorySubsystem* Story = GetGameInstance()->GetSubsystem<ULCOStorySubsystem>())
		{
			if (Story->CanReportToManager(ELCOStoryFlag::K05))
			{
				Story->ReportToManager(ELCOStoryFlag::K05);
			}
		}
	}
	else if (ReportKeyword == FName(TEXT("K07")))
	{
		// The existing API blocks END 3 when K03 is owned and has no ending checkpoint UI.
		// Keep this branch as a dialogue skeleton until that external contract is updated.
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Manager K07 report is not connected: END 3/checkpoint integration required"));
	}
}

void ULCODialogueWidget::EndActiveConversation()
{
	CloseDialogue(true);
}

void ULCODialogueWidget::StartTypewriter(const FText& FullText)
{
	ClearTypewriterTimer();

	FullTypewriterString = FullText.ToString();
	TypewriterVisibleLength = 0;
	bIsTypewriting = !FullTypewriterString.IsEmpty() && TypewriterInterval > 0.f;

	if (Text_Dialogue)
	{
		Text_Dialogue->SetText(FText::GetEmpty());
	}

	if (!bIsTypewriting)
	{
		FinishTypewriter();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TypewriterTimerHandle, this, &ThisClass::RevealNextTypewriterCharacter, TypewriterInterval, true);
	}
	else
	{
		FinishTypewriter();
	}
}

void ULCODialogueWidget::RevealNextTypewriterCharacter()
{
	if (!bIsTypewriting)
	{
		return;
	}

	const int32 PreviousVisibleLength = TypewriterVisibleLength;
	TypewriterVisibleLength = GetNextTypewriterIndex();

	if (Text_Dialogue)
	{
		Text_Dialogue->SetText(FText::FromString(FullTypewriterString.Left(TypewriterVisibleLength)));
	}

	const FString RevealedCharacter = FullTypewriterString.Mid(PreviousVisibleLength, TypewriterVisibleLength - PreviousVisibleLength);
	PlayDialogueCharacterSound(RevealedCharacter);
	OnDialogueCharacterRevealed(RevealedCharacter);

	if (TypewriterVisibleLength >= FullTypewriterString.Len())
	{
		bIsTypewriting = false;
		ClearTypewriterTimer();
		HandleTypewriterCompleted();
	}
}

void ULCODialogueWidget::FinishTypewriter()
{
	const bool bShouldHandleCompletion = !ActiveNodeID.IsNone();
	ClearTypewriterTimer();
	bIsTypewriting = false;
	TypewriterVisibleLength = FullTypewriterString.Len();

	if (Text_Dialogue)
	{
		Text_Dialogue->SetText(FText::FromString(FullTypewriterString));
	}

	if (bShouldHandleCompletion)
	{
		HandleTypewriterCompleted();
	}
}

void ULCODialogueWidget::HandleTypewriterCompleted()
{
	const FLCODialogueNode* Node = ActiveFlow.Nodes.Find(ActiveNodeID);
	if (!Node || Node->bEndConversation || IsEndNode(Node->NextNodeID))
	{
		return;
	}

	const FLCODialogueNode* NextNode = ActiveFlow.Nodes.Find(Node->NextNodeID);
	if (NextNode && NextNode->RowType == RowTypeChoice)
	{
		StartNode(Node->NextNodeID);
	}
}

void ULCODialogueWidget::ClearTypewriterTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TypewriterTimerHandle);
	}
}

int32 ULCODialogueWidget::GetNextTypewriterIndex() const
{
	if (TypewriterVisibleLength >= FullTypewriterString.Len())
	{
		return FullTypewriterString.Len();
	}

	int32 NextIndex = TypewriterVisibleLength + 1;
	if (FullTypewriterString.IsValidIndex(TypewriterVisibleLength)
		&& IsHighSurrogate(FullTypewriterString[TypewriterVisibleLength])
		&& FullTypewriterString.IsValidIndex(NextIndex)
		&& IsLowSurrogate(FullTypewriterString[NextIndex]))
	{
		++NextIndex;
	}

	return FMath::Min(NextIndex, FullTypewriterString.Len());
}

bool ULCODialogueWidget::IsExitChoice(const FLCODialogueChoiceRow& Choice) const
{
	// Support the existing BYE -> BYE_01 convention and explicit end choices.
	return Choice.ChoiceID == FName(TEXT("BYE")) || Choice.bEndConversation || IsEndNode(Choice.ChoiceTargetNodeID);
}

bool ULCODialogueWidget::HasAvailableQuestions(const FLCODialogueNode& ChoiceNode) const
{
	return ChoiceNode.Choices.ContainsByPredicate([this](const FLCODialogueChoiceRow& Choice)
	{
		return !IsExitChoice(Choice) && IsConditionMet(Choice.Condition);
	});
}

bool ULCODialogueWidget::TryStartNoQuestionsDialogue(const FLCODialogueNode& Node)
{
	// Only NPC repeat conversations with an actual choice step use this fallback.
	// Phone dialogue, first conversations and ordinary line-only sequences retain their flow.
	if (!ActiveNPC.IsValid() || bActiveDialogueStartedAsFirstConversation || Node.bEndConversation) return false;
	const FLCODialogueNode* ChoiceNode = Node.RowType == RowTypeChoice ? &Node : nullptr;
	if (Node.RowType == RowTypeLine && Node.UnlockFlag.IsNone() && Node.ReportKeyword.IsNone())
	{
		const FLCODialogueNode* NextNode = ActiveFlow.Nodes.Find(Node.NextNodeID);
		if (NextNode && NextNode->RowType == RowTypeChoice) ChoiceNode = NextNode;
	}
	if (!ChoiceNode || HasAvailableQuestions(*ChoiceNode)) return false;

	// Feed a side-effect-free terminal line through the existing typewriter/normal-close path.
	FLCODialogueNode* Fallback = ActiveFlow.Nodes.Find(NoQuestionsNodeID);
	if (!Fallback) return false;
	Fallback->SpeakerID = Node.SpeakerID.IsNone() ? ActiveDialogueID : Node.SpeakerID;
	StartNode(NoQuestionsNodeID);
	return true;
}

void ULCODialogueWidget::RefreshChoices()
{
	if (!ChoicesPanel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Cannot display choices: ChoicesPanel is not bound (Dialogue=%s Node=%s Widget=%s)"),
			*ActiveDialogueID.ToString(), *ActiveNodeID.ToString(), *GetClass()->GetPathName());
		return;
	}

	ChoicesPanel->ClearChildren();
	VisibleChoiceTargets.Empty();

	const FLCODialogueNode* Node = ActiveFlow.Nodes.Find(ActiveNodeID);
	if (!Node || Node->RowType != RowTypeChoice)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Cannot show choices for node '%s' in %s"),
			*ActiveNodeID.ToString(),
			*ActiveFlow.SourcePath);
		SetChoicesVisible(false);
		return;
	}
	if (TryStartNoQuestionsDialogue(*Node)) return;

	RefreshSpeakerPresentation(Node->SpeakerID);

	TSubclassOf<ULCODialogueChoiceWidget> LoadedChoiceWidgetClass = ChoiceWidgetClass.IsNull() ? nullptr : ChoiceWidgetClass.LoadSynchronous();
	if (!LoadedChoiceWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] ChoiceWidgetClass is not configured"));
		SetChoicesVisible(false);
		return;
	}

	for (const FLCODialogueChoiceRow& Choice : Node->Choices)
	{
		if (!IsConditionMet(Choice.Condition))
		{
			continue;
		}

		ULCODialogueChoiceWidget* ChoiceWidget = CreateWidget<ULCODialogueChoiceWidget>(GetOwningPlayer(), LoadedChoiceWidgetClass);
		if (!ChoiceWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Failed to create choice '%s' using %s"),
				*Choice.ChoiceID.ToString(), *LoadedChoiceWidgetClass->GetPathName());
			continue;
		}

		ChoiceWidget->SetChoice(Choice.ChoiceID, ResolveDialogueText(Choice.ChoiceTextKey));
		ChoiceWidget->OnChoiceSelected.RemoveDynamic(this, &ThisClass::HandleChoiceSelected);
		ChoiceWidget->OnChoiceSelected.AddDynamic(this, &ThisClass::HandleChoiceSelected);
		ChoicesPanel->AddChild(ChoiceWidget);

		VisibleChoiceTargets.Add(Choice.ChoiceID, Choice.bEndConversation ? EndNodeID : Choice.ChoiceTargetNodeID);
	}

	if (VisibleChoiceTargets.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Choice node '%s' has no visible choices: %s"),
			*ActiveNodeID.ToString(),
			*ActiveFlow.SourcePath);
	}

	SetChoicesVisible(true);
	if (UsesActOneNotebookConditions())
	{
		// Re-measure newly added rows after revealing the previously collapsed choices border.
		InvalidateLayoutAndVolatility();
		ForceLayoutPrepass();
		UE_LOG(LogTemp, Log, TEXT("[LCODialogue] Choices displayed: Dialogue=%s Node=%s Configured=%d Visible=%d Children=%d Panel=%s Border=%s Class=%s"),
			*ActiveDialogueID.ToString(), *ActiveNodeID.ToString(), Node->Choices.Num(), VisibleChoiceTargets.Num(),
			ChoicesPanel->GetChildrenCount(), *GetNameSafe(ChoicesPanel), *GetNameSafe(Border_Choices), *LoadedChoiceWidgetClass->GetPathName());
	}
	if (Text_ContinueHint)
	{
		Text_ContinueHint->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ULCODialogueWidget::RefreshVisibleText()
{
	if (!IsInViewport())
	{
		return;
	}

	const FLCODialogueNode* Node = ActiveFlow.Nodes.Find(ActiveNodeID);
	if (Node && Node->RowType == RowTypeLine)
	{
		ShowCurrentLine();
	}
	else if (Node && Node->RowType == RowTypeChoice)
	{
		RefreshChoices();
	}
}

void ULCODialogueWidget::RefreshSpeakerPresentation(FName SpeakerID)
{
	if (SpeakerID == SystemSpeakerID)
	{
		if (Text_SpeakerName) Text_SpeakerName->SetText(FText::GetEmpty());
		if (Img_SpeakerPortrait)
		{
			Img_SpeakerPortrait->SetBrushFromTexture(nullptr, false);
			Img_SpeakerPortrait->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (Text_SpeakerName)
	{
		Text_SpeakerName->SetText(ResolveSpeakerName(SpeakerID));
	}
	if (!Img_SpeakerPortrait)
	{
		return;
	}

	UTexture2D* Portrait = DefaultSpeakerPortrait.Get();
	if (const TObjectPtr<UTexture2D>* SpeakerPortrait = SpeakerPortraitsBySpeakerID.Find(SpeakerID))
	{
		if (IsValid(SpeakerPortrait->Get())) Portrait = SpeakerPortrait->Get();
	}
	if (IsValid(Portrait))
	{
		// 디자이너에서 설정한 이미지 크기를 유지한다.
		Img_SpeakerPortrait->SetBrushFromTexture(Portrait, false);
		Img_SpeakerPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// 이전 화자의 이미지가 남지 않도록 브러시도 비운다.
		Img_SpeakerPortrait->SetBrushFromTexture(nullptr, false);
		Img_SpeakerPortrait->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ULCODialogueWidget::SetChoicesVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (Border_Choices)
	{
		Border_Choices->SetVisibility(NewVisibility);
	}
	if (ChoicesPanel)
	{
		ChoicesPanel->SetVisibility(NewVisibility);
	}
}

bool ULCODialogueWidget::IsEndNode(FName NodeID) const
{
	return NodeID == EndNodeID;
}

bool ULCODialogueWidget::IsConditionMet(FName Condition) const
{
	if (Condition.IsNone())
	{
		return true;
	}

	if (UsesActOneNotebookConditions())
	{
		// T/K entries come from the persistent notebook used by deduction and save/restore.
		if (const ULCONotebookSubsystem* Notebook = GetGameInstance()->GetSubsystem<ULCONotebookSubsystem>())
		{
			if (Notebook->HasEntry(Condition)) return true;
		}
		// Evidence can also be physically held. Do not treat inventory items as testimony/deductions.
		return Condition.ToString().StartsWith(TEXT("E")) && HasEvidenceToken(Condition);
	}
	return HasEvidenceToken(Condition);
}

bool ULCODialogueWidget::UsesActOneNotebookConditions() const
{
	static const TSet<FName> DialogueIDs =
	{
		TEXT("Bellboy"), TEXT("Caretaker"), TEXT("Cleaner"),
		TEXT("FrontDesk"), TEXT("Manager"), TEXT("OldProfessor")
	};
	// Preserve the existing SigolYouth and WorkSenior condition policies.
	return DialogueIDs.Contains(ActiveDialogueID);
}

bool ULCODialogueWidget::ValidateDialogueTextKey(FName TextKey, const FString& Context) const
{
	if (TextKey.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Missing text key in %s"), *Context);
		return false;
	}

	const FText LocalizedText = ResolveDialogueText(TextKey);
	if (LocalizedText.IsEmpty() || LocalizedText.ToString() == TextKey.ToString())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Text key '%s' is missing or unresolved in %s"),
			*TextKey.ToString(),
			*Context);
		return false;
	}

	return true;
}

void ULCODialogueWidget::PlayDialogueCharacterSound(const FString& RevealedCharacter)
{
	if (!ShouldPlayDialogueCharacterSound(RevealedCharacter))
	{
		return;
	}

	const FLCODialogueVoiceSettings* VoiceSettings = GetActiveVoiceSettings();
	if (!VoiceSettings || !VoiceSettings->Sound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastVoicePlaybackTime < VoiceSettings->MinPlaybackInterval)
	{
		return;
	}

	const float PitchMin = FMath::Min(VoiceSettings->PitchRandomMin, VoiceSettings->PitchRandomMax);
	const float PitchMax = FMath::Max(VoiceSettings->PitchRandomMin, VoiceSettings->PitchRandomMax);
	const float PitchMultiplier = VoiceSettings->BasePitch * FMath::FRandRange(PitchMin, PitchMax);

	UGameplayStatics::PlaySound2D(this, VoiceSettings->Sound, VoiceSettings->VolumeMultiplier, PitchMultiplier);
	LastVoicePlaybackTime = CurrentTime;
}

bool ULCODialogueWidget::ShouldPlayDialogueCharacterSound(const FString& RevealedCharacter) const
{
	if (RevealedCharacter.IsEmpty() || ActiveDialogueID.IsNone())
	{
		return false;
	}

	for (int32 Index = 0; Index < RevealedCharacter.Len(); ++Index)
	{
		if (!IsDialogueSilentCharacter(RevealedCharacter[Index]))
		{
			return true;
		}
	}

	return false;
}

const FLCODialogueVoiceSettings* ULCODialogueWidget::GetActiveVoiceSettings() const
{
	if (ActiveDialogueID == WorkSeniorDialogueID)
	{
		const FLCODialogueNode* Node = ActiveFlow.Nodes.Find(ActiveNodeID);
		if (!Node || Node->RowType != RowTypeLine || Node->SpeakerID == SystemSpeakerID
			|| Node->AutoAdvanceSeconds > 0.f)
		{
			return nullptr;
		}

		// Keep phone voices separate from the existing per-dialogue NPC settings.
		return WorkSeniorVoiceSettingsBySpeakerID.Find(Node->SpeakerID);
	}

	return VoiceSettingsByDialogueID.Find(ActiveDialogueID);
}

void ULCODialogueWidget::BindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
		LocalizationSubsystem->OnLanguageChanged.AddDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCODialogueWidget::UnbindLocalizationEvents()
{
	if (ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		LocalizationSubsystem->OnLanguageChanged.RemoveDynamic(this, &ThisClass::HandleLanguageChanged);
	}
}

void ULCODialogueWidget::ApplyDialogueInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (bDialogueInputModeApplied && InputModePlayerController.Get() != PC)
	{
		RestoreGameInputMode();
	}
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetWidgetToFocus(TakeWidget());

	if (!bDialogueInputModeApplied)
	{
		InputModePlayerController = PC;
		bPreviousShowMouseCursor = PC->bShowMouseCursor;

		// 입력 차단은 누적된다. 기존 차단과 별개로 한 번 추가하고 종료 시 한 번만 해제한다.
		PC->SetIgnoreMoveInput(true);
		bMoveInputBlockedByThisWidget = true;
		PC->SetIgnoreLookInput(true);
		bLookInputBlockedByThisWidget = true;
	}

	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	bDialogueInputModeApplied = true;
}

void ULCODialogueWidget::RestoreGameInputMode()
{
	// OwningPlayer가 바뀌어도 실제 차단을 적용한 컨트롤러에서 해제한다.
	if (APlayerController* PC = InputModePlayerController.Get())
	{
		if (bMoveInputBlockedByThisWidget) PC->SetIgnoreMoveInput(false);
		if (bLookInputBlockedByThisWidget) PC->SetIgnoreLookInput(false);

		// 커서/모드 복구 옵션과 무관하게 위의 이동/시점 차단은 항상 정리한다.
		if (bDialogueInputModeApplied && bRestoreGameInputOnClose
			&& !(bClosingFromOptionsRequest && UGameplayStatics::IsGamePaused(this))
			&& ShouldRestoreGameOnlyInputMode(PC))
		{
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
			PC->SetShowMouseCursor(bPreviousShowMouseCursor);
		}
	}

	bMoveInputBlockedByThisWidget = false;
	bLookInputBlockedByThisWidget = false;
	InputModePlayerController.Reset();
	bDialogueInputModeApplied = false;
}

bool ULCODialogueWidget::ShouldRestoreGameOnlyInputMode(APlayerController* PC) const
{
	if (!PC || bPreviousShowMouseCursor)
	{
		return false;
	}

	return HasUserFocus(PC) || HasUserFocusedDescendants(PC);
}

bool ULCODialogueWidget::HasEvidenceToken(FName EvidenceID) const
{
	const AActor* InteractorActor = Interactor.Get();
	if (!IsValid(InteractorActor))
	{
		return false;
	}

	if (const UInventoryComponent* Inventory = InteractorActor->FindComponentByClass<UInventoryComponent>())
	{
		if (Inventory->HasItem(EvidenceID))
		{
			return true;
		}
	}

	if (const UNotebookComponent* Notebook = InteractorActor->FindComponentByClass<UNotebookComponent>())
	{
		return Notebook->HasEvidence(EvidenceID);
	}

	return false;
}

FText ULCODialogueWidget::ResolveDialogueText(FName Key) const
{
	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		return LocalizationSubsystem->GetDialogueText(Key);
	}

	return FText::FromName(Key);
}

FText ULCODialogueWidget::ReplaceDialogueKeyPlaceholders(const FText& LocalizedText) const
{
	struct FKeyPlaceholder
	{
		const TCHAR* Token;
		FName BindingName;
	};
	const FKeyPlaceholder Placeholders[] =
	{
		{ TEXT("{퀵슬롯키}"), FName(TEXT("QuickSlot")) },
		{ TEXT("{능력키}"), FName(TEXT("Sense")) },
		{ TEXT("{수첩키}"), FName(TEXT("Notebook")) }
	};
	FString DisplayString = LocalizedText.ToString();
	bool bReplaced = false;
	for (const FKeyPlaceholder& Placeholder : Placeholders)
	{
		if (DisplayString.Contains(Placeholder.Token, ESearchCase::CaseSensitive))
		{
			DisplayString.ReplaceInline(Placeholder.Token, *ResolveDialogueKeyBinding(Placeholder.BindingName), ESearchCase::CaseSensitive);
			bReplaced = true;
		}
	}
	// Preserve the original FText, including its localization history, when no token is present.
	return bReplaced ? FText::FromString(DisplayString) : LocalizedText;
}

FString ULCODialogueWidget::ResolveDialogueKeyBinding(FName BindingName) const
{
	// Future Enhanced Input lookup belongs here, using the owning local player's mappings.
	// Keep these defaults as the fallback when no runtime mapping is available.
	if (BindingName == FName(TEXT("QuickSlot"))) return TEXT("Q");
	if (BindingName == FName(TEXT("Sense"))) return TEXT("Tab");
	if (BindingName == FName(TEXT("Notebook"))) return TEXT("J");
	return BindingName.ToString();
}

FText ULCODialogueWidget::ResolveSpeakerName(FName SpeakerID) const
{
	const FName LocalSpeakerNameKey = SpeakerID.IsNone() ? NAME_None : FName(*(SpeakerID.ToString() + TEXT("_NAME")));
	if (const ULCOLocalizationSubsystem* LocalizationSubsystem = GetGameInstance()->GetSubsystem<ULCOLocalizationSubsystem>())
	{
		const FText LocalizedName = LocalizationSubsystem->GetDialogueText(LocalSpeakerNameKey);
		if (!LocalSpeakerNameKey.IsNone() && !LocalizedName.IsEmpty() && LocalizedName.ToString() != LocalSpeakerNameKey.ToString())
		{
			return LocalizedName;
		}
	}

	return FallbackSpeakerName;
}
