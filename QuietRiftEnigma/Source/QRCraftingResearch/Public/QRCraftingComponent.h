#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "QRRecipeDefinition.h"
#include "QRCraftingComponent.generated.h"

class UQRInventoryComponent;
class UQRItemDefinition;
class AQRStationBase;
class UQRDepotComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftingProgress, FName, RecipeId, float, Progress01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftingCompleted, FName, RecipeId, const TArray<FName>&, OutputItemIds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftingFailed, FName, RecipeId, FText, Reason);

/**
 * Per-actor crafting runner. Drop on a workbench / forge / kiln (anything
 * derived from AQRStationBase) OR on the player character for hand-craft
 * recipes. The component owns:
 *
 *   - A queue of recipe IDs
 *   - The current task's progress timer
 *   - Validation: station-tag match, tech-node unlock, ref component,
 *     and ingredient availability
 *   - Ingredient consumption and output spawning
 *   - Blocker reporting (via FText) so UI can explain why a recipe stalls
 *
 * Two input source modes:
 *   1. Configured InputInventory (player-craft case)        — pull from
 *      the player's UQRInventoryComponent
 *   2. None set + owner is AQRStationBase (NPC-craft case)  — pull from
 *      depots via station's FindNearbyDepots / PullFromDepots
 *
 * Two output sink modes:
 *   1. Configured OutputInventory                            — push into
 *      a specific inventory (the crafter, the station's output bin, etc.)
 *   2. None set                                              — fire the
 *      OnCompleted delegate with output item IDs and let the listener
 *      decide where to spawn them
 *
 * The component auto-ticks. Owning code can pause it by setting
 * SetComponentTickEnabled(false) (e.g. when the station has no power).
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRCRAFTINGRESEARCH_API UQRCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRCraftingComponent();

	// ── Config ───────────────────────────────
	// DataTable (FQRRecipeTableRow rows). Set on the owning Blueprint.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UDataTable> RecipeTable;

	// DataTable mapping FName -> UQRItemDefinition* (project convention:
	// row struct has a "Definition" column). Required for output spawning.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UDataTable> ItemDefinitionTable;

	// Optional input inventory — if set, ingredients pull from here.
	// If null AND the owner is AQRStationBase, ingredients pull from
	// nearby depots. If null AND no station owner, crafting fails with
	// "no input source."
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TObjectPtr<UQRInventoryComponent> InputInventory;

	// Optional output inventory — if set, outputs push here on completion.
	// If null, OnCompleted fires with the output IDs and the listener
	// decides where to spawn them (e.g. drop on the ground at OutputDrop).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting")
	TObjectPtr<UQRInventoryComponent> OutputInventory;

	// Multiplier on recipe craft time. < 1 speeds up (skilled NPC,
	// upgraded station), > 1 slows down (low fatigue, damaged station).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting",
		meta = (ClampMin = "0.1", ClampMax = "10"))
	float CraftSpeedMultiplier = 1.0f;

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crafting")
	TArray<FName> RecipeQueue;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crafting")
	FName CurrentRecipeId;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crafting")
	float CurrentTaskTimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crafting")
	float CurrentTaskTotalTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crafting")
	bool bIsBlocked = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Crafting")
	FText BlockerReason;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
	FOnCraftingProgress OnProgress;

	UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
	FOnCraftingCompleted OnCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Crafting|Events")
	FOnCraftingFailed OnFailed;

	// ── API ──────────────────────────────────

	// Append a recipe to the queue. Returns false if RecipeId not found in
	// RecipeTable. Does NOT validate ingredients here — that happens when
	// the recipe reaches the front of the queue (so a player can queue 5
	// crafts while still gathering inputs).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	bool QueueRecipe(FName RecipeId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	void CancelCurrentTask();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting")
	void ClearQueue();

	// Pure check: would this recipe craft right now? Fills OutReason on
	// failure with a player-readable explanation ("Missing 3x Bracket Set",
	// "Tech Node TECH_OPTICS_T1 not unlocked", "Wrong station tag", etc.).
	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool CanCraft(FName RecipeId, FText& OutReason) const;

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsCrafting() const { return !CurrentRecipeId.IsNone(); }

	// 0..1 progress for UI bars. Returns 0 when nothing is crafting.
	UFUNCTION(BlueprintPure, Category = "Crafting")
	float GetCurrentProgress01() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                            FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	// Look up a recipe row. Returns nullptr if missing.
	const FQRRecipeTableRow* FindRecipeRow(FName RecipeId) const;

	// Look up a UQRItemDefinition by id. Returns nullptr if missing.
	UQRItemDefinition* FindItemDefinition(FName ItemId) const;

	// Try to start the next queued recipe. On success sets CurrentRecipeId
	// + CurrentTaskTimeRemaining and consumes ingredients. On failure
	// fires OnFailed and pops the recipe from the queue.
	void StartNextRecipe();

	// Spawn outputs for the current recipe. Yield chances are rolled per output.
	void CompleteCurrentRecipe();

	// Try to consume the recipe's non-reusable ingredients from the
	// input source. Returns false if any can't be fully consumed (will
	// roll back partial consumption).
	bool ConsumeIngredients(const FQRRecipeTableRow& Recipe, FText& OutReason);

	// Walk the input source(s) and check every ingredient is available
	// in the required quantity. Reusable ingredients only need to exist,
	// not be consumed.
	bool HasAllIngredients(const FQRRecipeTableRow& Recipe, FText& OutReason) const;

	// How many of an item are available across the configured input source.
	int32 CountAvailable(FName ItemId) const;

	// Take N of an item from the input source. Returns the actual count taken.
	int32 ConsumeFromInputs(FName ItemId, int32 Quantity);

	// Helpers for delivering an output (one item id + quantity).
	void DeliverOutput(FName ItemId, int32 Quantity, TArray<FName>& OutDelivered);

	void SetBlocker(const FText& Reason);
	void ClearBlocker();
};
