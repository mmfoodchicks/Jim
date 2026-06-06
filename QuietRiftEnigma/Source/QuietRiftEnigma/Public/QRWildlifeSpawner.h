#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRWildlifeSpawner.generated.h"

class AQRWildlifeBase;

/**
 * Drop-in level actor that keeps a target number of wildlife alive in
 * a play area. Designed for L_DevTest and similar dev maps: no
 * worldgen subsystem, no biome lookup, just "keep ~12 wildlife within
 * 60 m of me, top up every 8 seconds, never exceed the cap."
 *
 * Behaviour:
 *   - On BeginPlay, optionally seeds the play area immediately, then
 *     starts a repeating timer.
 *   - Each tick the spawner counts live AQRWildlifeBase actors in the
 *     world (or just its own spawned ones, see bGlobalCap). If the
 *     count is below MaxAlive, it picks a random species from
 *     SpeciesPool and tries to spawn at a navmesh-projected point
 *     in [SpawnRadiusMin..SpawnRadiusMax].
 *   - "Live" = not dead. Dead carcasses don't count toward the cap so
 *     hunting them down stops blocking respawns.
 *
 * Designer drops one in the level. To wire from Python, see
 * qr_dev_test_dressup.ensure_wildlife_spawner().
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlifeSpawner : public AActor
{
	GENERATED_BODY()

public:
	AQRWildlifeSpawner();

	/** Wildlife classes to pick from. Each pick is uniform-weighted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner")
	TArray<TSubclassOf<AQRWildlifeBase>> SpeciesPool;

	/** Hard cap on living wildlife the spawner will allow at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner",
		meta = (ClampMin = "0", ClampMax = "200"))
	int32 MaxAlive = 12;

	/** Seconds between top-up attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner",
		meta = (ClampMin = "0.25", ClampMax = "120"))
	float SpawnIntervalSeconds = 8.0f;

	/** Min/max distance from the spawner to scatter new wildlife. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner",
		meta = (ClampMin = "200"))
	float SpawnRadiusMin = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner",
		meta = (ClampMin = "500"))
	float SpawnRadiusMax = 6000.0f;

	/**
	 * If true the cap counts every AQRWildlifeBase in the world (so two
	 * spawners share one ceiling). If false only this spawner's own
	 * children count. Default true: global cap prevents "I placed three
	 * spawners and now there are 100 animals" surprises.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner")
	bool bGlobalCap = true;

	/** How many animals to spawn immediately on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner",
		meta = (ClampMin = "0", ClampMax = "200"))
	int32 InitialBurst = 6;

	/** Disable the timer (still honours InitialBurst). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner")
	bool bAutoStart = true;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "QR|Spawner")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "QR|Spawner")
	void StopSpawning();

	/** Force a single top-up pass right now (respects MaxAlive). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "QR|Spawner")
	void TopUpNow();

	/** Despawn every still-living animal this spawner spawned. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "QR|Spawner")
	void ClearMyWildlife();

	UFUNCTION(BlueprintPure, Category = "QR|Spawner")
	int32 CountLiveTrackedWildlife() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	FTimerHandle TopUpTimerHandle;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AQRWildlifeBase>> SpawnedAnimals;

	/** True if we successfully placed one. */
	bool TrySpawnOne();

	/** Random navmesh-projected point in the spawn ring around us. */
	bool PickSpawnLocation(FVector& OutLocation) const;
};
