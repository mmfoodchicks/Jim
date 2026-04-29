#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_IronbrineCups.generated.h"

// PLT_IRONBRINE_CUPS — Cup-shaped saline growths collecting rust-colored mineral brine
// Biomes: WetBasins, IronBasalt | Palette: oxide red, brine green, white salt bloom
// Mineral-chemical ingredient for crafting and research
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_IronbrineCups : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_IronbrineCups();

	// Current brine fill level [0,1] — fills over time, drains on harvest
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "IronbrineCups")
	float BrineFillLevel = 0.5f;

	// Hours for cup to refill from empty
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronbrineCups")
	float RefillTimeHours = 6.0f;

	// Brine can be consumed as partial mineral supplement (low-gravity drip mechanic)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IronbrineCups")
	FName BrineItemId = "ITEM_IRONBRINE";

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
