#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOLocalizationSubsystem.generated.h"

struct FLCOLocalizedTextRow
{
	TMap<FName, FString> TextByLanguage;
};

UCLASS()
class LASTCHECKOUT_API ULCOLocalizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "LCO|Localization")
	FText GetUIText(FName Key) const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Localization")
	FText GetDialogueText(FName Key) const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Localization")
	void SetLanguage(FName LanguageCode);

	UFUNCTION(BlueprintCallable, Category = "LCO|Localization")
	FName GetLanguage() const { return CurrentLanguage; }

private:
	static const FName KoreanLanguage;
	static const FName EnglishLanguage;
	static const FName JapaneseLanguage;
	static const FName SimplifiedChineseLanguage;
	static const FName TraditionalChineseLanguage;
	static const FName SpanishLanguage;
	static const FName FrenchLanguage;
	static const FName GermanLanguage;
	static const FName BrazilianPortugueseLanguage;
	static const FName FinnishLanguage;

	void LoadLocalizationFiles();
	void LoadCsvFile(const FString& RelativePath, TMap<FName, FLCOLocalizedTextRow>& OutRows);
	FText GetLocalizedText(const TMap<FName, FLCOLocalizedTextRow>& Rows, FName Key) const;
	bool IsSupportedLanguage(FName LanguageCode) const;
	bool LoadUtf8FileToString(const FString& FullPath, FString& OutContent) const;

	UPROPERTY()
	FName CurrentLanguage = TEXT("ko");

	TMap<FName, FLCOLocalizedTextRow> UITextRows;
	TMap<FName, FLCOLocalizedTextRow> DialogueTextRows;
};
