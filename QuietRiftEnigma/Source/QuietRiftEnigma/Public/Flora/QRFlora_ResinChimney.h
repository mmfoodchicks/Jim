#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_ResinChimney.generated.h"

// PLT_RESIN_CHIMNEY — Chimney stalks that vent resin mist and build layered collars over time
// Biomes: VentRims, SulfurRock | Palette: tar brown, amber resin, sulfur yellow accents
// Resin resource source; the collar layers represent accumulated harvestable resin
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_ResinChimney : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_ResinChimney();

	// Current collar thickness [0,1]; grows over time, depleted on harvest
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "ResinChimney")
	float CollarThickness = 0.5f;

	// Resin accumulation rate per game hour
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ResinChimney")
	float ResinAccumulationPerHour = 0.1f;

	// Resin mist applies minor heat resistance buff to nearby allies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ResinChimney")
	float ResinMistRadius = 200.0f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
