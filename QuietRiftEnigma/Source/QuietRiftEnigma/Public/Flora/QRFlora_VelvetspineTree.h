#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_VelvetspineTree.generated.h"

// TRE_VELVETSPINE — Tall trunk with soft-looking vertical ribs hiding stiff spines
// Biomes: WindPlains, RidgeShadows | Palette: deep brown, dusty mauve, pale ridges
// Fibrous long-grain construction tree; spine damage to unprotected fellers
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_VelvetspineTree : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_VelvetspineTree();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "VelvetspineTree")
	bool bIsFelled = false;

	// Spine damage per strike to ungloved hands
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VelvetspineTree")
	float SpinePierceDamage = 10.0f;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "VelvetspineTree")
	void Fell();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
