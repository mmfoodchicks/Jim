#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRDressingStreamer.generated.h"

class AQRProceduralScatterActor;
class UQRBiomeProfile;

/**
 * Runtime vegetation/dressing streamer -- the answer to "dress the
 * whole 64 km world without wrecking FPS". Instead of one giant static
 * scatter bubble around the origin (qr_world_dressing.run_full), this
 * actor maintains a moving (2R+1)^2 ring of scatter tiles centered on
 * the local player's tile:
 *
 *   - Player crosses a tile edge -> tiles leaving the ring are torn
 *     down, tiles entering it are queued.
 *   - The queue drains ONE tile per TileGenInterval seconds, nearest
 *     first, so a fresh ring never generates in a single frame hitch.
 *   - Per-tile seeds are pure functions of the tile coordinate + world
 *     seed, so walking away and back rebuilds the identical dressing.
 *
 * Each tile is an AQRProceduralScatterActor in WorldGen mode: every
 * placement asks UQRWorldGenSubsystem for the cell biome and picks
 * from BiomeProfileMap, so one streamer covers all 14 biomes. The
 * subsystem grid must be generated for biome resolution (the seed
 * actor's bAutoGenerateOnBeginPlay handles PIE/game sessions);
 * without it, tiles fall back to the first mapped profile.
 *
 * Worst-case live cost: (2R+1)^2 * InstancesPerTile HISM instances,
 * each with per-instance cull distances -- R=2, 350/tile = 8,750
 * instances, far under the 120k static-bubble budget while LOOKING
 * dressed everywhere the player can see.
 *
 * Spawned by qr_world_dressing.enable_streaming() (which also stamps
 * BiomeProfileMap), or dropped in a level by hand.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRDressingStreamer : public AActor
{
	GENERATED_BODY()

public:
	AQRDressingStreamer();

	// Tile edge length (cm). 500 m default matches the static bubble's
	// tiling so streamed and pre-dressed tiles line up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer",
		meta = (ClampMin = "10000", ClampMax = "200000"))
	float TileSizeCm = 50000.0f;

	// Ring radius in tiles around the player's tile. 2 -> 5x5 bubble,
	// 1.25 km of dressed ground in every direction at default TileSize.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer",
		meta = (ClampMin = "1", ClampMax = "6"))
	int32 RingRadiusTiles = 2;

	// HISM placements per tile. Live worst case is
	// (2*RingRadiusTiles+1)^2 * this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer",
		meta = (ClampMin = "8", ClampMax = "2000"))
	int32 InstancesPerTile = 350;

	// Seconds between tile generations while the queue drains. One tile
	// per interval keeps ring turnover hitch-free.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer",
		meta = (ClampMin = "0.05", ClampMax = "5"))
	float TileGenInterval = 0.25f;

	// Vertical half-extent (cm) of each tile's trace box, centered on
	// the player's Z when the tile spawns. Placements only land on
	// ground inside this window, so raise it for mountainous maps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer",
		meta = (ClampMin = "600", ClampMax = "200000"))
	float TileHalfHeightCm = 10000.0f;

	// Biome tag -> profile palette, copied onto every spawned tile.
	// Fill with the 14 canonical profiles (qr_seed_biome_profiles.py).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer")
	TMap<FName, TObjectPtr<UQRBiomeProfile>> BiomeProfileMap;

	// Per-instance cull distances stamped onto every tile (cm).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer|Performance",
		meta = (ClampMin = "0", ClampMax = "200000"))
	float CullStartDistance = 15000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer|Performance",
		meta = (ClampMin = "0", ClampMax = "400000"))
	float CullEndDistance = 30000.0f;

	// Mixed into every per-tile seed so different worlds dress
	// differently while the same world always dresses the same.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Streamer")
	int32 WorldSeed = 1337;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Live tiles keyed by integer tile coordinate.
	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<AQRProceduralScatterActor>> LiveTiles;

	// Tiles waiting to generate, nearest-to-player first.
	TArray<FIntPoint> PendingTiles;

	// Sentinel forces the first Tick to build the initial ring.
	FIntPoint CenterTile = FIntPoint(TNumericLimits<int32>::Min(), TNumericLimits<int32>::Min());

	float GenCooldown = 0.0f;

	FIntPoint TileAt(const FVector& WorldPos) const;
	void RefreshRing(const FIntPoint& NewCenter);
	void GenerateOnePending(const FVector& PlayerLoc);
	void DestroyAllTiles();
};
