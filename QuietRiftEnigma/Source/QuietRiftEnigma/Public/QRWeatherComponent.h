#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRWeatherComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeatherEventStarted, EQRWeatherEvent, EventType, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeatherEventEnded, EQRWeatherEvent, EventType);

// Manages the active atmospheric hazard for the map.
// Lives on the GameState or a persistent world actor (server-authoritative).
// Broadcasts weather transitions; tick-driven via AdvanceByHours called from game mode.
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRWeatherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRWeatherComponent();

	// ── Config ───────────────────────────────
	// Hours between randomly scheduled weather events (uniform distribution)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Config", meta = (ClampMin = "1"))
	float MinIntervalHours = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Config", meta = (ClampMin = "1"))
	float MaxIntervalHours = 168.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weather|Config", meta = (ClampMin = "0.5"))
	float DefaultDurationHours = 4.0f;

	// ── Runtime State (Replicated) ───────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ActiveWeatherEvent, Category = "Weather")
	EQRWeatherEvent ActiveWeatherEvent = EQRWeatherEvent::None;

	// Strength of the active event [0..1]
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weather")
	float EventIntensity = 0.0f;

	// Remaining game-hours until the active event clears (0 = no active event)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weather")
	float EventRemainingHours = 0.0f;

	// Game-hours until the next random event is scheduled
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weather")
	float HoursUntilNextEvent = 72.0f;

	// Currently active weather tag (matches ActiveWeatherEvent for Blueprint querying)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weather")
	FGameplayTag ActiveWeatherTag;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Weather|Events")
	FOnWeatherEventStarted OnWeatherEventStarted;

	UPROPERTY(BlueprintAssignable, Category = "Weather|Events")
	FOnWeatherEventEnded OnWeatherEventEnded;

	// ── Interface ────────────────────────────
	// Manually trigger a weather event (overrides any active event)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weather")
	void TriggerWeatherEvent(EQRWeatherEvent EventType, float Intensity = 0.5f, float DurationHours = 4.0f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weather")
	void EndWeatherEvent();

	UFUNCTION(BlueprintPure, Category = "Weather")
	bool HasActiveEvent() const { return ActiveWeatherEvent != EQRWeatherEvent::None; }

	// Ongoing DPS applied to unprotected actors during acid rain (0 when not acid rain)
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetAcidRainDamageRate() const;

	// Temperature modifier applied to survivor core temperature each game-second
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetTemperatureModifier() const;

	// Weapon fouling multiplier for the active event (1.0 = none, 2.0 during dust storm)
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetFoulingMultiplier() const;

	// Call each game-hour from the game mode sim tick
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weather")
	void AdvanceByHours(float GameHoursElapsed);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FGameplayTag EventTypeToTag(EQRWeatherEvent Event) const;
	void ScheduleNextEvent();

	UFUNCTION()
	void OnRep_ActiveWeatherEvent();
};
