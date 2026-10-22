#include "Core/LCOGameSettingsSubsystem.h"
#include "LastCheckOut.h"
#include "Core/LCOGameSettingsSaveGame.h"
#include "Player/LCOPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "GameplayTagContainer.h"

namespace
{
	// 키 설정은 첫 번째 칸(First Slot)만 사용
	constexpr EPlayerMappableKeySlot KeyMappingSlot = EPlayerMappableKeySlot::First;

	// 설정창에 표시할 순서 (IA / IMC의 Player Mappable Key Settings Name)
	const TArray<FName>& GetRemappableKeyNames()
	{
		static const TArray<FName> Names =
		{
			TEXT("Move_Forward"),
			TEXT("Move_Backward"),
			TEXT("Move_Left"),
			TEXT("Move_Right"),
			TEXT("Sprint"),
			TEXT("Crouch"),
			TEXT("Interact"),
			TEXT("Sense"),
			TEXT("Capture"),
			TEXT("ToggleView"),
			TEXT("Inventory"),
			TEXT("Notebook"),
			TEXT("QuickSlot")
		};
		return Names;
	}
}

const FString ULCOGameSettingsSubsystem::SettingsSlotName(TEXT("LCO_Settings"));

void ULCOGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadSettings();
	ApplyBrightness();

	// 게임 시작 시 적용된 그래픽 값 = 확정 값 (확인 창 되돌리기 기준)
	CaptureConfirmedGraphics();
}

// 그래픽

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
	EngineSettings->ConfirmVideoMode();
	CaptureConfirmedGraphics();
	bWaitingGraphicsConfirm = false;

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

void ULCOGameSettingsSubsystem::ApplyGraphicsSettingsForConfirm()
{
	UGameUserSettings* EngineSettings = GetEngineSettings();
	if (!EngineSettings)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameSettings] ApplyGraphicsSettingsForConfirm: GameUserSettings not found"));
		return;
	}

	// 화면에만 적용 (ApplySettings와 달리 GameUserSettings.ini에 저장하지 않음)
	EngineSettings->ApplyResolutionSettings(false);
	EngineSettings->ApplyNonResolutionSettings();
	bWaitingGraphicsConfirm = true;

	// Log Test
	const FIntPoint Resolution = EngineSettings->GetScreenResolution();
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] ApplyGraphicsSettingsForConfirm: %dx%d, Mode = %d, Quality = %d (waiting confirm)"),
		Resolution.X, Resolution.Y, static_cast<int32>(EngineSettings->GetFullscreenMode()), EngineSettings->GetOverallScalabilityLevel());

	OnSettingsChanged.Broadcast();
}

void ULCOGameSettingsSubsystem::ConfirmGraphicsSettings()
{
	UGameUserSettings* EngineSettings = GetEngineSettings();
	if (!EngineSettings || !bWaitingGraphicsConfirm)
	{
		return;
	}

	// 지금 화면 값을 확정 + 저장
	EngineSettings->ConfirmVideoMode();
	EngineSettings->SaveSettings();
	CaptureConfirmedGraphics();
	bWaitingGraphicsConfirm = false;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] ConfirmGraphicsSettings: %dx%d, Mode = %d, Quality = %d (saved)"),
		ConfirmedResolution.X, ConfirmedResolution.Y, static_cast<int32>(ConfirmedWindowMode), ConfirmedQuality);

	OnSettingsChanged.Broadcast();
}

void ULCOGameSettingsSubsystem::RevertAppliedGraphicsSettings()
{
	UGameUserSettings* EngineSettings = GetEngineSettings();
	if (!EngineSettings || !bWaitingGraphicsConfirm)
	{
		return;
	}

	// 직전 확정 값으로 되돌려 다시 적용 + 저장
	EngineSettings->SetFullscreenMode(ConfirmedWindowMode);
	EngineSettings->SetScreenResolution(ConfirmedResolution);
	if (ConfirmedQuality >= 0)
	{
		// -1 = 항목별 사용자 지정 품질 (전체 품질로 덮어쓰지 않음)
		EngineSettings->SetOverallScalabilityLevel(ConfirmedQuality);
	}
	EngineSettings->ApplySettings(false);
	EngineSettings->ConfirmVideoMode();
	bWaitingGraphicsConfirm = false;

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] RevertAppliedGraphicsSettings: back to %dx%d, Mode = %d, Quality = %d"),
		ConfirmedResolution.X, ConfirmedResolution.Y, static_cast<int32>(ConfirmedWindowMode), ConfirmedQuality);

	OnSettingsChanged.Broadcast();
}

void ULCOGameSettingsSubsystem::CaptureConfirmedGraphics()
{
	if (const UGameUserSettings* EngineSettings = GetEngineSettings())
	{
		ConfirmedWindowMode = EngineSettings->GetFullscreenMode();
		ConfirmedResolution = EngineSettings->GetScreenResolution();
		ConfirmedQuality = EngineSettings->GetOverallScalabilityLevel();
	}
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
		EngineSettings->ConfirmVideoMode();
	}
	CaptureConfirmedGraphics();
	bWaitingGraphicsConfirm = false;

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

// 키 설정

UEnhancedInputUserSettings* ULCOGameSettingsSubsystem::GetInputUserSettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return InputSubsystem ? InputSubsystem->GetUserSettings() : nullptr;
}

bool ULCOGameSettingsSubsystem::RegisterKeyMappingContext(UInputMappingContext* MappingContext)
{
	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings();
	if (!InputSettings || !MappingContext)
	{
		return false;
	}

	if (InputSettings->IsMappingContextRegistered(MappingContext))
	{
		return true;
	}

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] RegisterKeyMappingContext: %s"), *MappingContext->GetName());
	return InputSettings->RegisterInputMappingContext(MappingContext);
}

