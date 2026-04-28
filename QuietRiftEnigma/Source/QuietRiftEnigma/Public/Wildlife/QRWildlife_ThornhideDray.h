#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_ThornhideDray.generated.h"

// ANM_THORN_DRAY_001 — Scavenger pressure; corpse/food cleanup; pack harassment
// Biomes: scrubland, town outskirts, trash piles, carcass site
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_ThornhideDray : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_ThornhideDray();

	// Drays are attracted to carcasses and unsecured food within this radius
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dray")
	float ScavengeDetectionRadius = 3000.0f;

	// Pack size when multiple Drays spawn together
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dray")
	int32 PackSize = 3;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
