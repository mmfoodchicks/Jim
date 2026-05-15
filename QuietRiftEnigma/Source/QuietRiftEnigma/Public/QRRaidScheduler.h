#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRRaidScheduler.generated.h"

class AQRNPCActor;


/**
 * Raid severity tier — drives party size, leader presence, and reward
 * scaling. Master GDD §12 calls these Skirmish / Pressure / Assault.
 */
UENUM(BlueprintType)
enum class EQRRaidTier : uint8
{
	Skirmish UMETA(DisplayName = "Skirmish (2-3 attackers, light infantry)"),
	Pressure UMETA(DisplayName = "Pressure (4-6 attackers, mid-grade weapons)"),
	Assault  UMETA(DisplayName = "Assault (8-12 attackers, leader present)"),
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaidIncoming, EQRRaidTier, Tier);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaidConcluded);


/**
 * Periodic raid roller. Master GDD §12 + GAME_OVERVIEW "Voss's
 * personal guard dispatched without warning when his hostility
 * crosses a threshold."
 *
 * Every check interval (default 60 real-seconds = 1 game-hour at the
 * default 20-real-minute day length) the scheduler:
 *
 *   1. Aggregates pressure factors:
 *      • Day number (slow base ramp)
 *      • Night flag (×NightMultiplier when bIsNight)
 *      • Active weather event (×WeatherMultiplier if storm/etc)
 *      • Concordat hostility (provided by AQRVanguardColony if found)
 *      • Player camp prosperity (placeholder for v1)
 *   2. Rolls against a sliding probability threshold:
 *      RaidChance = BaseDailyRaidChance × cumulative pressure × interval
 *   3. On hit, picks a tier weighted by current pressure level and
 *      broadcasts OnRaidIncoming. Designer subscribes to mount the
 *      raid widget, play stinger, etc.
 *
 * For v1 the scheduler ONLY rolls + broadcasts. Actual raid party
 * spawning (NPCs at world edge, AI walks toward base) follows in a
 * dedicated pass once AQRNPCActor has behavior-tree AI. The existing
 * AQRNPCSpawner can stand in as a placeholder.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRRaidScheduler : public AActor
{
	GENERATED_BODY()

public:
	AQRRaidScheduler();

	// How often (real-seconds) the roll runs. Default 60s = ~1 game-hour
	// at the default DayLengthRealSeconds=1200.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid",
		meta = (ClampMin = "5", ClampMax = "3600"))
	float CheckIntervalSeconds = 60.0f;

	// Base chance per check at neutral pressure (no night, no storm,
	// no hostile faction, low days). ~0.02 = 2% per check ≈ one raid
	// every ~50 checks ≈ one raid every couple game-days.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid",
		meta = (ClampMin = "0", ClampMax = "1"))
	float BaseRaidChance = 0.02f;

	// Multipliers stacked into the roll.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid",
		meta = (ClampMin = "1", ClampMax = "10"))
	float NightMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid",
		meta = (ClampMin = "1", ClampMax = "10"))
	float WeatherMultiplier = 1.5f;

	// Pressure floor below which raids stop entirely (player can have
	// "quiet" days early when nothing has triggered hostility yet).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid",
		meta = (ClampMin = "0", ClampMax = "1"))
	float SilentPressureFloor = 0.10f;

	// Skirmish if cumulative pressure < this threshold; Pressure if
	// < the next; otherwise Assault.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid")
	float SkirmishCap = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Raid")
	float PressureCap = 0.65f;

	// Force-trigger a raid now (cheat / test). Bypasses the roll but
	// still picks a tier from current pressure.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Raid")
	void TriggerRaidNow();

	UPROPERTY(BlueprintAssignable, Category = "QR|Raid|Events")
	FOnRaidIncoming OnRaidIncoming;

	UPROPERTY(BlueprintAssignable, Category = "QR|Raid|Events")
	FOnRaidConcluded OnRaidConcluded;

	// Resolve a finished raid (called by the encounter system when
	// the last attacker dies or retreats). v1: just broadcasts.
	UFUNCTION(BlueprintCallable, Category = "QR|Raid")
	void ConcludeCurrentRaid();

	UPROPERTY(BlueprintReadOnly, Category = "QR|Raid|State")
	bool bRaidActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Raid|State")
	EQRRaidTier ActiveTier = EQRRaidTier::Skirmish;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	float Accum = 0.0f;

	// Computes the [0..1] pressure scalar from world state.
	float ComputePressure() const;

	// Picks a tier given a pressure scalar.
	EQRRaidTier RollTier(float Pressure) const;
};
