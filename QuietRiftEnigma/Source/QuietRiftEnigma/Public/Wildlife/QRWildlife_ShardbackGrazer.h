#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_ShardbackGrazer.generated.h"

// ANI_SHARDBACK_GRAZER — Bread-and-butter herd animal; overlapping ceramic back plates
// Biomes: WindPlains, MossFields | Role: Prey/Herd
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_ShardbackGrazer : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_ShardbackGrazer();

	// Ceramic plate count affects armor resist for harvesting tools
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShardbackGrazer")
	int32 PlateCount = 12;

	// Herd members panic-flee together when one is killed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ShardbackGrazer")
	float HerdFleeRadius = 2500.0f;

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
