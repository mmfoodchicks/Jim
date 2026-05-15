#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QRWorldGenTypes.h"
#include "QRFaunaSpawnRule.generated.h"

class AQRWildlifeActor;
class AActor;

/**
 * One fauna entry — single species the spawner may roll inside a
 * biome. Either Mesh or ActorClass populates the spawn; Mesh is fast
 * but non-interactive (HISM), ActorClass spawns a real AQRWildlifeActor
 * or subclass with the AI / loot / etc.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRFaunaEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SpeciesId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AActor> ActorClass;

	// 0..1 spawn-chance roll per attempted placement. 1.0 = always.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 1.0f;

	// Group size — for herd / pack species, this is the size of each
	// cluster spawned at the chosen location. 1 = lone individual.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "30"))
	int32 MinGroupSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "30"))
	int32 MaxGroupSize = 1;

	// True for predators — spawner enforces a lower density and bigger
	// min-spacing for these so the world doesn't read as wall-to-wall
	// monsters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsPredator = false;
};


/**
 * Per-biome fauna pool. Designer authors one of these per biome
 * (BasaltShelf / WindPlains / WetBasins / …) and the worldgen
 * spawner reads them to know what species can spawn where.
 *
 * Density is scaled by the GDD §4 BiomeFaunaSpawnBudgetPerKm2 (1.0
 * default) and then multiplied by SpawnDensityMultiplier here so
 * tundra can read sparse, jungle dense, deserts almost empty.
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API UQRFaunaSpawnRule : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Fauna")
	FName BiomeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Fauna")
	TArray<FQRFaunaEntry> Entries;

	// Relative density multiplier vs. the global default (1.0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Fauna",
		meta = (ClampMin = "0", ClampMax = "5"))
	float SpawnDensityMultiplier = 1.0f;

	// Predator/prey ratio cap — predators capped at this fraction of
	// total spawns regardless of weights, so a high-weight predator
	// can't dominate the biome.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Fauna",
		meta = (ClampMin = "0", ClampMax = "1"))
	float MaxPredatorFraction = 0.20f;
};
