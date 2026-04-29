#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_GlasbarkTree.generated.h"

// TRE_GLASSBARK — Pale translucent bark tree with visible internal rib structure
// Biomes: BasaltShelf, MeltlineEdges | Palette: milky white, pale cyan, grey wood core
// General-purpose wood source; sheds glassy flakes when struck
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_GlasbarkTree : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_GlasbarkTree();

	// Whether this tree has been felled
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "GlasbarkTree")
	bool bIsFelled = false;

	// Glassy flake secondary drop on each axe hit (crafting ingredient)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GlasbarkTree")
	FName GlassFlakeItemId = "ITEM_GLASSBARK_FLAKE";

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GlasbarkTree")
	void Fell();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
