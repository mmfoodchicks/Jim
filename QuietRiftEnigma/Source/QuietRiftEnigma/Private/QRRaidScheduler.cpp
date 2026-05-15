#include "QRRaidScheduler.h"
#include "QRGameMode.h"
#include "QRVanguardColony.h"
#include "QRWeatherComponent.h"
#include "Engine/World.h"


AQRRaidScheduler::AQRRaidScheduler()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f;
	bReplicates = true;
}


void AQRRaidScheduler::BeginPlay()
{
	Super::BeginPlay();
	Accum = 0.0f;
}


void AQRRaidScheduler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!HasAuthority()) return;
	if (bRaidActive) return;  // don't start a second raid while one runs

	Accum += DeltaTime;
	if (Accum < CheckIntervalSeconds) return;
	Accum = 0.0f;

	const float Pressure = ComputePressure();
	if (Pressure < SilentPressureFloor) return;

	const float Roll = FMath::FRand();
	const float Threshold = BaseRaidChance * (1.0f + Pressure * 4.0f);
	if (Roll > Threshold) return;

	ActiveTier = RollTier(Pressure);
	bRaidActive = true;
	OnRaidIncoming.Broadcast(ActiveTier);

	UE_LOG(LogTemp, Log, TEXT("[QRRaidScheduler] raid incoming — tier %d (pressure %.2f, threshold %.2f, roll %.2f)"),
		static_cast<int32>(ActiveTier), Pressure, Threshold, Roll);
}


void AQRRaidScheduler::TriggerRaidNow()
{
	if (bRaidActive) return;
	const float Pressure = ComputePressure();
	ActiveTier = RollTier(Pressure);
	bRaidActive = true;
	OnRaidIncoming.Broadcast(ActiveTier);
	UE_LOG(LogTemp, Log, TEXT("[QRRaidScheduler] manually triggered raid (tier %d)"),
		static_cast<int32>(ActiveTier));
}


void AQRRaidScheduler::ConcludeCurrentRaid()
{
	if (!bRaidActive) return;
	bRaidActive = false;
	OnRaidConcluded.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("[QRRaidScheduler] raid concluded"));
}


float AQRRaidScheduler::ComputePressure() const
{
	float Pressure = 0.0f;
	if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
	{
		// Day ramp: each game-day adds 0.05 pressure until clamp.
		Pressure += GM->DayNumber * 0.05f;

		// Night multiplier — only applies as a final modifier below.
		if (GM->bIsNight) Pressure *= NightMultiplier;

		// Weather pressure — any active event escalates.
		if (UQRWeatherComponent* Weather = GM->Weather)
		{
			// Reflective lookup so we don't hard-bind to the enum value.
			const FBoolProperty* InEvent = FindFProperty<FBoolProperty>(Weather->GetClass(), TEXT("bEventActive"));
			if (InEvent && InEvent->GetPropertyValue_InContainer(Weather))
			{
				Pressure *= WeatherMultiplier;
			}
		}

		// Concordat hostility — read the colony actor directly if found.
		if (AQRVanguardColony* Concordat = GM->VanguardConcordat)
		{
			// Convention: VanguardColony exposes a float Hostility [0..1].
			const FFloatProperty* HP = FindFProperty<FFloatProperty>(Concordat->GetClass(), TEXT("Hostility"));
			if (HP)
			{
				const float H = HP->GetPropertyValue_InContainer(Concordat);
				Pressure += H * 0.50f;
			}
		}
	}
	return FMath::Clamp(Pressure, 0.0f, 1.0f);
}


EQRRaidTier AQRRaidScheduler::RollTier(float Pressure) const
{
	if (Pressure < SkirmishCap) return EQRRaidTier::Skirmish;
	if (Pressure < PressureCap) return EQRRaidTier::Pressure;
	return EQRRaidTier::Assault;
}
