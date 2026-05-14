#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "QRBuildTypes.h"
#include "QRBuildModeComponent.generated.h"

class AActor;
class UStaticMeshComponent;
class UQRInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBuildModeEntered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBuildModeExited);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildPiecePlaced, AActor*, Piece);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildPlacementBlocked, FText, Reason);

/**
 * Player-side build placement. Drop on AQRCharacter. Workflow:
 *
 *   1. Player presses a build key — BP calls EnterBuildMode().
 *   2. Player selects a piece — BP calls SelectPiece(FName) with a row
 *      id from the PieceCatalog DataTable.
 *   3. Component spawns a ghost preview actor and ticks it to follow
 *      the player's look direction. If the ghost is near a placed
 *      piece's SOCKET_Snap*, the ghost snaps to that socket.
 *   4. RotateGhost(deltaDeg) — turning the ghost in 90-degree steps
 *      while keeping it on its snap point.
 *   5. Player presses confirm — BP calls TryConfirmPlacement().
 *      Component validates (no overlap with existing pieces, all
 *      materials present in inventory), consumes materials, spawns
 *      the placed piece actor (with UQRBuildPieceTag), removes ghost.
 *   6. ExitBuildMode() removes any active ghost and clears state.
 *
 * Snap convention: ghosts look for any UQRBuildPieceTag-owning actor
 * within SnapSearchRadius of the trace hit point, then test each of
 * that actor's sockets named SOCKET_SnapN/S/E/W/Left/Right/Top/Bottom.
 * The closest socket to the trace hit becomes the snap target.
 *
 * Material consumption uses the player's UQRInventoryComponent and the
 * piece row's MaterialCost array (FQRRecipeIngredient — same struct the
 * crafting system uses).
 *
 * Save: placed pieces persist via FQRBuildableSaveData in FQRGameSaveData
 * (already defined). The save flow walks all actors with UQRBuildPieceTag
 * and serializes piece id + transform + guid. Not wired in this commit;
 * that's a save-system follow-up.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRBuildModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRBuildModeComponent();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TObjectPtr<UDataTable> PieceCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build")
	TObjectPtr<UDataTable> ItemDefinitionTable;

	// Distance from camera to drop ghost when no snap surface is found.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build",
		meta = (ClampMin = "50", ClampMax = "1000"))
	float PlacementDistance = 350.0f;

	// Search radius for snap-target sockets, centered on the trace hit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build",
		meta = (ClampMin = "50", ClampMax = "1000"))
	float SnapSearchRadius = 200.0f;

	// Optional translucent material applied to the ghost. If null, the
	// ghost uses the mesh's authored materials.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TObjectPtr<UMaterialInterface> GhostMaterialValid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Build")
	TObjectPtr<UMaterialInterface> GhostMaterialInvalid;

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bBuildModeActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FName CurrentPieceId;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	bool bCurrentPlacementValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Build")
	FText LastBlockerReason;

	UPROPERTY()
	TObjectPtr<AActor> GhostActor = nullptr;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Build|Events")
	FOnBuildModeEntered OnBuildModeEntered;

	UPROPERTY(BlueprintAssignable, Category = "Build|Events")
	FOnBuildModeExited OnBuildModeExited;

	UPROPERTY(BlueprintAssignable, Category = "Build|Events")
	FOnBuildPiecePlaced OnPiecePlaced;

	UPROPERTY(BlueprintAssignable, Category = "Build|Events")
	FOnBuildPlacementBlocked OnPlacementBlocked;

	// ── API ──────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Build")
	void EnterBuildMode();

	UFUNCTION(BlueprintCallable, Category = "Build")
	void ExitBuildMode();

	UFUNCTION(BlueprintCallable, Category = "Build")
	void SelectPiece(FName PieceId);

	UFUNCTION(BlueprintCallable, Category = "Build")
	void RotateGhost(float DeltaYawDeg);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Build")
	bool TryConfirmPlacement();

	UFUNCTION(BlueprintPure, Category = "Build")
	bool HasMaterialsFor(FName PieceId) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	float CurrentGhostYaw = 0.0f;

	const FQRBuildPieceRow* FindPieceRow(FName PieceId) const;

	// Update ghost transform every tick: trace from camera, find snap
	// target, position ghost, run validation.
	void UpdateGhost();

	// Search for the closest SOCKET_Snap* on any nearby UQRBuildPieceTag
	// actor. Returns true if a valid snap was found.
	bool FindSnapTransform(const FVector& AroundLocation,
		FVector& OutLocation, FRotator& OutRotation) const;

	// Validate: no overlap with an existing build piece (other than the
	// one we just snapped to). Returns true if placement is allowed.
	bool ValidatePlacement(const FVector& Location, const FRotator& Rotation,
		UStaticMesh* Mesh, FText& OutReason) const;

	bool ConsumeMaterials(const TArray<FQRRecipeIngredient>& Cost);
	int32 CountAvailable(FName ItemId) const;

	void SpawnGhost(UStaticMesh* Mesh);
	void DestroyGhost();
	void UpdateGhostMaterial(bool bValid);

	UQRInventoryComponent* GetOwnerInventory() const;
};
