#include "QRMountHusbandryComponent.h"
#include "QRWeatherComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"


UQRMountHusbandryComponent::UQRMountHusbandryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


void UQRMountHusbandryComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRMountHusbandryComponent, DaysTamed);
	DOREPLIFETIME(UQRMountHusbandryComponent, bIsTamed);
	DOREPLIFETIME(UQRMountHusbandryComponent, CurrentStressPool);
	DOREPLIFETIME(UQRMountHusbandryComponent, HoursSinceLastCare);
	DOREPLIFETIME(UQRMountHusbandryComponent, CurrentRider);
}


void UQRMountHusbandryComponent::TickGameHours(float DeltaGameHours)
{
	if (DeltaGameHours <= 0.0f) return;

	HoursSinceLastCare += DeltaGameHours;

	// In-progress taming sessions break with P_tameFailPerDay if the
	// animal goes more than FeedIntervalHours without care. Once tamed
	// the relationship survives -- it just gets stressed.
	if (!bIsTamed && DaysTamed > 0.0f &&
		HoursSinceLastCare > FeedIntervalHours)
	{
		const float DaysUnattended = (HoursSinceLastCare - FeedIntervalHours) / 24.0f;
		if (FMath::FRand() < P_tameFailPerDay * DaysUnattended)
		{
			DaysTamed = 0.0f;
			HoursSinceLastCare = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("[QRMount] %s taming session broken (unattended)"),
				GetOwner() ? *GetOwner()->GetName() : TEXT("?"));
		}
	}

	// Stress accumulates from riding + active weather, decays otherwise.
	float StressDelta = -StressDecayPerHour * DeltaGameHours;
	if (CurrentRider)
	{
		StressDelta += StressFromRidingPerHour * DeltaGameHours;
	}
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (UQRWeatherComponent* Weather = It->FindComponentByClass<UQRWeatherComponent>())
			{
				if (Weather->HasActiveEvent())
				{
					StressDelta += StressFromWeatherPerHour * DeltaGameHours;
				}
				break;
			}
		}
	}
	CurrentStressPool = FMath::Clamp(CurrentStressPool + StressDelta, 0.0f, 100.0f);

	// Auto-panic the moment we cross the threshold with a rider on top
	// -- they get bucked instead of waiting for the next FeedOrPet call.
	if (CurrentRider && CurrentStressPool >= PanicThreshold)
	{
		Panic();
	}
}


bool UQRMountHusbandryComponent::FeedOrPet()
{
	// A panicking animal refuses interaction until stress drops.
	if (CurrentStressPool >= PanicThreshold)
	{
		return false;
	}

	HoursSinceLastCare = 0.0f;
	CurrentStressPool  = FMath::Clamp(CurrentStressPool - 20.0f, 0.0f, 100.0f);

	if (!bIsTamed)
	{
		// One feeding interaction = roughly one day's progress, with a
		// small TamingDifficultyScore variability layer that the host
		// species can author if it wants to (left at 1.0 here).
		DaysTamed = FMath::Min(BaseTameDays * 2.0f, DaysTamed + 1.0f);
		if (DaysTamed >= BaseTameDays)
		{
			bIsTamed = true;
			OnTamed.Broadcast();
			UE_LOG(LogTemp, Log, TEXT("[QRMount] %s is now tamed"),
				GetOwner() ? *GetOwner()->GetName() : TEXT("?"));
		}
	}
	return true;
}


void UQRMountHusbandryComponent::ApplyStress(float Amount)
{
	CurrentStressPool = FMath::Clamp(CurrentStressPool + Amount, 0.0f, 100.0f);
	if (CurrentRider && CurrentStressPool >= PanicThreshold)
	{
		Panic();
	}
}


bool UQRMountHusbandryComponent::TryMount(AActor* Rider)
{
	if (!Rider || !bIsTamed || CurrentRider) return false;
	if (CurrentStressPool >= PanicThreshold)
	{
		Panic();
		return false;
	}
	CurrentRider = Rider;
	return true;
}


void UQRMountHusbandryComponent::Dismount()
{
	CurrentRider = nullptr;
}


void UQRMountHusbandryComponent::Panic()
{
	if (CurrentRider)
	{
		UE_LOG(LogTemp, Log, TEXT("[QRMount] %s panicked -- rider bucked"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("?"));
		CurrentRider = nullptr;
	}
	// One-shot trust break: the player has to start over. Stress
	// doesn't reset -- the animal is still rattled, just no longer
	// usable until calmed and re-trained.
	bIsTamed  = false;
	DaysTamed = 0.0f;
	OnPanicked.Broadcast();
}
