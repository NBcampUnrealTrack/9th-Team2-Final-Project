#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LCOLocalizationSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLCOLanguageChanged, FName, NewLanguage);

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
	FText GetItemText(FName ItemID) const;

	UFUNCTION(BlueprintCallable, Category = "LCO|Localization")
	void SetLanguage(FName LanguageCode);

	UFUNCTION(BlueprintCallable, Category = "LCO|Localization")
	FName GetLanguage() const { return CurrentLanguage; }

	UPROPERTY(BlueprintAssignable, Category = "LCO|Localization")
	FOnLCOLanguageChanged OnLanguageChanged;

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
	void LoadDialogueCsvFiles();
	void LoadCsvFile(const FString& RelativePath, TMap<FName, FLCOLocalizedTextRow>& OutRows);
	void LoadCsvFileByFullPath(const FString& FullPath, TMap<FName, FLCOLocalizedTextRow>& OutRows, TMap<FName, FString>* LoadedKeySources = nullptr);
	FText GetLocalizedText(const TMap<FName, FLCOLocalizedTextRow>& Rows, FName Key) const;
	FText GetLocalizedText(const TMap<FName, FLCOLocalizedTextRow>& Rows, FName Key, bool bFallbackToKey) const;
	bool IsSupportedLanguage(FName LanguageCode) const;
	bool LoadUtf8FileToString(const FString& FullPath, FString& OutContent) const;

	UPROPERTY()
	FName CurrentLanguage = TEXT("ko");

	TMap<FName, FLCOLocalizedTextRow> UITextRows;
	TMap<FName, FLCOLocalizedTextRow> DialogueTextRows;
	TMap<FName, FLCOLocalizedTextRow> ItemTextRows;
};
