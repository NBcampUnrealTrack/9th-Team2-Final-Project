#include "Localization/LCOLocalizationSubsystem.h"

#include "Containers/StringConv.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

const FName ULCOLocalizationSubsystem::KoreanLanguage = TEXT("ko");
const FName ULCOLocalizationSubsystem::EnglishLanguage = TEXT("en");
const FName ULCOLocalizationSubsystem::JapaneseLanguage = TEXT("ja");
const FName ULCOLocalizationSubsystem::SimplifiedChineseLanguage = TEXT("zh-CN");
const FName ULCOLocalizationSubsystem::TraditionalChineseLanguage = TEXT("zh-TW");
const FName ULCOLocalizationSubsystem::SpanishLanguage = TEXT("es");
const FName ULCOLocalizationSubsystem::FrenchLanguage = TEXT("fr");
const FName ULCOLocalizationSubsystem::GermanLanguage = TEXT("de");
const FName ULCOLocalizationSubsystem::BrazilianPortugueseLanguage = TEXT("pt-BR");
const FName ULCOLocalizationSubsystem::FinnishLanguage = TEXT("fi");

void ULCOLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentLanguage = KoreanLanguage;
	LoadLocalizationFiles();
}

FText ULCOLocalizationSubsystem::GetUIText(FName Key) const
{
	return GetLocalizedText(UITextRows, Key);
}

FText ULCOLocalizationSubsystem::GetDialogueText(FName Key) const
{
	return GetLocalizedText(DialogueTextRows, Key);
}

FText ULCOLocalizationSubsystem::GetItemText(FName ItemID) const
{
	return GetLocalizedText(ItemTextRows, ItemID, false);
}

void ULCOLocalizationSubsystem::SetLanguage(FName LanguageCode)
{
	if (!IsSupportedLanguage(LanguageCode))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOLocalization] Unsupported language: %s"), *LanguageCode.ToString());
		return;
	}

	if (CurrentLanguage == LanguageCode)
	{
		return;
	}

	CurrentLanguage = LanguageCode;
	OnLanguageChanged.Broadcast(CurrentLanguage);
}

void ULCOLocalizationSubsystem::LoadLocalizationFiles()
{
	UITextRows.Empty();
	DialogueTextRows.Empty();
	ItemTextRows.Empty();

	LoadCsvFile(TEXT("Localization/UI.csv"), UITextRows);
	LoadDialogueCsvFiles();
	LoadCsvFile(TEXT("Localization/Items.csv"), ItemTextRows);
}

void ULCOLocalizationSubsystem::LoadDialogueCsvFiles()
{
	TMap<FName, FString> LoadedKeySources;

	const FString LocalizationDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Localization"));
	const FString LegacyDialogueCsvPath = FPaths::Combine(LocalizationDir, TEXT("Dialogue.csv"));
	if (IFileManager::Get().FileExists(*LegacyDialogueCsvPath))
	{
		LoadCsvFileByFullPath(LegacyDialogueCsvPath, DialogueTextRows, &LoadedKeySources);
	}

	TArray<FString> DialogueCsvFiles;
	IFileManager::Get().FindFiles(DialogueCsvFiles, *FPaths::Combine(LocalizationDir, TEXT("Dialogue_*.csv")), true, false);
	DialogueCsvFiles.Sort();

	for (const FString& DialogueCsvFile : DialogueCsvFiles)
	{
		const FString DialogueCsvPath = FPaths::IsRelative(DialogueCsvFile)
			? FPaths::Combine(LocalizationDir, DialogueCsvFile)
			: DialogueCsvFile;
		LoadCsvFileByFullPath(DialogueCsvPath, DialogueTextRows, &LoadedKeySources);
	}
}

void ULCOLocalizationSubsystem::LoadCsvFile(const FString& RelativePath, TMap<FName, FLCOLocalizedTextRow>& OutRows)
{
	const FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativePath);
	LoadCsvFileByFullPath(FullPath, OutRows);
}

