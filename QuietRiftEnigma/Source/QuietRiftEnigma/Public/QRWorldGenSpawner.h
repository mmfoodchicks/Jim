#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRWorldGenTypes.h"
#include "QRWorldGenSpawner.generated.h"

class AQRRemnantSite;
class AQRCrashSiteActor;
class AQRCaveEntrance;
class AQRNPCActor;
class AQRWildlifeActor;
class AActor;
class UQRFaunaSpawnRule;

/**
 * Consumes UQRWorldGenSubsystem's output and physically spawns every
 * world entity the GDD §4 pipeline asks for:
 *
 *   • Concordat capital (1 at world origin)
 *   • Remnant sites (one per RemnantSite POI, kind cycled across
 *     SignalSpire / PowerCore / DataArchive / ResonanceChamber)
 *   • Faction satellites (NPC actors with FactionComponent)
 *   • Crash-site wrecks (one per archetype POI, loot scattered as
 *     AQRWorldItem actors per CrashLootTemplates)
 *   • Cave entrances (per cell with Caves habitat flag, sparsified)
 *   • Wildlife — biome-aware density via FaunaRulesPerBiome
 *
 * Designer drops one in any level alongside AQRWorldGenSeedActor.
 * After Generate has run on the seed actor, click SpawnAll on the
 * spawner. Re-running clears prior spawns first (idempotent).
 *
 * Spawning is deterministic given the worldgen seed: same WorldSeed
 * = same set of actors at the same positions = shareable worlds.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWorldGenSpawner : public AActor
{
	GENERATED_BODY()

public:
	AQRWorldGenSpawner();

	// ── Actor class assignments (BP-overridable) ──────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Remnants")
	TSubclassOf<AQRRemnantSite> RemnantSiteClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Wrecks")
	TSubclassOf<AQRCrashSiteActor> CrashSiteClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Caves")
	TSubclassOf<AQRCaveEntrance> CaveEntranceClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Factions")
	TSubclassOf<AActor> ConcordatCapitalClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Factions")
	TSubclassOf<AQRNPCActor> FactionSatelliteClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Fauna")
	TSubclassOf<AQRWildlifeActor> WildlifeFallbackClass;

	// ── Data ──────────────────────────────────────────────────────

	// Biome tag → fauna rules data asset. Worldgen iterates cells,
	// looks up rules per biome, scatters wildlife by density.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Fauna")
	TMap<FName, TObjectPtr<UQRFaunaSpawnRule>> FaunaRulesPerBiome;

	// Archetype id → hardcoded crash-site loot template. Constructor
	// pre-fills the 7 canonical wreck archetypes; designer can
	// override per-project / per-level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Wrecks")
	TMap<FName, FQRCrashLootTemplate> CrashLootTemplates;

	// Optional POI archetype DataTable (rows of FQRPOIArchetypeRow).
	// When set, overrides the hardcoded archetype-id → ActorClass map
	// and the corresponding LootTemplate per archetype. Lets designers
	// add new archetypes (caves variants, faction lookouts, etc.)
	// without recompiling C++.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Archetypes")
	TObjectPtr<class UDataTable> POIArchetypeTable;

	// ── Tunables ──────────────────────────────────────────────────

	// Approximate animals per square kilometer per biome. Multiplied
	// by the biome's rule SpawnDensityMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Fauna",
		meta = (ClampMin = "0", ClampMax = "200"))
	float FaunaPerKm2Base = 6.0f;

	// 1 cave entrance per N cells that carry the Caves habitat flag.
	// Higher = sparser. Default 50 keeps caves rare-feeling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Caves",
		meta = (ClampMin = "1", ClampMax = "500"))
	int32 OneCavePerNFlaggedCells = 50;

	// ── API ───────────────────────────────────────────────────────

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Spawner")
	void SpawnAll();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Spawner")
	void ClearAll();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Spawner|Sections")
	void SpawnPOIsOnly();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Spawner|Sections")
	void SpawnFaunaOnly();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Spawner|Sections")
	void SpawnCavesOnly();

	// ── Remnant escalation ────────────────────────────────────────
	// Rift-family tech node ids that bump every spawned RemnantSite
	// to the next wake state when unlocked by UQRResearchComponent.
	// Order matters — first id unlocks Stirring, second Active, etc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner|Remnants")
	TArray<FName> RiftTechNodeProgression;

	// Force-set every live AQRRemnantSite to the same wake state.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Spawner|Remnants")
	void BumpAllRemnantsToState(EQRRemnantWakeState NewState);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleTechUnlocked(FName TechNodeId);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner")
	bool bSpawnOnBeginPlay = false;

private:
	// All spawned actors tracked here so ClearAll() can destroy them.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;

	void PopulateDefaultCrashTemplates();

	void SpawnPOIs();
	void SpawnFauna();
	void SpawnCaves();

	// Helpers
	AActor* SpawnAt(UClass* Cls, const FVector& Loc, const FRotator& Rot);
	bool    TraceGround(const FVector& XY, FVector& OutHit) const;
	int32   ResolveFaunaSpawnCount(const class UQRWorldGenSubsystem* Sub) const;
};
