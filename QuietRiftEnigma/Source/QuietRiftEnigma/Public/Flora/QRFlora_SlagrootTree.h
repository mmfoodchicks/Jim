#pragma once
#include "QRHarvestableBase.h"
#include "QRFlora_SlagrootTree.generated.h"

// TRE_SLAGROOT — Squat heat-scarred tree; fused root pedestal, thermal-stress bark crazing
// Biomes: ThermalCracks, CraterFloors | Palette: dark brown, black, rust orange
// Dense structural wood source; heat-resistant lumber for kiln and forge construction
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFlora_SlagrootTree : public AQRHarvestableBase
{
	GENERATED_BODY()
public:
	AQRFlora_SlagrootTree();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "SlagrootTree")
	bool bIsFelled = false;

	// Dense wood yields more logs per fell but requires heavy axe
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SlagrootTree")
	bool bRequiresHeavyAxe = true;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SlagrootTree")
	void Fell();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
