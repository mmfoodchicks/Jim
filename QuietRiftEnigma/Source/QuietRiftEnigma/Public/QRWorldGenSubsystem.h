#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "QRWorldGenTypes.h"
#include "QRWorldGenSubsystem.generated.h"

class UTexture2D;
class AQRWorldGenSeedActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldGenComplete);


/**
 * UWorldSubsystem that owns the per-cell biome grid and POI placement
 * plan produced by the GDD §4 worldgen pipeline.
 *
 * Pipeline (matches GDD §4):
 *   1. Derive 5 RNG streams from WorldSeed (BiomeRng / POIRng /
 *      EcologyRng / FactionRng / LootRng).
 *   2. For every cell in GridW × GridH:
 *        a. Compute world position from cell coords.
 *        b. Compute DistanceToCenter; distort by macro noise.
 *        c. Assign DepthBand based on distorted distance band.
 *        d. Assign MacroBiome by picking from the band pool weighted
 *           by macro-scale noise (Voronoi-feeling but cheap).
 *        e. Layer MicroBiome / Habitat flags from micro-scale noise.
 *   3. Place POIs in canonical order:
 *        Remnant (anchor at center + scatter in Remnant band)
 *        Faction satellites (ring in Deep band)
 *        Major crash wrecks (Surface band, spread)
 *        Minor POIs (any band, biome-restricted)
 *        Micro POIs (filler)
 *   4. Generate a minimap texture for debug / UI.
 *
 * Lookups (GetCellAt / GetBiomeAt / GetDepthBandAt) are O(1) integer
 * grid math, suitable for per-tick query from the scatter actor or
 * any other consumer.
 *
 * The subsystem is created automatically per UWorld; designer drops an
 * AQRWorldGenSeedActor in the level to drive it via a button.
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API UQRWorldGenSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── Tunables (GDD §4 canonical defaults) ──────────────────────

	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	int32 WorldSeed = 1337;

	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	float WorldMapSizeKm = 64.0f;

	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	float CellSizeMeters = 128.0f;

	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	int32 GridW = 0;

	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	int32 GridH = 0;

	// Hazard belt width in meters at the outer edge. Cells beyond
	// PlayableRadius - HazardBeltMeters carry an implicit "hazard"
	// flag the scatter system can read.
	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	float HazardBeltMeters = 5000.0f;

	// PlayerStart safe radius — no Remnant / faction capital POIs
	// place within this distance of world origin.
	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	float StartSafeRadiusMeters = 1500.0f;

	// Macro noise scale (GDD §4 BiomeMacroNoiseScale default 0.00055).
	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	float MacroNoiseScale = 0.00055f;

	// Micro noise scale (GDD §4 BiomeMicroNoiseScale default 0.0025).
	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	float MicroNoiseScale = 0.0025f;

	// Configurable per-band biome pools. Defaults populated in
	// Initialize() with the GDD §4 canonical lists; designer can
	// override on a per-level basis via the seed actor.
	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen")
	TMap<EQRDepthBand, FQRBiomeBandPool> BandPools;

	// Generated data — empty until Generate() runs.
	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen|Output")
	TArray<FQRWorldCell> Cells;

	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen|Output")
	TArray<FQRPOIPlacement> POIPlacements;

	UPROPERTY(BlueprintReadOnly, Category = "QR|WorldGen|Output")
	bool bGenerated = false;

	// ── Events ────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "QR|WorldGen|Events")
	FOnWorldGenComplete OnWorldGenComplete;

	// ── API ───────────────────────────────────────────────────────

	// Full pipeline run. Idempotent — call again with a different
	// seed for a new world. Output structs are overwritten.
	UFUNCTION(BlueprintCallable, Category = "QR|WorldGen")
	void Generate(int32 InWorldSeed,
		float InWorldMapSizeKm = 64.0f,
		float InCellSizeMeters = 128.0f);

	// World-space → grid coordinates. Returns false if the position is
	// outside the playable interior.
	UFUNCTION(BlueprintPure, Category = "QR|WorldGen")
	bool WorldToCell(FVector WorldPos, int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintPure, Category = "QR|WorldGen")
	FQRWorldCell GetCellAt(FVector WorldPos) const;

	UFUNCTION(BlueprintPure, Category = "QR|WorldGen")
	FName GetBiomeAt(FVector WorldPos) const;

	UFUNCTION(BlueprintPure, Category = "QR|WorldGen")
	EQRDepthBand GetDepthBandAt(FVector WorldPos) const;

	// Returns the POI placements emitted by the last Generate() call,
	// optionally filtered by archetype id (Remnant / ArmoryWreck / …).
	// Empty Archetype matches all.
	UFUNCTION(BlueprintCallable, Category = "QR|WorldGen")
	TArray<FQRPOIPlacement> GetPOIs(FName Archetype = NAME_None) const;

	// Render a top-down minimap texture from the cell grid. Each
	// canonical biome maps to a distinctive color (palette in cpp).
	// Returns nullptr if Generate() hasn't run yet.
	UFUNCTION(BlueprintCallable, Category = "QR|WorldGen")
	UTexture2D* BuildMinimapTexture(int32 PixelsPerCell = 1) const;

	// ── UWorldSubsystem overrides ─────────────────────────────────
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
	FRandomStream BiomeRng;
	FRandomStream POIRng;
	FRandomStream EcologyRng;
	FRandomStream FactionRng;
	FRandomStream LootRng;

	void PopulateDefaultBandPools();
	void GenerateCellGrid();
	void PlacePOIs();

	EQRDepthBand DepthBandForDistanceKm(float DistKm) const;
	FName PickBiomeForBand(EQRDepthBand Band, float NoiseSample) const;
	FName PickMicroOverlay(const FQRWorldCell& Cell, float NoiseSample) const;
};
