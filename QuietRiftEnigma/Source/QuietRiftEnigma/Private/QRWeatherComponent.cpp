#include "QRWeatherComponent.h"
#include "QRGameplayTags.h"
#include "Net/UnrealNetwork.h"

UQRWeatherComponent::UQRWeatherComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Randomize initial scheduling so not all servers sync their first event
	HoursUntilNextEvent = FMath::FRandRange(MinIntervalHours, MaxIntervalHours);
}

void UQRWeatherComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRWeatherComponent, ActiveWeatherEvent);
	DOREPLIFETIME(UQRWeatherComponent, EventIntensity);
	DOREPLIFETIME(UQRWeatherComponent, EventRemainingHours);
	DOREPLIFETIME(UQRWeatherComponent, HoursUntilNextEvent);
	DOREPLIFETIME(UQRWeatherComponent, ActiveWeatherTag);
}

void UQRWeatherComponent::TriggerWeatherEvent(EQRWeatherEvent EventType, float Intensity, float DurationHours)
{
	if (ActiveWeatherEvent != EQRWeatherEvent::None)
		EndWeatherEvent();

	ActiveWeatherEvent   = EventType;
	EventIntensity       = FMath::Clamp(Intensity, 0.0f, 1.0f);
	EventRemainingHours  = FMath::Max(DurationHours, 0.1f);
	ActiveWeatherTag     = EventTypeToTag(EventType);

	OnWeatherEventStarted.Broadcast(ActiveWeatherEvent, EventIntensity);
	ScheduleNextEvent();
}

void UQRWeatherComponent::EndWeatherEvent()
{
	if (ActiveWeatherEvent == EQRWeatherEvent::None) return;

	const EQRWeatherEvent Ended = ActiveWeatherEvent;
	ActiveWeatherEvent  = EQRWeatherEvent::None;
	EventIntensity      = 0.0f;
	EventRemainingHours = 0.0f;
	ActiveWeatherTag    = FGameplayTag();

	OnWeatherEventEnded.Broadcast(Ended);
}

void UQRWeatherComponent::AdvanceByHours(float GameHoursElapsed)
{
	if (!GetOwner()->HasAuthority()) return;

	if (ActiveWeatherEvent != EQRWeatherEvent::None)
	{
		EventRemainingHours -= GameHoursElapsed;
		if (EventRemainingHours <= 0.0f)
			EndWeatherEvent();
	}
	else
	{
		HoursUntilNextEvent -= GameHoursElapsed;
		if (HoursUntilNextEvent <= 0.0f)
		{
			// Pick a random event weighted roughly by intensity
			const int32 NumEvents = 6; // EQRWeatherEvent has 6 non-None values
			const int32 Pick      = FMath::RandRange(1, NumEvents);
			TriggerWeatherEvent(
				static_cast<EQRWeatherEvent>(Pick),
				FMath::FRandRange(0.3f, 1.0f),
				FMath::FRandRange(DefaultDurationHours * 0.5f, DefaultDurationHours * 2.0f)
			);
		}
	}
}

float UQRWeatherComponent::GetAcidRainDamageRate() const
{
	if (ActiveWeatherEvent != EQRWeatherEvent::AcidRain) return 0.0f;
	return 0.002f * EventIntensity; // DPS to structures
}

float UQRWeatherComponent::GetTemperatureModifier() const
{
	switch (ActiveWeatherEvent)
	{
	case EQRWeatherEvent::HeatWave:     return  2.0f * EventIntensity;
	case EQRWeatherEvent::IceFog:       return -3.0f * EventIntensity;
	case EQRWeatherEvent::VentEruption: return  4.0f * EventIntensity;
	default: return 0.0f;
	}
}

float UQRWeatherComponent::GetFoulingMultiplier() const
{
	if (ActiveWeatherEvent == EQRWeatherEvent::DustStorm)
		return 1.0f + EventIntensity; // 1.0x–2.0x extra fouling
	return 1.0f;
}

FGameplayTag UQRWeatherComponent::EventTypeToTag(EQRWeatherEvent Event) const
{
	switch (Event)
	{
	case EQRWeatherEvent::DustStorm:     return QRGameplayTags::Weather_DustStorm;
	case EQRWeatherEvent::AcidRain:      return QRGameplayTags::Weather_AcidRain;
	case EQRWeatherEvent::VentEruption:  return QRGameplayTags::Weather_VentEruption;
	case EQRWeatherEvent::HeatWave:      return QRGameplayTags::Weather_HeatWave;
	case EQRWeatherEvent::IceFog:        return QRGameplayTags::Weather_IceFog;
	case EQRWeatherEvent::MagneticStorm: return QRGameplayTags::Weather_MagneticStorm;
	default: return FGameplayTag();
	}
}

void UQRWeatherComponent::ScheduleNextEvent()
{
	HoursUntilNextEvent = FMath::FRandRange(MinIntervalHours, MaxIntervalHours);
}

void UQRWeatherComponent::OnRep_ActiveWeatherEvent()
{
	// Client-side: re-broadcast so UI and FX systems can react
	if (ActiveWeatherEvent != EQRWeatherEvent::None)
		OnWeatherEventStarted.Broadcast(ActiveWeatherEvent, EventIntensity);
	else
		OnWeatherEventEnded.Broadcast(EQRWeatherEvent::None);
}
