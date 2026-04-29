#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_NullmintNodes.generated.h"

// PLT_NULLMINT_NODES — Pebble-like nodules in dark moss; crushing releases cooling vapor
// Biomes: MossFields, ColdBasins | Palette: black moss, pale mint vapour, grey nodules
// Medicinal/nerve-soothing; reduces toxin severity and pain status
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_NullmintNodes : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_NullmintNodes();

	// Nodes per cluster (each node is an individual yield unit)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NullmintNodes")
	int32 NodeCount = 8;

	// Vapor radius when crushed on harvest
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NullmintNodes")
	float VaporRadius = 120.0f;

	// Toxin severity reduction applied in vapor radius
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NullmintNodes")
	float ToxinReductionOnHarvest = 0.3f;
};
