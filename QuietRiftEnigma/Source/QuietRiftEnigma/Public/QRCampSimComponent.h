#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRCampSimComponent.generated.h"

class AQRFactionCamp;


/**
 * The persistent state of a single enemy AI camp. Grows over time;
 * sometimes raids the player. Master GDD §30: "Enemy factions operate
 * under the same pressures as the player colony — shortages, morale,
 * leadership, raids, flight, merging, collapse."
 *
 * Numbers are deliberately abstract — a camp doesn't simulate
 * individual NPCs, just aggregates. The actual raid party spawns
 * physical AQRNPCActor instances on launch.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRCampState
{
	GENERATED_BODY()

	// Civilian + soldier headcount combined.
	UPROPERTY(BlueprintReadOnly) int32 Population = 5;

	// Subset of population trained for combat. Spent on raids.
	UPROPERTY(BlueprintReadOnly) int32 MilitaryStrength = 2;

	// Resource pool. Accumulates over time, spent on training new
	// military or growing population.
	UPROPERTY(BlueprintReadOnly) float Resources = 0.0f;

	// 0..1 hostility toward the player. 0 = neutral, 1 = open war.
	// Modified by player actions (faction-component diplomacy events).
	UPROPERTY(BlueprintReadOnly) float Hostility = 0.4f;

	// Game-hours since the camp's last successful raid launch.
	UPROPERTY(BlueprintReadOnly) float HoursSinceLastRaid = 0.0f;

	// Game-hours since the last failed raid (party wiped). Camps that
	// just lost their force back off.
	UPROPERTY(BlueprintReadOnly) float HoursSinceLastDefeat = 9999.0f;
};


/**
 * One launched raid party. The camp emits this when it decides to
 * attack; the spawner system reads it to materialize the NPCs.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRRaidPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName SourceCampId;
	UPROPERTY(BlueprintReadOnly) FVector OriginLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) int32 PartySize = 3;
	UPROPERTY(BlueprintReadOnly) float HostilityAtLaunch = 0.5f;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaidLaunched, FQRRaidPlan, Plan);


/**
 * Per-camp autonomous simulation. Ticks game-hours, grows population,
 * trains military, accumulates resources, eventually rolls a raid
 * decision. Lives on AQRFactionCamp as a default subobject.
 *
 * Grand-strategy style — each camp ticks independently. Multiple
 * camps can raid concurrently. Player can't predict the wave from
 * a centralized scheduler.
 *
 * AQRGameMode drives the sim by calling AdvanceGameHours on each
 * camp's component every world-tick (camps find themselves in the
 * world via TActorIterator and the game mode walks them all).
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRCampSimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRCampSimComponent();

	// ── Config ────────────────────────────────────────────────────

	// Camp identity tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp")
	FName CampId;

	// Population growth rate per game-day. Modified by hostility +
	// resources (resource-starved camps don't grow).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp",
		meta = (ClampMin = "0", ClampMax = "10"))
	float BasePopGrowthPerDay = 0.5f;

	// Resources gained per game-day from territory + foraging.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp",
		meta = (ClampMin = "0", ClampMax = "50"))
	float ResourceIncomePerDay = 4.0f;

	// Resources spent to train one new military unit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp",
		meta = (ClampMin = "1", ClampMax = "100"))
	float ResourcesPerSoldier = 8.0f;

	// Population cap for this camp. Beyond this, growth stalls and the
	// camp may "split" into a new camp (future feature).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp",
		meta = (ClampMin = "5", ClampMax = "200"))
	int32 PopulationCap = 30;

	// Minimum military strength required before the camp will consider
	// launching a raid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Raid",
		meta = (ClampMin = "1", ClampMax = "50"))
	int32 RaidMilitaryThreshold = 6;

	// Hostility threshold below which the camp won't raid regardless
	// of strength. Keeps neutral camps from attacking even if they
	// have soldiers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Raid",
		meta = (ClampMin = "0", ClampMax = "1"))
	float RaidHostilityThreshold = 0.55f;

	// Minimum game-hours between raid launches per camp.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Raid",
		meta = (ClampMin = "1", ClampMax = "240"))
	float RaidCooldownHours = 36.0f;

	// Defeat penalty (game-hours added back to cooldown after a wipe).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Raid",
		meta = (ClampMin = "0", ClampMax = "240"))
	float DefeatExtraCooldownHours = 48.0f;

	// Fraction of military to commit per raid. 0.6 = send 60% of
	// available soldiers, keep the rest at home.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Raid",
		meta = (ClampMin = "0.1", ClampMax = "1"))
	float MilitaryRaidCommitment = 0.6f;

	// ── Leadership influence ──────────────────────────────────────
	// The camp's effective leader skill drives raid quality:
	//   Low leadership  (< 3): camp launches early, ignores conditions,
	//                          under-prepared. Player gets hit by
	//                          poorly-staged raids in bad weather /
	//                          mid-day / outnumbered.
	//   High leadership (> 7): camp waits for the right moment —
	//                          full military strength, nighttime,
	//                          active weather event, player far from
	//                          base. Hits hard when it hits.
	//
	// Read at decision time from the owning actor's
	// UQRLeaderComponent::LeadershipAptitude (0..10). Falls back to
	// FallbackLeadership if no leader component is found.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Leadership",
		meta = (ClampMin = "0", ClampMax = "10"))
	float FallbackLeadership = 5.0f;

	// High-skill leaders raise the bar — effective MilitaryThreshold
	// = base × (0.5 + 0.1 × Leadership). At L=0 → 0.5×, L=5 → 1.0×,
	// L=10 → 1.5×.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Leadership",
		meta = (ClampMin = "0", ClampMax = "1"))
	float LeadershipThresholdInfluence = 1.0f;

	// Leaders above this skill prefer night raids and active weather.
	// They'll hold a raid even when ready in plain daylight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Camp|Leadership",
		meta = (ClampMin = "0", ClampMax = "10"))
	float ConditionGatedSkillFloor = 6.5f;

	// ── State ─────────────────────────────────────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "QR|Camp|State")
	FQRCampState State;

	UPROPERTY(BlueprintAssignable, Category = "QR|Camp|Events")
	FOnRaidLaunched OnRaidLaunched;

	// ── API ───────────────────────────────────────────────────────

	// Advance the sim by N game-hours. Called by AQRGameMode each
	// world tick (DeltaGameHours = real-DeltaTime × 24 / DayLengthRealSeconds).
	UFUNCTION(BlueprintCallable, Category = "QR|Camp")
	void AdvanceGameHours(float DeltaGameHours);

	// Apply hostility delta from player actions (treaty broken, NPC
	// killed, faction quest accepted, etc.). Clamped [0, 1].
	UFUNCTION(BlueprintCallable, Category = "QR|Camp")
	void ModifyHostility(float Delta);

	// External signal that a launched raid was defeated. Camp gains
	// cooldown + loses military it was banking on returning.
	UFUNCTION(BlueprintCallable, Category = "QR|Camp")
	void ReportRaidDefeated(int32 PartySize);

	// External signal of a successful raid (player damaged, resources
	// taken). Camp gains a small Hostility cooldown and resource boost.
	UFUNCTION(BlueprintCallable, Category = "QR|Camp")
	void ReportRaidSuccessful(int32 SurvivingMilitary, float LootedResources);

private:
	void TryDecideRaid();
	FVector FindRaidTargetLocation() const;
	float   GetEffectiveLeadership() const;
	bool    AreConditionsFavorable() const;
};