TArray<FLCOKeyMappingInfo> ULCOGameSettingsSubsystem::GetKeyMappings() const
{
	TArray<FLCOKeyMappingInfo> Result;

	const UEnhancedInputUserSettings* InputSettings = GetInputUserSettings();
	if (!InputSettings)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameSettings] GetKeyMappings: Input user settings not found (Enable User Settings?)"));
		return Result;
	}

	// 등록되지 않은 액션(아직 IMC에 없는 퀵슬롯 등)은 건너뜀
	for (const FName& MappingName : GetRemappableKeyNames())
	{
		if (const FPlayerKeyMapping* Mapping = InputSettings->FindCurrentMappingForSlot(MappingName, KeyMappingSlot))
		{
			FLCOKeyMappingInfo& Info = Result.AddDefaulted_GetRef();
			Info.MappingName = MappingName;
			Info.DisplayName = Mapping->GetDisplayName();
			Info.CurrentKey = Mapping->GetCurrentKey();
			Info.DefaultKey = Mapping->GetDefaultKey();
		}
	}

	return Result;
}

ELCOKeyRemapResult ULCOGameSettingsSubsystem::RemapKey(FName MappingName, FKey NewKey, FName& OutSwappedMappingName)
{
	OutSwappedMappingName = NAME_None;

	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings();
	if (!InputSettings)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameSettings] RemapKey: Input user settings not found (Enable User Settings?)"));
		return ELCOKeyRemapResult::Failed;
	}

	if (IsKeyBlocked(NewKey))
	{
		// Log Test
		UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] RemapKey: %s -> %s blocked"), *MappingName.ToString(), *NewKey.ToString());
		return ELCOKeyRemapResult::Blocked;
	}

	const FPlayerKeyMapping* CurrentMapping = InputSettings->FindCurrentMappingForSlot(MappingName, KeyMappingSlot);
	if (!CurrentMapping)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameSettings] RemapKey: %s not found"), *MappingName.ToString());
		return ELCOKeyRemapResult::Failed;
	}

	const FKey OldKey = CurrentMapping->GetCurrentKey();
	if (OldKey == NewKey)
	{
		return ELCOKeyRemapResult::Unchanged;
	}

	// 같은 키를 쓰던 액션 찾기 → 그 액션은 이 액션의 원래 키로 교체
	for (const FName& OtherName : GetRemappableKeyNames())
	{
		if (OtherName == MappingName)
		{
			continue;
		}

		const FPlayerKeyMapping* OtherMapping = InputSettings->FindCurrentMappingForSlot(OtherName, KeyMappingSlot);
		if (OtherMapping && OtherMapping->GetCurrentKey() == NewKey)
		{
			OutSwappedMappingName = OtherName;
			break;
		}
	}

	FGameplayTagContainer FailureReason;

	FMapPlayerKeyArgs Args;
	Args.MappingName = MappingName;
	Args.Slot = KeyMappingSlot;
	Args.NewKey = NewKey;
	InputSettings->MapPlayerKey(Args, FailureReason);

	if (!FailureReason.IsEmpty())
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameSettings] RemapKey: %s -> %s failed (%s)"),
			*MappingName.ToString(), *NewKey.ToString(), *FailureReason.ToStringSimple());
		OutSwappedMappingName = NAME_None;
		return ELCOKeyRemapResult::Failed;
	}

	if (!OutSwappedMappingName.IsNone())
	{
		FMapPlayerKeyArgs SwapArgs;
		SwapArgs.MappingName = OutSwappedMappingName;
		SwapArgs.Slot = KeyMappingSlot;
		SwapArgs.NewKey = OldKey;
		InputSettings->MapPlayerKey(SwapArgs, FailureReason);
	}

	InputSettings->SaveSettings();

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] RemapKey: %s %s -> %s (Swapped = %s)"),
		*MappingName.ToString(), *OldKey.ToString(), *NewKey.ToString(), *OutSwappedMappingName.ToString());

	OnKeyMappingChanged.Broadcast(MappingName, OutSwappedMappingName);
	return OutSwappedMappingName.IsNone() ? ELCOKeyRemapResult::Success : ELCOKeyRemapResult::Swapped;
}

void ULCOGameSettingsSubsystem::ResetKeysToDefault()
{
	UEnhancedInputUserSettings* InputSettings = GetInputUserSettings();
	if (!InputSettings)
	{
		UE_LOG(LogLCOCore, Warning, TEXT("[LCOGameSettings] ResetKeysToDefault: Input user settings not found (Enable User Settings?)"));
		return;
	}

	FGameplayTagContainer FailureReason;
	InputSettings->ResetKeyProfileIdToDefault(InputSettings->GetActiveKeyProfileId(), FailureReason);
	InputSettings->SaveSettings();

	// Log Test
	UE_LOG(LogLCOCore, Log, TEXT("[LCOGameSettings] ResetKeysToDefault"));

	OnKeyMappingChanged.Broadcast(NAME_None, NAME_None);
}

bool ULCOGameSettingsSubsystem::IsKeyBlocked(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return true;
	}

	// 게임패드 / 터치 / 마우스 이동 같은 축 입력은 지정 불가 (키보드 / 마우스 버튼만)
	if (Key.IsGamepadKey() || Key.IsTouch() || Key.IsAnalog())
	{
		return true;
	}

	// ESC / O: 옵션(일시정지) 고정 키, Windows 키: 시스템 키
	return Key == EKeys::Escape || Key == EKeys::O || Key == EKeys::LeftCommand || Key == EKeys::RightCommand;
}