void ULCOLocalizationSubsystem::LoadCsvFileByFullPath(const FString& FullPath, TMap<FName, FLCOLocalizedTextRow>& OutRows, TMap<FName, FString>* LoadedKeySources)
{
	FString CsvContent;
	if (!LoadUtf8FileToString(FullPath, CsvContent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOLocalization] Failed to load CSV: %s"), *FullPath);
		return;
	}

	FCsvParser CsvParser(CsvContent);
	const FCsvParser::FRows& Rows = CsvParser.GetRows();
	if (Rows.Num() <= 1)
	{
		return;
	}

	TMap<FString, int32> ColumnIndices;
	for (int32 ColumnIndex = 0; ColumnIndex < Rows[0].Num(); ++ColumnIndex)
	{
		ColumnIndices.Add(FString(Rows[0][ColumnIndex]).TrimStartAndEnd(), ColumnIndex);
	}

	const int32* KeyColumn = ColumnIndices.Find(TEXT("Key"));
	if (!KeyColumn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LCOLocalization] CSV has no Key column: %s"), *FullPath);
		return;
	}

	const FName SupportedLanguages[] =
	{
		KoreanLanguage,
		EnglishLanguage,
		JapaneseLanguage,
		SimplifiedChineseLanguage,
		TraditionalChineseLanguage,
		SpanishLanguage,
		FrenchLanguage,
		GermanLanguage,
		BrazilianPortugueseLanguage,
		FinnishLanguage
	};

	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		const TArray<const TCHAR*>& CsvRow = Rows[RowIndex];
		if (!CsvRow.IsValidIndex(*KeyColumn))
		{
			continue;
		}

		const FString KeyString = FString(CsvRow[*KeyColumn]).TrimStartAndEnd();
		if (KeyString.IsEmpty())
		{
			continue;
		}

		FLCOLocalizedTextRow LocalizedRow;
		for (const FName Language : SupportedLanguages)
		{
			const int32* LanguageColumn = ColumnIndices.Find(Language.ToString());
			if (LanguageColumn && CsvRow.IsValidIndex(*LanguageColumn))
			{
				LocalizedRow.TextByLanguage.Add(Language, FString(CsvRow[*LanguageColumn]));
			}
		}

		const FName KeyName(*KeyString);
		if (FLCOLocalizedTextRow* ExistingRow = OutRows.Find(KeyName))
		{
			if (LoadedKeySources)
			{
				const FString* ExistingSource = LoadedKeySources->Find(KeyName);
				UE_LOG(LogTemp, Warning, TEXT("[LCOLocalization] Duplicate dialogue key '%s' in CSV: %s (previous: %s). Keeping the first value."),
					*KeyString,
					*FullPath,
					ExistingSource ? **ExistingSource : TEXT("unknown"));
				continue;
			}

			*ExistingRow = MoveTemp(LocalizedRow);
			continue;
		}

		OutRows.Add(KeyName, MoveTemp(LocalizedRow));
		if (LoadedKeySources)
		{
			LoadedKeySources->Add(KeyName, FullPath);
		}
	}
}

FText ULCOLocalizationSubsystem::GetLocalizedText(const TMap<FName, FLCOLocalizedTextRow>& Rows, FName Key) const
{
	return GetLocalizedText(Rows, Key, true);
}

FText ULCOLocalizationSubsystem::GetLocalizedText(const TMap<FName, FLCOLocalizedTextRow>& Rows, FName Key, bool bFallbackToKey) const
{
	if (Key.IsNone())
	{
		return FText::GetEmpty();
	}

	const FLCOLocalizedTextRow* Row = Rows.Find(Key);
	if (!Row)
	{
		return bFallbackToKey ? FText::FromName(Key) : FText::GetEmpty();
	}

	const FString* LocalizedString = Row->TextByLanguage.Find(CurrentLanguage);
	if (LocalizedString && !LocalizedString->IsEmpty())
	{
		return FText::FromString(*LocalizedString);
	}

	const FString* FallbackString = Row->TextByLanguage.Find(KoreanLanguage);
	if (FallbackString && !FallbackString->IsEmpty())
	{
		return FText::FromString(*FallbackString);
	}

	return bFallbackToKey ? FText::FromName(Key) : FText::GetEmpty();
}

bool ULCOLocalizationSubsystem::IsSupportedLanguage(FName LanguageCode) const
{
	return LanguageCode == KoreanLanguage
		|| LanguageCode == EnglishLanguage
		|| LanguageCode == JapaneseLanguage
		|| LanguageCode == SimplifiedChineseLanguage
		|| LanguageCode == TraditionalChineseLanguage
		|| LanguageCode == SpanishLanguage
		|| LanguageCode == FrenchLanguage
		|| LanguageCode == GermanLanguage
		|| LanguageCode == BrazilianPortugueseLanguage
		|| LanguageCode == FinnishLanguage;
}

bool ULCOLocalizationSubsystem::LoadUtf8FileToString(const FString& FullPath, FString& OutContent) const
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FullPath))
	{
		return false;
	}

	if (FileData.Num() >= 3 && FileData[0] == 0xEF && FileData[1] == 0xBB && FileData[2] == 0xBF)
	{
		FileData.RemoveAt(0, 3, EAllowShrinking::No);
	}

	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(FileData.GetData()), FileData.Num());
	OutContent = FString(Converter.Length(), Converter.Get());
	return true;
}
