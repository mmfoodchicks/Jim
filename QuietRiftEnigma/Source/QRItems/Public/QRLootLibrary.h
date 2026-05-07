#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRTypes.h"
#include "QRLootLibrary.generated.h"

class UQRItemDefinition;
class UQRInventoryComponent;

/**
 * One possible item drop in a loot table. The actual drop is decided by
 * weighted random pull — Weight=10 entries are 10x more likely to roll
 * than Weight=1 entries.
 *
 * MinTier / MaxTier let a single table cover multiple tiers of the same
 * source — e.g. a Vanguard outpost loot table can include T1 ammo at low
 * outpost tiers and T3 ammo at Inner Sanctum. Set both to 0 to skip the
 * tier filter entirely.
 */
USTRUCT(BlueprintType)
struct QRITEMS_API FQRLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 MaxQuantity = 1;

	// Relative weight in the weighted-random pull. 0 means "never picked"
	// (useful for temporarily disabling an entry without deleting the row).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 Weight = 1;

	// Inclusive tier filter. Set MinTier=MaxTier=0 to disable.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "5"))
	int32 MinTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "5"))
	int32 MaxTier = 0;

	// Per-entry drop chance applied AFTER the weighted pick. Lets a rare
	// item win a roll but still occasionally fizzle. 1.0 = always drops.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float DropChance = 1.0f;
};

/**
 * One loot table — typically one row in DT_LootTables.csv.
 *
 * RollCount is how many times the weighted pull runs per loot event.
 * A rolled entry can still produce nothing if its DropChance fails.
 *
 * Examples:
 *   "Wildlife.AshbackBoar" — 2 rolls into a table of meat / hide / sinew
 *   "POI.ArmoryWreck.T2"   — 4 rolls into ammo / weapon parts / brass
 *   "Vanguard.Hardpoint"   — 3 rolls into raid drops scaled by tier
 */
USTRUCT(BlueprintType)
struct QRITEMS_API FQRLootTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "20"))
	int32 RollCount = 1;

	// If true, the same item id can appear more than once across rolls.
	// If false (default) once an entry rolls, it's removed from the
	// remaining rolls — simulates "you got the cool thing, no double dip."
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bAllowDuplicates = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FQRLootEntry> Entries;
};

// Result of one loot pull — used by spawners that want to enumerate
// rolled items before instantiating them (e.g. for UI preview).
USTRUCT(BlueprintType)
struct QRITEMS_API FQRRolledLoot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName ItemId;
	UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
};

/**
 * Static helpers for resolving loot tables. Tables are looked up by row
 * name in a UDataTable (FQRLootTableRow row type). Spawning the rolled
 * items into a UQRInventoryComponent is a separate convenience helper.
 *
 * Determinism: pass an explicit RandomStream to RollTableSeeded for
 * reproducible loot (save-system replay, world-gen, automated tests).
 * The non-Seeded variant uses FMath::RandRange (global RNG).
 */
UCLASS()
class QRITEMS_API UQRLootLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Roll a loot table by row name. Returns the rolled items (may be empty
	// if every entry failed its DropChance, or the table itself was missing).
	UFUNCTION(BlueprintCallable, Category = "Loot")
	static TArray<FQRRolledLoot> RollTable(const UDataTable* Table, FName RowName, int32 Tier = 0);

	// Same as RollTable but with an explicit random stream for reproducibility.
	static TArray<FQRRolledLoot> RollTableSeeded(const UDataTable* Table, FName RowName,
	                                              int32 Tier, FRandomStream& Stream);

	// Convenience: roll a table and push every result into an inventory.
	// Returns the number of items added (sum of quantities). Items that
	// won't fit (weight/volume/slots) are silently dropped.
	UFUNCTION(BlueprintCallable, Category = "Loot")
	static int32 RollAndDeposit(const UDataTable* Table, FName RowName, int32 Tier,
	                             UQRInventoryComponent* Target,
	                             const UDataTable* ItemDefinitionTable);

	// Compute total weight of all valid entries for a tier — useful for UI
	// previews that show "this container has ~N candidate items."
	UFUNCTION(BlueprintPure, Category = "Loot")
	static int32 GetTotalEntryWeight(const FQRLootTableRow& Row, int32 Tier);

private:
	static const FQRLootTableRow* FindRow(const UDataTable* Table, FName RowName);
	static bool EntryMatchesTier(const FQRLootEntry& Entry, int32 Tier);
};
