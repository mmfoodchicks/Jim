#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_RidgebackGrazer.generated.h"

// ANM_RIDGE_GRAZER_001 — Large food source; herd behavior; pack animal candidate
// Biomes: forest edge, grassland, road shoulder
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_RidgebackGrazer : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_RidgebackGrazer();

	// If carrying pack load, drops additional supply items
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grazer")
	bool bIsPackAnimal = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grazer")
	TArray<FQRWildlifeDrop> PackLoadDrops;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
