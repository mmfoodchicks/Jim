#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRProceduralScatterActor.generated.h"

class UBoxComponent;
class UStaticMesh;
class UQRBiomeProfile;

/**
 * One entry in a scatter actor's palette. The Mesh / ActorClass / Tag
 * fields are alternatives — pick ONE per entry:
 *   • Mesh        → spawn as a HierarchicalInstancedStaticMeshComponent
 *                   (cheap, thousands of instances, no per-instance
 *                   logic — best for plants, rocks, decals).
 *   • ActorClass  → SpawnActor each placement (heavy, each has its
 *                   own AActor — use for wildlife / NPCs / interactables).
 *
 * Weight controls relative selection probability. Scale + rotation
 * apply randomization per placement.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRScatterEntry
{
	GENERATED_BODY()

	// Static mesh to scatter. Mutually exclusive with ActorClass —
	// prefer this when you don't need per-instance gameplay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> Mesh;

	// Actor class to scatter (Wildlife, NPC, gathering node, etc.).
	// Mutually exclusive with Mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AActor> ActorClass;

	// Relative pick weight against other entries in the same scatter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float Weight = 1.0f;

	// Uniform scale range per instance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float MinScale = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float MaxScale = 1.20f;

	// Slot the instance Z to the ground hit + this Z offset (cm).
	// Negative values bury the instance slightly — good for rocks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ZOffset = 0.0f;

	// If true, rotate yaw around world up by a random amount.
	// Pitch / roll stay zero (no awkward flipped foliage).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRandomYaw = true;

	// Align the instance's up vector to the ground normal instead of
	// world Z. Looks better on slopes but bad for trees / pillars.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAlignToSurface = false;
};


/**
 * Drop one in a level → defines a box volume → on Generate() (button
 * or BeginPlay), random-scatters N instances chosen from Palette
 * within the box, each one ground-traced down to find a hit. Static
 * mesh entries pack into a single HISM per mesh for performance;
 * actor entries spawn-as-actors so they can be picked up / interacted
 * with by the existing F-trace path.
 *
 * Reseed: the public Seed field drives FRandomStream so re-generations
 * with the same seed produce the same world (deterministic worlds,
 * shareable seeds).
 *
 * Why this and not just PCG: PCG is great but BP-heavy and asset-
 * heavy to author from scratch. This C++ scatter is the cheap path
 * — designer drags one in, fills the Palette array, hits Generate.
 * Mix it freely with the ScifiJungle PCG_Manager for layered biomes.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRProceduralScatterActor : public AActor
{
	GENERATED_BODY()

public:
	AQRProceduralScatterActor();

	// Volume the scatter runs inside. Drag the gizmo to resize in
	// editor; instances spawn at random (X, Y) within the box footprint
	// and slot to the ground via downward trace.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Scatter")
	TObjectPtr<UBoxComponent> Bounds;

	// Optional biome profile data asset. When set, its Palette overrides
	// the inline Palette below and its Suggested* defaults override
	// TargetCount / MinSpacing / MaxSlopeDeg if those are left at the
	// constructor defaults. Multiple scatter actors that share a profile
	// guarantee a consistent biome look.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Scatter")
	TObjectPtr<UQRBiomeProfile> BiomeProfile;

	// Things this scatter can place. Ignored if BiomeProfile is set.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Scatter")
	TArray<FQRScatterEntry> Palette;

	// Total target placements within the box. The actual count may be
	// less if ground traces miss too often.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Scatter",
		meta = (ClampMin = "1", ClampMax = "10000"))
	int32 TargetCount = 200;

	// Min spacing between any two placements (cm). Higher = sparser.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Scatter",
		meta = (ClampMin = "0", ClampMax = "5000"))
	float MinSpacing = 80.0f;

	// Max angle (degrees) between ground normal and world up that the
	// scatter will accept. > this and the placement is rejected.
	// 90 = vertical cliff allowed; 30 = gentle slopes only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Scatter",
		meta = (ClampMin = "0", ClampMax = "90"))
	float MaxSlopeDeg = 45.0f;

	// Deterministic seed. Same seed + same Palette + same Bounds =
	// same scattered world.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Scatter")
	int32 Seed = 1337;

	// Generate on BeginPlay. Turn off if you only want editor-time
	// generation (saved into the level).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Scatter")
	bool bAutoGenerateOnBeginPlay = true;

	// Run the scatter now. Clears previous spawns. Callable from BP /
	// from the editor button.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Scatter")
	void Generate();

	// Wipe everything spawned by the last Generate() call.
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "QR|Scatter")
	void ClearGenerated();

	virtual void BeginPlay() override;

protected:
	// Instanced-mesh components owned by this actor, keyed by mesh ptr
	// so multiple palette entries that share a mesh pack together.
	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<class UHierarchicalInstancedStaticMeshComponent>> MeshInstances;

	// Spawned non-instanced actors so we can destroy them on regen.
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;

private:
	// Pick a palette index weighted by FQRScatterEntry::Weight.
	int32 PickPaletteIndex(FRandomStream& Rng) const;

	// One placement attempt. Returns true if it succeeded.
	bool TryPlaceOne(const FQRScatterEntry& Entry, FRandomStream& Rng,
		const FBox& WorldBox, const TArray<FVector>& AlreadyPlaced);
};
