#include "QRLootLibrary.h"
#include "QRInventoryComponent.h"
#include "QRItemDefinition.h"

const FQRLootTableRow* UQRLootLibrary::FindRow(const UDataTable* Table, FName RowName)
{
	if (!Table || RowName.IsNone()) return nullptr;
	return Table->FindRow<FQRLootTableRow>(RowName, TEXT("QRLoot"), /*bWarnIfRowMissing*/ false);
}

bool UQRLootLibrary::EntryMatchesTier(const FQRLootEntry& Entry, int32 Tier)
{
	// MinTier=MaxTier=0 disables the filter entirely so old data without
	// tier columns still rolls.
	if (Entry.MinTier == 0 && Entry.MaxTier == 0) return true;
	return Tier >= Entry.MinTier && Tier <= Entry.MaxTier;
}

int32 UQRLootLibrary::GetTotalEntryWeight(const FQRLootTableRow& Row, int32 Tier)
{
	int32 Sum = 0;
	for (const FQRLootEntry& E : Row.Entries)
	{
		if (E.Weight <= 0) continue;
		if (!EntryMatchesTier(E, Tier)) continue;
		Sum += E.Weight;
	}
	return Sum;
}

TArray<FQRRolledLoot> UQRLootLibrary::RollTable(const UDataTable* Table, FName RowName, int32 Tier)
{
	// Non-seeded path uses the global FMath RNG. Wrap it in an FRandomStream
	// seeded from FMath::Rand so the seeded code path stays the only
	// implementation — keeps determinism semantics consistent.
	FRandomStream Stream(FMath::Rand());
	return RollTableSeeded(Table, RowName, Tier, Stream);
}

TArray<FQRRolledLoot> UQRLootLibrary::RollTableSeeded(const UDataTable* Table, FName RowName,
                                                       int32 Tier, FRandomStream& Stream)
{
	TArray<FQRRolledLoot> Results;
	const FQRLootTableRow* Row = FindRow(Table, RowName);
	if (!Row || Row->Entries.Num() == 0 || Row->RollCount <= 0) return Results;

	// Local mutable copy of entries so we can implement bAllowDuplicates=false
	// by zeroing out an entry's weight after it rolls.
	TArray<FQRLootEntry> Pool = Row->Entries;
	const int32 Rolls = FMath::Clamp(Row->RollCount, 1, 20);

	for (int32 i = 0; i < Rolls; ++i)
	{
		// Compute remaining valid weight for this roll.
		int32 TotalWeight = 0;
		for (const FQRLootEntry& E : Pool)
		{
			if (E.Weight <= 0) continue;
			if (!EntryMatchesTier(E, Tier)) continue;
			TotalWeight += E.Weight;
		}
		if (TotalWeight <= 0) break; // nothing rollable left

		int32 Pick = Stream.RandRange(1, TotalWeight);
		int32 PickedIdx = INDEX_NONE;
		int32 Cursor = 0;
		for (int32 k = 0; k < Pool.Num(); ++k)
		{
			if (Pool[k].Weight <= 0) continue;
			if (!EntryMatchesTier(Pool[k], Tier)) continue;
			Cursor += Pool[k].Weight;
			if (Cursor >= Pick) { PickedIdx = k; break; }
		}
		if (PickedIdx == INDEX_NONE) break;

		const FQRLootEntry& Picked = Pool[PickedIdx];

		// Per-entry DropChance — fizzles harmlessly without consuming the slot
		// when allow-duplicates is on, but still consumes it when off (so a
		// rare item rolling and fizzling doesn't dominate the table).
		const bool bDropped = (Picked.DropChance >= 1.0f) || (Stream.FRand() < Picked.DropChance);
		if (bDropped)
		{
			FQRRolledLoot R;
			R.ItemId = Picked.ItemId;
			R.Quantity = Stream.RandRange(FMath::Max(Picked.MinQuantity, 1),
			                                FMath::Max(Picked.MaxQuantity, Picked.MinQuantity));
			Results.Add(R);
		}

		if (!Row->bAllowDuplicates)
		{
			// Zero the weight so this entry can't roll again.
			Pool[PickedIdx].Weight = 0;
		}
	}

	return Results;
}

int32 UQRLootLibrary::RollAndDeposit(const UDataTable* Table, FName RowName, int32 Tier,
                                      UQRInventoryComponent* Target,
                                      const UDataTable* ItemDefinitionTable)
{
	if (!Target) return 0;
	const TArray<FQRRolledLoot> Rolled = RollTable(Table, RowName, Tier);
	int32 Added = 0;

	for (const FQRRolledLoot& R : Rolled)
	{
		if (R.ItemId.IsNone() || R.Quantity <= 0) continue;
		// Look up the item definition. If no item-definition table was
		// provided we can't spawn — caller has to supply one.
		if (!ItemDefinitionTable) continue;

		// Item definition rows are expected to be FName-keyed entries that
		// resolve to a UQRItemDefinition pointer in a UPROPERTY column called
		// "Definition" (project convention). Fall back to nullptr if the
		// project hasn't set up that table — the caller can do the lookup
		// themselves and call TryAddByDefinition directly.
		struct FQRItemDefinitionTableRow : public FTableRowBase
		{
			TObjectPtr<UQRItemDefinition> Definition = nullptr;
		};
		const FQRItemDefinitionTableRow* DefRow =
			ItemDefinitionTable->FindRow<FQRItemDefinitionTableRow>(R.ItemId, TEXT("QRLootDeposit"), false);
		if (!DefRow || !DefRow->Definition) continue;

		int32 Remainder = 0;
		Target->TryAddByDefinition(DefRow->Definition, R.Quantity, Remainder);
		Added += (R.Quantity - Remainder);
	}

	return Added;
}
