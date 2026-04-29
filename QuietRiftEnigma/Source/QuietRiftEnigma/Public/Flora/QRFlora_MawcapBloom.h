#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_MawcapBloom.generated.h"

// PLT_MAWCAP_BLOOM — Lip-like fungal cap that coughs spore bursts when disturbed
// Biomes: WetBasins, ShadowFens | Palette: rot green, fleshy beige, bruised purple underside
// Elite: PLT_MAWCAP_BLOOM_PRIME — zone-defining hazard with heavy concentric flesh rings
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_MawcapBloom : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_MawcapBloom();

	// Prime elite variant (larger, denser spore fog)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MawcapBloom")
	bool bIsPrimeElite = false;

	// Spore burst radius
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MawcapBloom")
	float SporeBurstRadius = 350.0f;

	// Spore toxin damage per second in cloud
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MawcapBloom")
	float SporePoisonDPS = 4.0f;

	// Cloud linger duration in seconds
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MawcapBloom")
	float SporeCloudDuration = 12.0f;

	// Trigger spore burst on proximity
	UFUNCTION(BlueprintCallable, Category = "MawcapBloom")
	void TriggerSporeBurst();

	virtual void OnHarvest_Implementation(AActor* Harvester) override;
};
