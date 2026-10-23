#include "Interaction/Reveal/LCORevealLightSubsystem.h"
#include "Interaction/Reveal/LCORevealLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

const FName ULCORevealLightSubsystem::PN_Position(TEXT("RevealLightPosition"));
const FName ULCORevealLightSubsystem::PN_Direction(TEXT("RevealLightDirection"));
const FName ULCORevealLightSubsystem::PN_OuterConeCos(TEXT("RevealOuterConeCos"));
const FName ULCORevealLightSubsystem::PN_InnerConeCos(TEXT("RevealInnerConeCos"));
const FName ULCORevealLightSubsystem::PN_Range(TEXT("RevealRange"));
const FName ULCORevealLightSubsystem::PN_Intensity(TEXT("RevealIntensity"));
const FName ULCORevealLightSubsystem::PN_LightType(TEXT("RevealLightType"));

ULCORevealLightSubsystem::ULCORevealLightSubsystem()
{
	RevealMPCPath = FSoftObjectPath(TEXT("/Game/Interactables/Reveal/MPC_RevealLight.MPC_RevealLight"));
}

bool ULCORevealLightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 게임 / PIE 월드에서만 (에디터 미리보기 월드 제외)
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void ULCORevealLightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RevealMPC = Cast<UMaterialParameterCollection>(RevealMPCPath.TryLoad());
}

TStatId ULCORevealLightSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULCORevealLightSubsystem, STATGROUP_Tickables);
}

void ULCORevealLightSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ActiveLight = SelectActiveLight();
	WriteToMPC();
}

void ULCORevealLightSubsystem::RegisterRevealLight(ULCORevealLightComponent* Light)
{
	if (IsValid(Light))
	{
		RegisteredLights.AddUnique(Light);
	}
}

void ULCORevealLightSubsystem::UnregisterRevealLight(ULCORevealLightComponent* Light)
{
	RegisteredLights.Remove(Light);
}

float ULCORevealLightSubsystem::GetRevealStrengthAt(const FVector& WorldLocation, ELCORevealLightType RequiredLightType) const
{
	if (!ActiveLight.bActive || ActiveLight.LightType != RequiredLightType || ActiveLight.Range <= 0.f)
	{
		return 0.f;
	}

	// MF_RevealMask와 같은 계산
	const FVector ToPoint = WorldLocation - ActiveLight.Position;
	const float Distance = ToPoint.Size();
	if (Distance >= ActiveLight.Range || Distance <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}

	const float CosAngle = FVector::DotProduct(ToPoint / Distance, ActiveLight.Direction);
	const float ConeMask = FMath::SmoothStep(ActiveLight.OuterConeCos, ActiveLight.InnerConeCos, CosAngle);
	const float DistanceMask = 1.f - Distance / ActiveLight.Range;

	return ConeMask * DistanceMask;
}

FLCORevealLightState ULCORevealLightSubsystem::SelectActiveLight()
{
	FLCORevealLightState Best;
	int32 BestPriority = TNumericLimits<int32>::Lowest();

	// 등록된 광원 (UV 램프 등)
	for (int32 Index = RegisteredLights.Num() - 1; Index >= 0; --Index)
	{
		const ULCORevealLightComponent* Light = RegisteredLights[Index].Get();
		if (!Light)
		{
			RegisteredLights.RemoveAtSwap(Index);
			continue;
		}

		if (Light->IsRevealActive() && Light->GetPriority() > BestPriority)
		{
			BestPriority = Light->GetPriority();
			Best = MakeState(Light->GetSpotLight(), Light->GetLightType());
		}
	}

	// 플레이어 손전등 (등록된 광원보다 우선순위가 높을 때만)
	if (bUsePlayerFlashlight && PlayerFlashlightPriority > BestPriority)
	{
		const USpotLightComponent* Flashlight = FindPlayerFlashlight();
		if (Flashlight && Flashlight->IsVisible() && Flashlight->Intensity > 0.f)
		{
			Best = MakeState(Flashlight, ELCORevealLightType::Flashlight);
		}
	}

	return Best;
}

USpotLightComponent* ULCORevealLightSubsystem::FindPlayerFlashlight()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	if (Pawn != CachedPlayerPawn.Get())
	{
		CachedPlayerPawn = Pawn;
		CachedPlayerFlashlight = Pawn ? Pawn->FindComponentByClass<USpotLightComponent>() : nullptr;
	}

	return CachedPlayerFlashlight.Get();
}

FLCORevealLightState ULCORevealLightSubsystem::MakeState(const USpotLightComponent* SpotLight, ELCORevealLightType LightType) const
{
	FLCORevealLightState State;
	if (!SpotLight)
	{
		return State;
	}

	const float OuterAngle = FMath::Clamp(SpotLight->OuterConeAngle, 1.f, 89.f);
	const float InnerAngle = FMath::Clamp(FMath::Max(SpotLight->InnerConeAngle, OuterAngle * MinInnerConeRatio), 0.f, OuterAngle - 0.5f);

	State.bActive = true;
	State.LightType = LightType;
	State.Position = SpotLight->GetComponentLocation();
	State.Direction = SpotLight->GetForwardVector();
	State.OuterConeCos = FMath::Cos(FMath::DegreesToRadians(OuterAngle));
	State.InnerConeCos = FMath::Cos(FMath::DegreesToRadians(InnerAngle));
	State.Range = SpotLight->AttenuationRadius;
	return State;
}

void ULCORevealLightSubsystem::WriteToMPC()
{
	if (!RevealMPC)
	{
		if (!bWarnedMissingMPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LCORevealLight] MPC not found: %s (reveal materials will not react to light)"), *RevealMPCPath.ToString());
			bWarnedMissingMPC = true;
		}
		return;
	}

	UMaterialParameterCollectionInstance* Instance = GetWorld()->GetParameterCollectionInstance(RevealMPC);
	if (!Instance)
	{
		return;
	}

	// 광원이 없으면 Intensity 0만 기록 (나머지 값은 마지막 값 유지, 머티리얼은 Intensity로 꺼짐)
	Instance->SetScalarParameterValue(PN_Intensity, ActiveLight.bActive ? 1.f : 0.f);
	if (!ActiveLight.bActive)
	{
		return;
	}

	Instance->SetVectorParameterValue(PN_Position, FLinearColor(ActiveLight.Position));
	Instance->SetVectorParameterValue(PN_Direction, FLinearColor(ActiveLight.Direction));
	Instance->SetScalarParameterValue(PN_OuterConeCos, ActiveLight.OuterConeCos);
	Instance->SetScalarParameterValue(PN_InnerConeCos, ActiveLight.InnerConeCos);
	Instance->SetScalarParameterValue(PN_Range, ActiveLight.Range);
	Instance->SetScalarParameterValue(PN_LightType, static_cast<float>(ActiveLight.LightType));
}
