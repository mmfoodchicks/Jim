#include "QRMath.h"

float UQRMath::MultiplicativeStack(float BaseValue, float FactorPerStack, int32 NumStacks)
{
	float Multiplier = 1.0f;
	for (int32 i = 0; i < NumStacks; ++i)
	{
		Multiplier *= (1.0f + FactorPerStack);
	}
	return BaseValue * Multiplier;
}

float UQRMath::ClampRate(float Value, float Max)
{
	return FMath::Clamp(Value, 0.0f, Max);
}

float UQRMath::MoraleDecay(float Current, float Target, float DecayRate, float DeltaTime)
{
	return FMath::FInterpTo(Current, Target, DeltaTime, DecayRate);
}

float UQRMath::ComputeRaidOpportunityScore(float BaseRisk, float WallHealth, float LightLevel,
	float LivestockValue, float NoiseFactor, float NightMult)
{
	// Lower wall health and higher noise/livestock raise score
	float WallFactor     = (1.0f - FMath::Clamp(WallHealth, 0.0f, 1.0f)) * 30.0f;
	float LightFactor    = (1.0f - FMath::Clamp(LightLevel, 0.0f, 1.0f)) * 20.0f;
	float LivestockFactor = FMath::Clamp(LivestockValue / 100.0f, 0.0f, 1.0f) * 25.0f;
	float NoiseFact      = FMath::Clamp(NoiseFactor, 0.0f, 1.0f) * 15.0f;

	float Score = (BaseRisk + WallFactor + LightFactor + LivestockFactor + NoiseFact) * NightMult;
	return FMath::Clamp(Score, 0.0f, 100.0f);
}

float UQRMath::AdvanceSpoil(float CurrentSpoil, float SpoilRatePerHour, float GameHoursElapsed)
{
	return FMath::Clamp(CurrentSpoil + SpoilRatePerHour * GameHoursElapsed, 0.0f, 1.0f);
}

float UQRMath::MicroResearchFinalScalar(const TArray<float>& BonusArray)
{
	float Scalar = 1.0f;
	for (float Bonus : BonusArray)
	{
		Scalar *= (1.0f + Bonus);
	}
	return Scalar;
}

float UQRMath::ColonyEfficiencyMultiplier(float MoraleIndex)
{
	return FMath::Clamp(0.50f + 0.007f * MoraleIndex, 0.0f, 1.2f);
}

float UQRMath::LeaderBuffScalar(float L, float S)
{
	// Clamp inputs first so NaN from corrupted save data can't propagate
	const float SafeL = FMath::IsFinite(L) ? FMath::Clamp(L, 0.0f, 10.0f) : 5.0f;
	const float SafeS = FMath::IsFinite(S) ? FMath::Clamp(S, 0.0f, 10.0f) : 5.0f;
	return FMath::Clamp(1.0f + 0.02f * SafeL + 0.01f * SafeS, 1.0f, 1.35f);
}

int32 UQRMath::ComputeLeaderLevel(float L, float S)
{
	const float SafeL = FMath::IsFinite(L) ? FMath::Clamp(L, 0.0f, 10.0f) : 5.0f;
	const float SafeS = FMath::IsFinite(S) ? FMath::Clamp(S, 0.0f, 10.0f) : 5.0f;
	const float Combined = 0.6f * SafeL + 0.4f * SafeS;
	return FMath::Clamp(FMath::FloorToInt(1.0f + 4.0f * (Combined / 10.0f)), 1, 5);
}

float UQRMath::IssueEscalationScore(float BlockerSeverity, float BlockerDurationHours,
	float GuidanceDelayHours, float LeaderAwarenessMult)
{
	const float SafeSeverity = FMath::Clamp(BlockerSeverity, 0.0f, 10.0f);
	const float SafeMult     = FMath::Clamp(LeaderAwarenessMult, 0.5f, 2.0f);
	const float ActiveHours  = FMath::Max(BlockerDurationHours - GuidanceDelayHours, 0.0f);
	return SafeSeverity * ActiveHours * SafeMult;
}

float UQRMath::CarryCapacityKg(int32 STR)
{
	// Clamp STR >= 0 so negative values don't produce a negative weight cap
	return 20.0f + 6.0f * static_cast<float>(FMath::Max(STR, 0));
}

float UQRMath::EncumbranceRatio(float CurrentWeightKg, float MaxCarryWeightKg)
{
	// Return 1.0 for any non-positive cap so callers treat it as fully encumbered
	if (MaxCarryWeightKg <= 0.0f) return 1.0f;
	return FMath::Clamp(CurrentWeightKg / MaxCarryWeightKg, 0.0f, 2.0f);
}

int32 UQRMath::ShoulderStackMax(int32 STR)
{
	return FMath::Clamp(1 + FMath::FloorToInt(static_cast<float>(FMath::Max(STR, 0)) / 4.0f), 1, 3);
}

float UQRMath::StorageDeficitMod(float TargetStockpile, float CurrentStockpile)
{
	// Reject negative target; a negative stockpile target is a logic error, treat as no deficit
	const float SafeTarget = FMath::Max(TargetStockpile, 0.0f);
	const float Denom      = FMath::Max(CurrentStockpile, 1.0f);
	return FMath::Clamp(SafeTarget / Denom, 0.5f, 3.0f);
}

float UQRMath::CrossContamExposure(float ToxicSoil, float InfectedWater, float SporeLoad,
	float FarmerCrossContamScore)
{
	// Clamp each component >= 0 and cap total to a reasonable maximum (prevents runaway mutation)
	const float Sum = FMath::Max(ToxicSoil, 0.0f)
	                + FMath::Max(InfectedWater, 0.0f)
	                + FMath::Max(SporeLoad, 0.0f)
	                + FMath::Max(FarmerCrossContamScore, 0.0f);
	return FMath::Clamp(Sum, 0.0f, 10.0f);
}
