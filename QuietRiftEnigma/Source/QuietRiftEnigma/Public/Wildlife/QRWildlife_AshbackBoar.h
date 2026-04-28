#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_AshbackBoar.generated.h"

// ANM_ASH_BOAR_001 — Early danger; food thief; camp nuisance
// Biomes: burn zone, forest edge, junkyard, abandoned farm
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_AshbackBoar : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_AshbackBoar();

	// Distance within which boar will steal food from unsecured depots
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boar")
	float FoodTheftRadius = 600.0f;

	// Damage dealt per charge attack
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boar")
	float ChargeDamage = 25.0f;

	UFUNCTION(BlueprintCallable, Category = "Boar")
	bool TryStealFood(AActor* FoodDepot);

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
