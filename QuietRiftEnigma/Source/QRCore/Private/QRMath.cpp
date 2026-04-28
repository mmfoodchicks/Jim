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
