#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_SmokebarkTree.generated.h"

// PLT_SMOKE_TREE_001 — Core lumber/resin tree; construction base
// Priority: Very High | Biomes: forest, foothill, town edge
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_SmokebarkTree : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_SmokebarkTree();

	// Whether this tree has been felled (changes hitbox and future yield)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "SmokebarkTree")
	bool bIsFelled = false;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SmokebarkTree")
	void Fell(); // Requires axe; dramatically increases log yield

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
