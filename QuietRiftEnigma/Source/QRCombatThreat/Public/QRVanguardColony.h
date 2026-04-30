#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRVanguardColony.generated.h"

class UQRFactionComponent;
class AQRSatelliteOutpost;
class AQRRaidScheduler;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConcordatRaidCapacityChanged);

// The Vanguard Concordat — the hardcoded primary antagonist colony.
//
// Lore: Director Harlan Voss arrived on Tharsis IV six years before the Meridian's Threshold
// as head of the Colonial Authority's classified advance mission. He found the Quiet Rift,
// claimed the moon as CA property, and has been suppressing all outside knowledge of what's
// here while extracting Progenitor technology for the Authority's profit.
//
// This actor is placed once by the level designer near the Rift entrance. World generation
// does not move or duplicate it. Satellite outposts register with it at BeginPlay and scale
// their difficulty from their distance to this actor's location.
UCLASS(BlueprintType, Blueprintable)
class QRCOMBATTHREAT_API AQRVanguardColony : public AActor
{
	GENERATED_BODY()

public:
	AQRVanguardColony();

	// ── Identity ─────────────────────────────
	// Always "The Concordat" in normal gameplay; exposed so Blueprint can localise if needed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Concordat")
	FText ColonyName = FText::FromString(TEXT("The Concordat"));

	// The named antagonist leader. Named NPC spawned by Blueprint subclass.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Concordat")
	FText LeaderName = FText::FromString(TEXT("Director Harlan Voss"));

	// Prevents world generation from moving or removing this actor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Concordat")
	bool bIsHardcoded = true;

	// ── Faction ───────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UQRFactionComponent> FactionComp;

	// ── Raid Capacity ─────────────────────────
	// Each surviving satellite outpost contributes 1 raid slot.
	// When outposts fall, the Concordat's ability to pressure the player colony diminishes.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Concordat")
	int32 ActiveOutpostCount = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Concordat")
	int32 TotalOutpostCount = 0;

	// The Concordat itself always retains at least this many direct raid actions,
	// regardless of how many satellites have been destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Concordat")
	int32 ConcordatBaseRaidSlots = 2;

	// Minimum game-hours between Concordat-direct raids (satellites have their own cooldowns)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Concordat")
	float DirectRaidCooldownHours = 72.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Concordat")
	float HoursSinceLastDirectRaid = 0.0f;

	// ── Voss Hostility Escalation ─────────────
	// Rises each time the player attacks an outpost or advances Rift research past a threshold.
	// High escalation causes the Concordat to issue direct Fanatic-tier raids rather than
	// delegating to satellites.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Concordat")
	float HostilityScore = 0.0f;

	// ── Registered Outposts ───────────────────
	UPROPERTY(BlueprintReadOnly, Category = "Concordat")
	TArray<TObjectPtr<AQRSatelliteOutpost>> RegisteredOutposts;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Concordat|Events")
	FOnConcordatRaidCapacityChanged OnRaidCapacityChanged;

	// ── Interface ────────────────────────────
	// Called by AQRSatelliteOutpost::BeginPlay() to self-register.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Concordat")
	void RegisterOutpost(AQRSatelliteOutpost* Outpost);

	// Called by AQRSatelliteOutpost::SetDestroyed() when the player captures an outpost.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Concordat")
	void OnOutpostLost(AQRSatelliteOutpost* Outpost);

	// Raise hostility (player attacked outpost, researched Rift tier, stole Progenitor tech, etc.)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Concordat")
	void RaiseHostility(float Amount);

	// How many raid actions the Concordat can currently field (base + surviving satellites)
	UFUNCTION(BlueprintPure, Category = "Concordat")
	int32 GetTotalRaidCapacity() const;

	// Returns 0..1 fraction of outposts still standing — used for UI threat meter
	UFUNCTION(BlueprintPure, Category = "Concordat")
	float GetOutpostIntegrityRatio() const;

	// Issue a direct Concordat raid at Fanatic tier (bypasses satellite routing)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Concordat")
	void IssueDirectRaid(AQRRaidScheduler* Scheduler);

	// Advance the cooldown clock — call from GameMode tick with game-hours elapsed
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Concordat")
	void AdvanceTime(float GameHoursElapsed);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
