#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_LatticeBulb.generated.h"

// PLT_LATTICE_BULB — Waist-high hollow polygon bulb clusters; food/tuber source
// Biomes: BasaltShelf, MeltlineEdges | Palette: frosted glass white, smoky grey, black ribbing
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_LatticeBulb : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_LatticeBulb();

	// Number of bulbs in this cluster (affects yield)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LatticeBulb")
	int32 BulbCount = 6;

	// Click audio cue plays when bulbs move in wind
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LatticeBulb")
	bool bPlayClickAmbient = true;
};
