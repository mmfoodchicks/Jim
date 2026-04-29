#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_AsterbarkTree.generated.h"

// TRE_ASTERBARK — Dark hardwood with star-like mineral flecks that glitter under light
// Biomes: MagneticRidges, HighRims | Palette: black-brown, silver sparkle, iron red undertones
// Premium late-tier wood; mineral fleck secondary drop for research crafting
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_AsterbarkTree : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_AsterbarkTree();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AsterbarkTree")
	bool bIsFelled = false;

	// Star mineral fleck secondary drop (research ingredient)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AsterbarkTree")
	FName MineralFleckItemId = "ITEM_ASTERBARK_FLECK";

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AsterbarkTree")
	int32 FleckDropCountMin = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AsterbarkTree")
	int32 FleckDropCountMax = 6;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AsterbarkTree")
	void Fell();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
