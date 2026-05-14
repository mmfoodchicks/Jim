#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "QRRecipeDefinition.h"   // reuse FQRRecipeIngredient for material cost
#include "QRBuildTypes.generated.h"

/**
 * Categories the build system uses for socket-matching rules:
 *   Wall      — vertical wall pieces (BLD_WALL_*). Snaps via
 *               SOCKET_SnapLeft/Right and SOCKET_SnapTop/Bottom.
 *   Floor     — horizontal floor tiles (BLD_FLOOR_*, BLD_FOUNDATION_*).
 *               Snaps via SOCKET_SnapN/S/E/W on neighbors.
 *   Roof      — angled / flat roof tiles (BLD_ROOF_*). Snap conventions
 *               match Floor (cardinal SnapN/S/E/W).
 *   Door      — fits inside a SOCKET_HingePivot on a doorway wall.
 *   Structural — pillars, stairs, ramps. Single anchor (SnapBottom).
 */
UENUM(BlueprintType)
enum class EQRBuildCategory : uint8
{
	Wall        UMETA(DisplayName = "Wall"),
	Floor       UMETA(DisplayName = "Floor"),
	Roof        UMETA(DisplayName = "Roof"),
	Door        UMETA(DisplayName = "Door"),
	Structural  UMETA(DisplayName = "Structural"),
};

/**
 * One buildable piece in the catalog. Authored as a DataTable row; one
 * row per piece id (BLD_WALL_WOOD, BLD_FLOOR_STONE, etc.). The build
 * mode component reads this to know what mesh to spawn, how much
 * material to consume, and how to snap.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRBuildPieceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQRBuildCategory Category = EQRBuildCategory::Wall;

	// Static mesh to spawn for both ghost preview and placed piece.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UStaticMesh> Mesh;

	// Material cost. Reuses the recipe ingredient struct from the
	// crafting module so the same author conventions apply (ItemId +
	// Quantity; bIsReusable is ignored here — building always consumes).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FQRRecipeIngredient> MaterialCost;

	// Optional: tech-node prerequisite. Empty = always available once
	// build mode is unlocked.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RequiredTechNodeId;
};
