#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "QRTypes.h"
#include "QRCodexSubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCodexEntryUpdated,
	FName, EntryId, EQRCodexDiscoveryState, NewState);


/**
 * One codex record. Identity is the FName key in the subsystem map.
 *
 *   Unknown    — never observed; silhouette only.
 *   Observed   — seen at distance / from passing wildlife / brief
 *                glance. Reveals name + biome.
 *   Sampled    — picked up / hunted / approached. Reveals stats.
 *   Researched — fully studied at a workbench. Reveals all data
 *                including hazards, recipes, lore fragments.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRCodexEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FName Category;    // Flora / Fauna / Item / POI / Biome / Remnant
	UPROPERTY(BlueprintReadOnly) EQRCodexDiscoveryState State = EQRCodexDiscoveryState::Undiscovered;
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) FText Description;
	UPROPERTY(BlueprintReadOnly) int32 SeenCount = 0;
	UPROPERTY(BlueprintReadOnly) FDateTime FirstSeen;
};


/**
 * Aggregates every Codex discovery in one place. Hooks anywhere
 * "first contact" matters:
 *   • AQRWorldItem::TryPickup        → Record(ItemId, "Item")
 *   • AQRWildlifeActor::Tick         → Record(SpeciesId, "Fauna") on first
 *                                       perception
 *   • AQRCharacter::ApplyBiomeProfile→ Record(BiomeTag, "Biome")
 *   • AQRRemnantSite::SetWakeState   → Record(KindName, "Remnant") + bump
 *                                       state through observation
 *
 * UQRCodexWidget reads from this and groups by Category. Designer can
 * also drop hand-authored lore entries via PreloadEntries.
 */
UCLASS(BlueprintType)
class QUIETRIFTENIGMA_API UQRCodexSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnCodexEntryUpdated OnEntryUpdated;

	// Record (or update) a codex entry. Returns true if state advanced.
	UFUNCTION(BlueprintCallable, Category = "QR|Codex")
	bool Record(FName Id, FName Category, FText DisplayName,
		EQRCodexDiscoveryState NewState = EQRCodexDiscoveryState::Observed);

	UFUNCTION(BlueprintPure, Category = "QR|Codex")
	bool HasEntry(FName Id) const { return Entries.Contains(Id); }

	UFUNCTION(BlueprintPure, Category = "QR|Codex")
	FQRCodexEntry GetEntry(FName Id) const;

	UFUNCTION(BlueprintPure, Category = "QR|Codex")
	TArray<FQRCodexEntry> GetEntriesByCategory(FName Category) const;

	UFUNCTION(BlueprintPure, Category = "QR|Codex")
	TArray<FName> GetKnownCategories() const;

	UFUNCTION(BlueprintPure, Category = "QR|Codex")
	int32 CountByCategoryAndState(FName Category, EQRCodexDiscoveryState State) const;

private:
	UPROPERTY()
	TMap<FName, FQRCodexEntry> Entries;
};
