#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_RustcapFungus.generated.h"

// PLT_RUST_FUNGUS_001 — Food/medicine/risk fungus; seed-based toxicity test
// Priority: High | Biomes: rotting log, damp cave, old wood structure
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_RustcapFungus : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_RustcapFungus();

	// Randomly determined at spawn — player must research to know
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Fungus")
	bool bIsToxicVariant = false;

	// Probability this spawn is the toxic variant
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fungus")
	float ToxicVariantChance = 0.3f;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
