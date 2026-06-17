#pragma once

#include "CoreMinimal.h"
#include "QRSaveTypes.h"

class UQRInventoryComponent;
class UQRSurvivalComponent;
class UQRResearchComponent;
class UQRItemDefinition;
class UQRItemInstance;

/**
 * One place that knows how to turn live component state into save structs
 * and back. QuickSave used to hand-copy fields in AQRGameMode, which is how
 * research, armour slots, injuries, and grid positions all silently fell
 * out of the save — each new system needed someone to remember to extend
 * the copy code. Now a system is saved iff it has a Capture/Apply pair
 * here, and the GameMode just calls through.
 *
 * All functions are static and server-side; call only with authority.
 */
struct QRSAVENET_API FQRSaveSnapshot
{
	// ── Inventory ────────────────────────────────────────────────
	// Captures every grid item with its placement, plus the 7 equip slots
	// (hand / offhand / helm / chest / legs / rig / backpack) tagged with
	// EQRSavedEquipSlot. Legacy HandSlot/bHasHandSlot mirrors are still
	// written so pre-v2 readers keep working.
	static void CaptureInventory(const UQRInventoryComponent* Inv, FQRInventorySaveData& Out);

	// Wipes the live inventory and rebuilds it from Data. Restore order:
	// containers first (so their grids exist), then armour, then hands,
	// then loose items — auto-placed on add, then moved to their saved
	// cells (two passes, so an interim auto-placement squatting on another
	// item's saved cell gets evicted by the second pass).
	static void ApplyInventory(UQRInventoryComponent* Inv, const FQRInventorySaveData& Data);

	// ── Survival vitals + injuries ───────────────────────────────
	static void CaptureSurvival(const UQRSurvivalComponent* Surv, FQRSurvivorSaveData& Out);
	static void ApplySurvival(UQRSurvivalComponent* Surv, const FQRSurvivorSaveData& Data);

	// ── Research / tech tree / codex ─────────────────────────────
	static void CaptureResearch(const UQRResearchComponent* Research, FQRResearchSaveData& Out);
	static void ApplyResearch(UQRResearchComponent* Research, const FQRResearchSaveData& Data);

	// ── Item definition resolution ───────────────────────────────
	// ItemId -> UQRItemDefinition. Definitions live in nested buckets under
	// /Game/QuietRift/Data/Items/<Bucket>/<Id>, so a root-path LoadObject
	// (the old GameMode approach) missed everything seeded by the arsenal /
	// item scripts. This scans the asset registry once and caches.
	static UQRItemDefinition* ResolveItemDefinition(FName ItemId);

private:
	static FQRItemSaveData CaptureOne(const UQRItemInstance* Inst, EQRSavedEquipSlot EquipSlot);
	// Applies the per-instance fields (durability, spoil, guid, ...) that
	// ForceAddByDefinition's fresh instance doesn't carry.
	static void ApplyInstanceFields(UQRItemInstance* Inst, const FQRItemSaveData& Saved);
};
