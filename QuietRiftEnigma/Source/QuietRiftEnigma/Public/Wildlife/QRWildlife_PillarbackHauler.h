#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_PillarbackHauler.generated.h"

// ANI_PILLARBACK_HAULER — Megafauna; stacked column vertebrae with dorsal mineral cavities
// Biomes: BasaltShelf, CraterFloors | Role: Prey/Large megafauna
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_PillarbackHauler : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_PillarbackHauler();

	// Dorsal cavities hold mineral deposits harvestable on kill
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PillarbackHauler")
	TArray<FQRWildlifeDrop> DorsalMineralDrops;

	// Requires colony-level research to hunt safely
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PillarbackHauler")
	FName RequiredResearchTag = "Research.Ecology.MegafaunaHunting";

	// Stomp AoE damage to small creatures underfoot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PillarbackHauler")
	float StompDamageRadius = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PillarbackHauler")
	float StompDamage = 60.0f;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
	virtual void OnDeath_Implementation() override;
};
