#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRMath.generated.h"

UCLASS()
class QRCORE_API UQRMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Multiplicative stacking: each stack adds multiplier factor (e.g., 0.05 per stack)
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float MultiplicativeStack(float BaseValue, float FactorPerStack, int32 NumStacks);

	// Clamp to 0..Max with epsilon guard
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float ClampRate(float Value, float Max);

	// Interpolate morale toward target using exponential decay
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float MoraleDecay(float Current, float Target, float DecayRate, float DeltaTime);

	// Compute opportunity score for raid AI
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float ComputeRaidOpportunityScore(float BaseRisk, float WallHealth, float LightLevel,
		float LivestockValue, float NoiseFactor, float NightMult);

	// Apply spoil rate over delta time, return new spoil percentage [0,1]
	UFUNCTION(BlueprintPure, Category = "QR|Math")
	static float AdvanceSpoil(float CurrentSpoil, float SpoilRatePerHour, float GameHoursElapsed);
};
