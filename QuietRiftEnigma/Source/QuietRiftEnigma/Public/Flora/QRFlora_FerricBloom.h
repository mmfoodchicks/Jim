#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_FerricBloom.generated.h"

// PLT_FERRIC_BLOOM — Metallic petal forms with magnetic sheen; orient toward local fields
// Biomes: IronBasalt, MagneticRidges | Palette: dark iron red, gunmetal, black stems
// Late-mid resource and research plant; drops magnetic components for tech research
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_FerricBloom : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_FerricBloom();

	// Whether petals are currently oriented toward a magnetic event
	UPROPERTY(BlueprintReadOnly, Category = "FerricBloom")
	bool bOrientedToMagneticField = false;

	// Magnetic field orientation amplifies yield quality
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FerricBloom")
	float MagneticYieldBonus = 1.5f;
};
