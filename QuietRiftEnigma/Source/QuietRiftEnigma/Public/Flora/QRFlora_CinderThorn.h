#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_CinderThorn.generated.h"

// PLT_CINDER_THORN — Low heat-baked black shrub with ember-red thorns and ash pods
// Biomes: VentRims, CraterWalls | Palette: charcoal black, ember red, ash grey
// Visual warning plant: painful harvest, requires protective gloves to safely collect
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_CinderThorn : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_CinderThorn();

	// Thorn pierce damage to unprotected harvesters
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CinderThorn")
	float ThornPierceDamage = 15.0f;

	// Whether harvester has glove protection (set by equipment check)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CinderThorn")
	bool bRequiresProtectiveGloves = true;

	// Ash pods drop a toxic dust cloud when pods are broken
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CinderThorn")
	float AshPodToxinRadius = 150.0f;

	virtual void OnHarvest_Implementation(AActor* Harvester) override;
};
