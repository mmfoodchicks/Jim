#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRRaidScheduler.generated.h"

class UQRColonyStateComponent;

// A single raid wave definition
USTRUCT(BlueprintType)
struct QRCOMBATTHREAT_API FQRRaidWave
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 WaveNumber = 1;
	UPROPERTY(BlueprintReadOnly) int32 AttackerCount = 5;
	UPROPERTY(BlueprintReadOnly) EQRRaidExperienceTier ExperienceTier = EQRRaidExperienceTier::Inexperienced;
	UPROPERTY(BlueprintReadOnly) FGameplayTag FactionTag;
	UPROPERTY(BlueprintReadOnly) float ApproachDelaySeconds = 30.0f;

	// Behavior overrides for this tier
	UPROPERTY(BlueprintReadOnly) bool bUsesFlankRoutes = false;
	UPROPERTY(BlueprintReadOnly) bool bTargetsLivestock = false;
	UPROPERTY(BlueprintReadOnly) bool bTargetsLights = false;
	UPROPERTY(BlueprintReadOnly) bool bCallsForRetreat = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaidStarted, FQRRaidWave, Wave);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaidEnded, bool, bDefendersWon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRaidThreatChanged, float, NewScore, bool, bIsNight);

// Controls raid scheduling, opportunity scoring, and wave spawning
UCLASS(BlueprintType, Blueprintable)
class QRCOMBATTHREAT_API AQRRaidScheduler : public AActor
{
	GENERATED_BODY()

public:
	AQRRaidScheduler();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Raid")
	float BaseRisk = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Raid")
	float NightRiskMultiplier = 2.5f;

	// Minimum game-hours between raids
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Raid")
	float MinRaidCooldownHours = 24.0f;

	// Opportunity score threshold to trigger a raid
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Raid")
	float RaidTriggerThreshold = 60.0f;

	// ── Runtime ──────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Raid")
	float CurrentOpportunityScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Raid")
	float HoursSinceLastRaid = 72.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Raid")
	bool bRaidInProgress = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Raid")
	FQRRaidWave ActiveWave;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Raid|Events")
	FOnRaidStarted OnRaidStarted;

	UPROPERTY(BlueprintAssignable, Category = "Raid|Events")
	FOnRaidEnded OnRaidEnded;

	UPROPERTY(BlueprintAssignable, Category = "Raid|Events")
	FOnRaidThreatChanged OnThreatChanged;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid")
	void UpdateThreatScore(float WallHealth, float LightLevel, float LivestockValue, float NoiseFactor, bool bIsNight);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid")
	void TriggerRaid(FGameplayTag FactionTag, EQRRaidExperienceTier Tier);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Raid")
	void EndRaid(bool bDefendersWon);

	UFUNCTION(BlueprintNativeEvent, Category = "Raid")
	void SpawnRaidWave(const FQRRaidWave& Wave);
	virtual void SpawnRaidWave_Implementation(const FQRRaidWave& Wave) {}

	UFUNCTION(BlueprintPure, Category = "Raid")
	bool IsRaidReady() const;

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	float RaidCheckAccumulator = 0.0f;
	const float RaidCheckInterval = 300.0f; // Check every 5 real-minutes

	EQRRaidExperienceTier DetermineRaidTier() const;
};
