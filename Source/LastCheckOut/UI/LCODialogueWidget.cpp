#include "UI/LCODialogueWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Inventory/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Localization/LCOLocalizationSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Notebook/NotebookComponent.h"
#include "NPC/LCONPCBase.h"
#include "Serialization/Csv/CsvParser.h"
#include "TimerManager.h"
#include "UI/LCODialogueChoiceWidget.h"
#include "UI/LCOInteractionPromptSubsystem.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FName RowTypeLine = TEXT("Line");
	const FName RowTypeChoice = TEXT("Choice");
	const FName FirstNodeID = TEXT("FIRST_01");
	const FName DefaultNodeID = TEXT("DEFAULT_01");
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
}

ULCODialogueWidget::ULCODialogueWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ChoiceWidgetClass = TSoftClassPtr<ULCODialogueChoiceWidget>(FSoftObjectPath(TEXT("/Game/HJ/UI_Test/Widget_Test/WBP_DialogueChoice.WBP_DialogueChoice_C")));
	FallbackSpeakerName = NSLOCTEXT("LCODialogue", "FallbackSpeakerName", "NPC");
}

void ULCODialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
	SetChoicesVisible(false);
}

void ULCODialogueWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindLocalizationEvents();
	RefreshVisibleText();
}

void ULCODialogueWidget::NativeDestruct()
{
	ClearTypewriterTimer();
	UnbindLocalizationEvents();

	if (bDialogueInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	ActiveNPC.Reset();
	Interactor.Reset();

	Super::NativeDestruct();
}

FReply ULCODialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsInViewport() && (InKeyEvent.GetKey() == EKeys::F || InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::SpaceBar))
	{
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

	if (IsInViewport() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDialogue(true);
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply ULCODialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsInViewport() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !ActiveNodeID.IsNone())
	{
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

	ActiveNPC = InNPC;
	Interactor = InInteractor;
	ActiveDialogueID = InNPC->GetNPCId();
	LastVoicePlaybackTime = -FLT_MAX;

	if (ActiveDialogueID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] NPC %s has no DialogueID/NPCId."), *GetNameSafe(InNPC));
		ActiveNPC.Reset();
		Interactor.Reset();
		return false;
	}

	if (!LoadFlowForDialogueID(ActiveDialogueID))
	{
		ActiveNPC.Reset();
		Interactor.Reset();
		ActiveDialogueID = NAME_None;
		return false;
	}

	const FName InitialNodeID = bPlayFirstConversation ? FirstNodeID : DefaultNodeID;
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
		return false;
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
	ALCONPCBase* NPC = ActiveNPC.Get();

	ClearTypewriterTimer();

	if (bDialogueInputModeApplied && bRestoreGameInputOnClose)
	{
		RestoreGameInputMode();
	}

	ActiveNodeID = NAME_None;
	ActiveDialogueID = NAME_None;
	FullTypewriterString.Empty();
	TypewriterVisibleLength = 0;
	bIsTypewriting = false;
	LastVoicePlaybackTime = -FLT_MAX;
	ActiveFlow = FLCODialogueFlow();
	VisibleChoiceTargets.Empty();
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
	OnDialogueWidgetClosed.Broadcast(NPC);
}

void ULCODialogueWidget::HandleLanguageChanged(FName NewLanguage)
{
	RefreshVisibleText();
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

	if (IsEndNode(*TargetNodeID))
	{
		EndActiveConversation();
		return;
	}

	StartNode(*TargetNodeID);
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

	for (const TPair<FName, FLCODialogueNode>& NodePair : ActiveFlow.Nodes)
	{
		const FLCODialogueNode& Node = NodePair.Value;
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

void ULCODialogueWidget::StartNode(FName NodeID)
{
	if (IsEndNode(NodeID))
	{
		EndActiveConversation();
		return;
	}

	FLCODialogueNode* Node = ActiveFlow.Nodes.Find(NodeID);
	if (!Node)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCODialogue] Missing node '%s' in %s"), *NodeID.ToString(), *ActiveFlow.SourcePath);
		EndActiveConversation();
		return;
	}

	ActiveNodeID = NodeID;
	SetChoicesVisible(false);
	VisibleChoiceTargets.Empty();

	if (Node->RowType == RowTypeLine)
	{
		ShowCurrentLine();
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

	if (Text_SpeakerName)
	{
		Text_SpeakerName->SetText(ResolveSpeakerName(Node->SpeakerID));
	}

	StartTypewriter(ResolveDialogueText(Node->TextKey));

	if (Text_ContinueHint)
	{
		Text_ContinueHint->SetVisibility(!ActiveNodeID.IsNone() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void ULCODialogueWidget::AdvanceDialogue()
{
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
		EndActiveConversation();
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

void ULCODialogueWidget::RefreshChoices()
{
	if (!ChoicesPanel)
	{
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
		if (Text_SpeakerName)
		{
			Text_SpeakerName->SetText(ResolveSpeakerName(Node->SpeakerID));
		}
		RefreshChoices();
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

	return HasEvidenceToken(Condition);
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
	if (!PC)
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetWidgetToFocus(TakeWidget());

	if (!bDialogueInputModeApplied)
	{
		bPreviousShowMouseCursor = PC->bShowMouseCursor;

		if (bBlockLookInputWhileOpen && !PC->IsLookInputIgnored())
		{
			PC->SetIgnoreLookInput(true);
			bLookInputBlockedByThisWidget = true;
		}
	}

	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
	bDialogueInputModeApplied = true;
}

void ULCODialogueWidget::RestoreGameInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	if (bLookInputBlockedByThisWidget)
	{
		PC->SetIgnoreLookInput(false);
		bLookInputBlockedByThisWidget = false;
	}

	if (ShouldRestoreGameOnlyInputMode(PC))
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
		PC->SetShowMouseCursor(bPreviousShowMouseCursor);
	}

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
