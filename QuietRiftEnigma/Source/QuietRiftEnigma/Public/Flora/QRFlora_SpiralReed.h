#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_SpiralReed.generated.h"

// PLT_SPIRAL_REED — Corkscrew reeds with banded fibers and luminous tips
// Biomes: WindPlains, ShallowFens | Palette: muted straw, soft teal glow, damp brown root beds
// Fiber + modest food source; patches create directional wind flow visual cues
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_SpiralReed : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_SpiralReed();

	// Reed patch density (affects fiber yield)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpiralReed")
	int32 ReedCount = 12;

	// Whether tips are bioluminescent (night navigation aid)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SpiralReed")
	bool bBioluminescentTips = true;
};
