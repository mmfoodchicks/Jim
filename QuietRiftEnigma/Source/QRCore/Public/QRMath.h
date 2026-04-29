#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRMath.generated.h"

UCLASS()
class QRCORE_API UQRMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ── General ──────────────────────────────

	// Multiplicative stacking for micro-research bonuses: Π(1 + bonus_i)
	// Pass each bonus as an element of BonusArray (e.g. 0.05 = +5%)
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float MultiplicativeStack(float BaseValue, float FactorPerStack, int32 NumStacks);

	// Variant that takes an explicit array of bonus scalars: FinalScalar = Π(1 + bonuses[i])
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float MicroResearchFinalScalar(const TArray<float>& BonusArray);

	// Clamp to 0..Max with epsilon guard
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float ClampRate(float Value, float Max);

	// ── Morale ───────────────────────────────

	// Interpolate morale toward target using exponential decay
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float MoraleDecay(float Current, float Target, float DecayRate, float DeltaTime);

	// Eff_M = Clamp(0.50 + 0.007 * MoraleIndex, 0.0, 1.2)
	// Colony-wide efficiency multiplier driven by Morale Index
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float ColonyEfficiencyMultiplier(float MoraleIndex);

	// ── Leader Stats (v1.4) ──────────────────

	// LeaderBuff = Clamp(1 + 0.02*L + 0.01*S, 1.0, 1.35)
	UFUNCTION(BlueprintPure, Category = "QR|Math|Leader")
	static float LeaderBuffScalar(float L, float S);

	// LeaderLevel = floor(1 + 4 * ((0.6*L + 0.4*S) / 10)), range 1..5
	UFUNCTION(BlueprintPure, Category = "QR|Math|Leader")
	static int32 ComputeLeaderLevel(float L, float S);

	// IssueEscalationScore = BlockerSeverity * max(BlockerDurationHours - GuidanceDelayHours, 0) * LeaderAwarenessMult
	// Quest is issued when score >= 100
	UFUNCTION(BlueprintPure, Category = "QR|Math|Leader")
	static float IssueEscalationScore(float BlockerSeverity, float BlockerDurationHours,
		float GuidanceDelayHours, float LeaderAwarenessMult);

	// ── Encumbrance (v1.17) ──────────────────

	// CarryCapacityKg = 20 + 6 * STR
	UFUNCTION(BlueprintPure, Category = "QR|Math|Inventory")
	static float CarryCapacityKg(int32 STR);

	// EncumbranceRatio = CurrentWeightKg / MaxCarryWeightKg
	UFUNCTION(BlueprintPure, Category = "QR|Math|Inventory")
	static float EncumbranceRatio(float CurrentWeightKg, float MaxCarryWeightKg);

	// ShoulderStackMax = Clamp(1 + floor(STR / 4), 1, 3)
	UFUNCTION(BlueprintPure, Category = "QR|Math|Inventory")
	static int32 ShoulderStackMax(int32 STR);

	// ── Logistics / Depot ────────────────────

	// StorageDeficitMod = Clamp(TargetStockpile / max(CurrentStockpile, 1), 0.5, 3.0)
	UFUNCTION(BlueprintPure, Category = "QR|Math|Logistics")
	static float StorageDeficitMod(float TargetStockpile, float CurrentStockpile);

	// ── Ecology / Agriculture (v1.17) ────────

	// CrossContamExposure = ToxicSoil + InfectedWater + SporeLoad + FarmerCrossContamScore
	// Mutation triggers when result >= CropMutationThreshold (default 4.0)
	UFUNCTION(BlueprintPure, Category = "QR|Math|Ecology")
	static float CrossContamExposure(float ToxicSoil, float InfectedWater, float SporeLoad,
		float FarmerCrossContamScore);

	// ── Combat / Raid ────────────────────────

	// Compute opportunity score for raid AI
	UFUNCTION(BlueprintPure, Category = "QR|Math|Combat")
	static float ComputeRaidOpportunityScore(float BaseRisk, float WallHealth, float LightLevel,
		float LivestockValue, float NoiseFactor, float NightMult);

	// ── Food ─────────────────────────────────

	// Apply spoil rate over delta time, return new spoil percentage [0,1]
	UFUNCTION(BlueprintPure, Category = "QR|Math|Food")
	static float AdvanceSpoil(float CurrentSpoil, float SpoilRatePerHour, float GameHoursElapsed);
};
