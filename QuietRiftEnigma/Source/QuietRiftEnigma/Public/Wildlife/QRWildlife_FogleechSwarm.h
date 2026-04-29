#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_FogleechSwarm.generated.h"

// PRD_FOGLEECH_SWARM — Area-control swarm; obscures vision and drains composure
// Biomes: ColdBasins, ShadowFens | Role: Swarm / Visibility Denial
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_FogleechSwarm : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_FogleechSwarm();

	// Radius within which the swarm applies fog/visibility penalty
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogleechSwarm")
	float FogDenialRadius = 600.0f;

	// Visibility reduction multiplier per second inside radius (0-1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogleechSwarm")
	float VisibilityDrainPerSecond = 0.08f;

	// Composure/morale drain per second while enveloped
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogleechSwarm")
	float ComposureDrainPerSecond = 3.0f;

	// Swarm disperses when exposed to bright light sources
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogleechSwarm")
	float LightDispersalThreshold = 500.0f;

	// Individual leech bleed tick
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FogleechSwarm")
	float BleedDamagePerSecond = 1.5f;

	UFUNCTION(BlueprintCallable, Category = "FogleechSwarm")
	void ApplySwarmEffects(AActor* Target, float DeltaTime);

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
