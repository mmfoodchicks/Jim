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


/**
 * Remnant wake state per Master GDD §13 / GAME_OVERVIEW.md.
 * AQRRemnantSite drives this FSM in response to player Rift research.
 */
UENUM(BlueprintType)
enum class EQRRemnantWakeState : uint8
{
	Dormant   UMETA(DisplayName = "Dormant"),
	Stirring  UMETA(DisplayName = "Stirring"),
	Active    UMETA(DisplayName = "Active"),
	Hostile   UMETA(DisplayName = "Hostile"),
	Subsiding UMETA(DisplayName = "Subsiding"),
};


/**
 * Remnant structure type (Master GDD §13). Drives mesh selection +
 * research yield + wake-state escalation curve.
 */
UENUM(BlueprintType)
enum class EQRRemnantKind : uint8
{
	SignalSpire       UMETA(DisplayName = "Signal Spire"),
	PowerCore         UMETA(DisplayName = "Power Core"),
	DataArchive       UMETA(DisplayName = "Data Archive"),
	ResonanceChamber  UMETA(DisplayName = "Resonance Chamber"),
};


/**
 * Single loot entry for a hardcoded crash-site wreck. ItemId references
 * a UQRItemDefinition by id; Quantity / DurabilityMin/Max randomize
 * within the placement seed for deterministic-shareable loot.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRCrashLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MinQty = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 MaxQty = 1;

	// 0..1, used by spawner to roll inclusion per entry. 1.0 = always.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float SpawnChance = 1.0f;
};


/**
 * Loot template applied to a hardcoded crash-site wreck. Keyed by
 * archetype id (ArmoryWreck / MedBayWreck / …) in the spawner.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRCrashLootTemplate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FQRCrashLootEntry> Entries;
};


/**
 * DataTable row that describes one POI archetype end-to-end. When the
 * spawner has a POIArchetypeTable set, it reads these rows instead of
 * using its hardcoded TMap defaults — designer can author per-project
 * POI behavior without recompiling.
 *
 * RowName matches the archetype id used by UQRWorldGenSubsystem's POI
 * placements (ArmoryWreck / RemnantSite / FactionSatellite / …).
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRPOIArchetypeRow : public FTableRowBase
{
	GENERATED_BODY()

	// Actor class spawned at each placement of this archetype.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AActor> ActorClass;

	// Optional mesh library — spawner picks one at random per
	// placement so the same archetype isn't visually identical.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftObjectPtr<class UStaticMesh>> MeshOptions;

	// For crash-site archetypes, this overrides the hardcoded loot
	// template. Empty Entries → fall back to spawner default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FQRCrashLootTemplate LootTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;
};
