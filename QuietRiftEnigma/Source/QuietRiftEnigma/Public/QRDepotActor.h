#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRDepotActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UQRInventoryComponent;


/**
 * Designer-placed storage depot. Master GDD §9 + §21 — the colony's
 * physical logistics layer. A depot is a fat AInventoryComponent host
 * with a PullRadius (haulers within range can deposit / withdraw),
 * a PullPriority (higher = used first when a station needs material),
 * and an optional StationTag list naming which stations the depot
 * services preferentially.
 *
 * The hauler AI loop (not in this commit — needs behavior trees) walks
 * the world, finds nearby depots, finds stations within their pull
 * radius that have unmet demand, transports items between them. For
 * v1 the depot is the storage primitive; auto-hauling is a follow-up.
 *
 * Players can manually deposit / withdraw via F-interact — the existing
 * UQRLootContainerComponent path handles this if the depot subclasses
 * carry that component. v1 keeps it as a plain inventory.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRDepotActor : public AActor
{
	GENERATED_BODY()

public:
	AQRDepotActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Depot")
	TObjectPtr<USphereComponent> InteractSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Depot")
	TObjectPtr<UStaticMeshComponent> DepotMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|Depot")
	TObjectPtr<UQRInventoryComponent> Storage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Depot")
	FText DisplayName;

	// Stations within this radius (cm) can pull from the depot
	// without an extra hauler trip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Depot",
		meta = (ClampMin = "100", ClampMax = "5000"))
	float PullRadiusCm = 2500.0f;

	// Higher = preferred when multiple depots are in range. Lets
	// designers stage materials closer to specific stations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Depot")
	int32 PullPriority = 0;

	// Optional tag list — if set, the depot only services stations whose
	// StationTag is in this list. Empty = services any station in range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Depot")
	TArray<FName> ServicedStationTags;
};


/**
 * One declared material demand from a crafting station: "I need 5
 * RAW_METAL_INGOT before I can finish my queue." Hauler AI consumes
 * these to know what to fetch.
 *
 * Set by UQRCraftingComponent when a recipe queue stalls on an
 * ingredient; cleared as items are delivered.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRStationDemand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName ItemId;
	UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
	UPROPERTY(BlueprintReadOnly) FName RequestingStationTag;
	UPROPERTY(BlueprintReadOnly) FVector RequestingLocation = FVector::ZeroVector;
};
