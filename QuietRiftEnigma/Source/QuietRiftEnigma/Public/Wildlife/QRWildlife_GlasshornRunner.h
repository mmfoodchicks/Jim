#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_GlasshornRunner.generated.h"

// ANM_GLASS_RUNNER_001 — Stealth hunting target; light food and optics resource
// Biomes: open field, sparse forest, abandoned suburb
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_GlasshornRunner : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_GlasshornRunner();

	// Noise threshold above which this animal detects and flees player
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	float FleeNoiseThreshold = 0.3f;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
