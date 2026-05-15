#pragma once

#include "CoreMinimal.h"
#include "QRBiomeProfile.h"
#include "QRWorldGenTypes.generated.h"

/**
 * Habitat / context tags that overlay macro biomes per Visual World
 * Bible §3. A cell can carry zero or more of these via bitflags.
 * Cave / AbandonedStructure / Pen drive POI eligibility, ambient
 * audio, and predator-density modifiers.
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EQRHabitatFlag : uint8
{
	None                = 0          UMETA(Hidden),
	Caves               = 1 << 0     UMETA(DisplayName = "Caves"),
	AbandonedStructure  = 1 << 1     UMETA(DisplayName = "Abandoned Structure"),
	Pen                 = 1 << 2     UMETA(DisplayName = "Pen"),
	NearCorpses         = 1 << 3     UMETA(DisplayName = "Near Corpses"),
	NearWaste           = 1 << 4     UMETA(DisplayName = "Near Waste"),
};
ENUM_CLASS_FLAGS(EQRHabitatFlag);


/**
 * One discrete biome-grid cell produced by UQRWorldGenSubsystem.
 *
 * Grid is square, centered at world origin, with GridW × GridH cells
 * each `CellSizeMeters` wide (default 128m per Master GDD §4
 * BiomeCellSizeMeters). MacroBiome is the canonical biome name
 * (BasaltShelf / WindPlains / MagneticRidges / …); MicroBiome layers
 * on top for vent rims, steam pockets, ice caves, etc.
 *
 * Lookups are O(1) via GetCellAt(WorldPos).
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRWorldCell
{
	GENERATED_BODY()

	// Grid coordinates (cell-space, not world-space).
	UPROPERTY(BlueprintReadOnly) int32 X = 0;
	UPROPERTY(BlueprintReadOnly) int32 Y = 0;

	// Canonical macro biome tag — one of the 14 in Master GDD §4.
	UPROPERTY(BlueprintReadOnly) FName MacroBiome;

	// Optional micro overlay (VentRims, SteamVents, IronBasalt, etc.).
	UPROPERTY(BlueprintReadOnly) FName MicroBiome;

	// Habitat bitflags (Caves / AbandonedStructure / Pen / …).
	UPROPERTY(BlueprintReadOnly) uint8 HabitatFlags = 0;

	// Depth band — Surface / Mid / Deep / Remnant.
	UPROPERTY(BlueprintReadOnly) EQRDepthBand DepthBand = EQRDepthBand::Surface;

	// Distance from world center, kilometers. Drives DepthBand.
	UPROPERTY(BlueprintReadOnly) float DistKm = 0.0f;
};


/**
 * POI placement plan emitted by the worldgen pass. Designer / spawner
 * code consumes the array to actually spawn AQRWorldItem / wreck /
 * remnant actors at the chosen world locations.
 *
 * Placement is deterministic given the same WorldSeed.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRPOIPlacement
{
	GENERATED_BODY()

	// e.g. "RemnantSite", "ArmoryWreck", "FactionSatellite", "MedBayWreck"
	UPROPERTY(BlueprintReadOnly) FName ArchetypeId;

	UPROPERTY(BlueprintReadOnly) FVector WorldLocation = FVector::ZeroVector;

	// Used for min-spacing checks; archetype-specific.
	UPROPERTY(BlueprintReadOnly) float RadiusCm = 500.0f;

	// Biome and band at the placement spot — handy for spawner queries.
	UPROPERTY(BlueprintReadOnly) FName BiomeTag;
	UPROPERTY(BlueprintReadOnly) EQRDepthBand DepthBand = EQRDepthBand::Surface;
};


/**
 * Canonical biome-pool-per-band lookup. Designer can override per
 * project; the defaults follow Master GDD §4.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRBiomeBandPool
{
	GENERATED_BODY()

	// Biomes the worldgen may assign within this band. Picked via
	// weighted Voronoi-like distribution on macro noise.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Biomes;
};
