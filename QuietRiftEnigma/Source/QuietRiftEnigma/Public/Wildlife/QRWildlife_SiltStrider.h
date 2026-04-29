#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_SiltStrider.generated.h"

// ANI_SILT_STRIDER — Wetland herbivore; three spring legs + buoyant belly sac
// Biomes: ShallowFens, WetBasins | Role: Prey/Herd
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_SiltStrider : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_SiltStrider();

	// Can traverse shallow water without speed penalty
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SiltStrider")
	bool bAquaticTraversal = true;

	// Spring-leg burst speed when startled
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SiltStrider")
	float BurstFleeSpeed = 900.0f;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
