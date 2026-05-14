#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "QRLootContainerComponent.generated.h"

class UQRInventoryComponent;
class UDataTable;
class UQRLootedRegistry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContainerLooted,
	AActor*, Looter, int32, ItemsAdded);

/**
 * Drop on any actor that should act as a lootable container — wrecked
 * crates, supply caches, body bags at POI spawn sockets, etc. When a
 * player interacts and the target has this component (in addition to
 * the dialogue/etc. lookups in AQRCharacter::Server_Interact), the
 * container rolls its loot table into the player's inventory and
 * marks itself empty.
 *
 * Persistence:
 *   - Each container has a UniqueId (FGuid). Generated automatically at
 *     editor placement time (or runtime spawn) so the same level
 *     instance always has the same id across saves.
 *   - The world's UQRLootedRegistry tracks which UniqueIds have been
 *     looted. On BeginPlay each container asks the registry "have I
 *     been looted?" and sets bHasBeenLooted accordingly — so reloaded
 *     levels don't re-spawn loot in already-emptied containers.
 *   - The save system serializes the registry's id set as part of
 *     FQRGameSaveData.LootedContainerIds.
 *
 * Loot resolution: uses UQRLootLibrary with LootTableId + LootTier.
 * Items spawn into the looter's UQRInventoryComponent. If the inventory
 * is full, the overflow items quietly drop (caller can listen on
 * OnContainerLooted to handle remainder — drop to ground, etc.).
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRLootContainerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRLootContainerComponent();

	// ── Config ───────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDataTable> LootTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName LootTableRowId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot",
		meta = (ClampMin = "0", ClampMax = "5"))
	int32 LootTier = 0;

	// Required for spawning resolved items — same row-struct convention
	// as UQRCraftingComponent / UQRLootLibrary (reflection-resolved
	// "Definition" property on each row).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDataTable> ItemDefinitionTable;

	// Stable id used by the registry to remember whether this container
	// has been looted. Auto-assigned in BeginPlay if left blank, but for
	// editor-placed containers you should leave it auto so each placement
	// gets its own id (the editor serializes the assigned value).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FGuid UniqueId;

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Loot")
	bool bHasBeenLooted = false;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Loot|Events")
	FOnContainerLooted OnContainerLooted;

	// ── API ──────────────────────────────────

	// Try to loot the container into `Looter`'s inventory. Returns false
	// if already looted or if `Looter` has no UQRInventoryComponent.
	// Called automatically by AQRCharacter::Server_Interact when the
	// player interacts with an actor carrying this component.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	bool TryLoot(AActor* Looter);

	UFUNCTION(BlueprintPure, Category = "Loot")
	bool IsLooted() const { return bHasBeenLooted; }

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
