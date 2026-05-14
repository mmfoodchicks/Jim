#include "QRLootedRegistry.h"

bool UQRLootedRegistry::HasBeenLooted(const FGuid& ContainerId) const
{
	return LootedIds.Contains(ContainerId);
}

void UQRLootedRegistry::MarkLooted(const FGuid& ContainerId)
{
	if (ContainerId.IsValid()) LootedIds.Add(ContainerId);
}

void UQRLootedRegistry::Clear()
{
	LootedIds.Reset();
}

TArray<FGuid> UQRLootedRegistry::ExportLootedIds() const
{
	return LootedIds.Array();
}

void UQRLootedRegistry::ImportLootedIds(const TArray<FGuid>& InIds)
{
	LootedIds.Reset();
	for (const FGuid& Id : InIds)
	{
		if (Id.IsValid()) LootedIds.Add(Id);
	}
}
