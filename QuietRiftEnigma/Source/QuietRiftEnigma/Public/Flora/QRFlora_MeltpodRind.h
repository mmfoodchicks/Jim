#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_MeltpodRind.generated.h"

// PLT_MELTPOD_RIND — Ceramic-looking pods fused to warm rock; seams part at dusk
// Biomes: ThermalCracks, SteamVents | Palette: warm beige, ember orange seams, wet amber gel
// High-calorie but risky heat-biome food; harvesting at peak ripeness requires timing
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_MeltpodRind : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_MeltpodRind();

	// Whether seams are currently open (dusk window) — peak yield period
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "MeltpodRind")
	bool bSeamsOpen = false;

	// Calorie multiplier when harvested during seam-open window
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MeltpodRind")
	float PeakRipenessCalorieMultiplier = 1.8f;

	// Heat damage to unprotected hands when harvesting from active vent surface
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MeltpodRind")
	float HarvestHeatDamage = 8.0f;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
