#include "Core/LCOGameSettingsSubsystem.h"
#include "LastCheckOut.h"
#include "Core/LCOGameSettingsSaveGame.h"
#include "Player/LCOPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

const FString ULCOGameSettingsSubsystem::SettingsSlotName(TEXT("LCO_Settings"));

void ULCOGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadSettings();
	ApplyBrightness();
}

// ---------- 그래픽 ----------

UGameUserSettings* ULCOGameSettingsSubsystem::GetEngineSettings()
{
	return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

EWindowMode::Type ULCOGameSettingsSubsystem::GetWindowMode() const
{
	const UGameUserSettings* EngineSettings = GetEngineSettings();
	return EngineSettings ? EngineSettings->GetFullscreenMode() : EWindowMode::Windowed;
}

void ULCOGameSettingsSubsystem::SetWindowMode(EWindowMode::Type Mode)
{
	if (UGameUserSettings* EngineSettings = GetEngineSettings())
	{
		EngineSettings->SetFullscreenMode(Mode);
	}
}

TArray<FIntPoint> ULCOGameSettingsSubsystem::GetSupportedResolutions() const
{
	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	return Resolutions;
}

FIntPoint ULCOGameSettingsSubsystem::GetResolution() const
{
	const UGameUserSettings* EngineSettings = GetEngineSettings();
	return EngineSettings ? EngineSettings->GetScreenResolution() : FIntPoint::ZeroValue;
}

void ULCOGameSettingsSubsystem::SetResolution(FIntPoint Resolution)
{
	if (UGameUserSettings* EngineSettings = GetEngineSettings())
	{
		EngineSettings->SetScreenResolution(Resolution);
	}
}

int32 ULCOGameSettingsSubsystem::GetOverallQuality() const
{
	const UGameUserSettings* EngineSettings = GetEngineSettings();
	return EngineSettings ? EngineSettings->GetOverallScalabilityLevel() : 3;
}

void ULCOGameSettingsSubsystem::SetOverallQuality(int32 Level)
{
	if (UGameUserSettings* EngineSettings = GetEngineSettings())
	{
		EngineSettings->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 3));
	}
}

void ULCOGameSettingsSubsystem::ApplyGraphicsSettings()
{
	UGameUserSettings* EngineSettings = GetEngineSettings();
	if (!EngineSettings)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameSettings] ApplyGraphicsSettings: GameUserSettings not found"));
		return;
	}

	// 적용 + GameUserSettings.ini 저장 (명령줄 옵션은 무시)
	EngineSettings->ApplySettings(false);

	// Log Test
	const FIntPoint Resolution = EngineSettings->GetScreenResolution();
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] ApplyGraphicsSettings: %dx%d, Mode = %d, Quality = %d"),
		Resolution.X, Resolution.Y, static_cast<int32>(EngineSettings->GetFullscreenMode()), EngineSettings->GetOverallScalabilityLevel());

	OnSettingsChanged.Broadcast();
}

void ULCOGameSettingsSubsystem::RevertGraphicsSettings()
{
	UGameUserSettings* EngineSettings = GetEngineSettings();
	if (!EngineSettings)
	{
		return;
	}

	// 아직 적용하지 않은 변경을 버리고 ini에 저장된 값으로 되돌림
	EngineSettings->LoadSettings(true);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] RevertGraphicsSettings"));

	OnSettingsChanged.Broadcast();
}

// ---------- 밝기 ----------

float ULCOGameSettingsSubsystem::GetBrightness() const
{
	return Settings ? Settings->Brightness : 2.2f;
}

void ULCOGameSettingsSubsystem::SetBrightness(float Value)
{
	if (!Settings)
	{
		return;
	}

	Settings->Brightness = FMath::Clamp(Value, MinBrightness, MaxBrightness);
	ApplyBrightness();
	SaveSettings();

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] SetBrightness: %.2f"), Settings->Brightness);

	OnSettingsChanged.Broadcast();
}

void ULCOGameSettingsSubsystem::ApplyBrightness() const
{
	if (GEngine && Settings)
	{
		GEngine->DisplayGamma = Settings->Brightness;
	}
}

// ---------- 게임플레이 ----------

float ULCOGameSettingsSubsystem::GetLookSensitivity() const
{
	return Settings ? Settings->LookSensitivity : 1.f;
}

void ULCOGameSettingsSubsystem::SetLookSensitivity(float Value)
{
	if (!Settings)
	{
		return;
	}

	Settings->LookSensitivity = FMath::Clamp(Value, MinLookSensitivity, MaxLookSensitivity);
	SaveSettings();

	// 지금 플레이 중이면 바로 적용
	if (ALCOPlayerCharacter* Player = GetCurrentPlayer())
	{
		Player->SetLookSensitivity(Settings->LookSensitivity);
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] SetLookSensitivity: %.2f"), Settings->LookSensitivity);

	OnSettingsChanged.Broadcast();
}

bool ULCOGameSettingsSubsystem::IsInvertLookY() const
{
	return Settings ? Settings->bInvertLookY : false;
}

void ULCOGameSettingsSubsystem::SetInvertLookY(bool bInvert)
{
	if (!Settings)
	{
		return;
	}

	Settings->bInvertLookY = bInvert;
	SaveSettings();

	// 지금 플레이 중이면 바로 적용
	if (ALCOPlayerCharacter* Player = GetCurrentPlayer())
	{
		Player->SetInvertLookY(Settings->bInvertLookY);
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] SetInvertLookY: %s"), bInvert ? TEXT("true") : TEXT("false"));

	OnSettingsChanged.Broadcast();
}

void ULCOGameSettingsSubsystem::ApplyGameplaySettingsToPlayer(ALCOPlayerCharacter* Player) const
{
	if (!Player || !Settings)
	{
		return;
	}

	Player->SetLookSensitivity(Settings->LookSensitivity);
	Player->SetInvertLookY(Settings->bInvertLookY);

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] ApplyGameplaySettingsToPlayer: Sensitivity = %.2f, InvertY = %s"),
		Settings->LookSensitivity, Settings->bInvertLookY ? TEXT("true") : TEXT("false"));
}

// ---------- 공통 ----------

void ULCOGameSettingsSubsystem::ResetToDefaults()
{
	if (!Settings)
	{
		return;
	}

	// 감도 / 반전 / 밝기
	const ULCOGameSettingsSaveGame* Defaults = GetDefault<ULCOGameSettingsSaveGame>();
	Settings->LookSensitivity = Defaults->LookSensitivity;
	Settings->bInvertLookY = Defaults->bInvertLookY;
	Settings->Brightness = Defaults->Brightness;

	ApplyBrightness();
	SaveSettings();

	if (ALCOPlayerCharacter* Player = GetCurrentPlayer())
	{
		ApplyGameplaySettingsToPlayer(Player);
	}

	// 그래픽: 엔진 기본값으로 되돌린 뒤 적용·저장
	if (UGameUserSettings* EngineSettings = GetEngineSettings())
	{
		EngineSettings->SetToDefaults();
		EngineSettings->ApplySettings(false);
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] ResetToDefaults"));

	OnSettingsChanged.Broadcast();
}

void ULCOGameSettingsSubsystem::LoadSettings()
{
	if (UGameplayStatics::DoesSaveGameExist(SettingsSlotName, SettingsUserIndex))
	{
		Settings = Cast<ULCOGameSettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSlotName, SettingsUserIndex));
	}

	// 파일이 없거나 읽기 실패 → 기본값으로 새로 만듦
	if (!Settings)
	{
		Settings = Cast<ULCOGameSettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(ULCOGameSettingsSaveGame::StaticClass()));
	}

	// Log Test
	if (Settings)
	{
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] Load: Sensitivity = %.2f, InvertY = %s, Brightness = %.2f"),
			Settings->LookSensitivity, Settings->bInvertLookY ? TEXT("true") : TEXT("false"), Settings->Brightness);
	}
}

void ULCOGameSettingsSubsystem::SaveSettings() const
{
	if (Settings)
	{
		// 설정은 크기가 작아 즉시 저장
		UGameplayStatics::SaveGameToSlot(Settings, SettingsSlotName, SettingsUserIndex);
	}
}

ALCOPlayerCharacter* ULCOGameSettingsSubsystem::GetCurrentPlayer() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* PlayerController = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	return PlayerController ? Cast<ALCOPlayerCharacter>(PlayerController->GetPawn()) : nullptr;
}
