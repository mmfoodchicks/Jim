#include "QRCodexSubsystem.h"
#include "Misc/DateTime.h"


bool UQRCodexSubsystem::Record(FName Id, FName Category, FText DisplayName,
	EQRCodexDiscoveryState NewState)
{
	if (Id.IsNone()) return false;
	FQRCodexEntry& E = Entries.FindOrAdd(Id);
	const EQRCodexDiscoveryState OldState = E.State;

	if (E.Id.IsNone())
	{
		E.Id          = Id;
		E.Category    = Category;
		E.DisplayName = DisplayName;
		E.FirstSeen   = FDateTime::UtcNow();
	}
	++E.SeenCount;

	// State is monotonic — never regresses.
	if (static_cast<uint8>(NewState) > static_cast<uint8>(E.State))
	{
		E.State = NewState;
	}

	if (E.State != OldState)
	{
		OnEntryUpdated.Broadcast(Id, E.State);
		return true;
	}
	return false;
}


FQRCodexEntry UQRCodexSubsystem::GetEntry(FName Id) const
{
	if (const FQRCodexEntry* Found = Entries.Find(Id)) return *Found;
	return FQRCodexEntry();
}


TArray<FQRCodexEntry> UQRCodexSubsystem::GetEntriesByCategory(FName Category) const
{
	TArray<FQRCodexEntry> Out;
	for (const TPair<FName, FQRCodexEntry>& KV : Entries)
	{
		if (KV.Value.Category == Category) Out.Add(KV.Value);
	}
	Out.Sort([](const FQRCodexEntry& A, const FQRCodexEntry& B)
	{
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	return Out;
}


TArray<FName> UQRCodexSubsystem::GetKnownCategories() const
{
	TSet<FName> Set;
	for (const TPair<FName, FQRCodexEntry>& KV : Entries)
	{
		Set.Add(KV.Value.Category);
	}
	TArray<FName> Out = Set.Array();
	Out.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Out;
}


int32 UQRCodexSubsystem::CountByCategoryAndState(FName Category,
	EQRCodexDiscoveryState State) const
{
	int32 Count = 0;
	for (const TPair<FName, FQRCodexEntry>& KV : Entries)
	{
		if (KV.Value.Category == Category && KV.Value.State == State) ++Count;
	}
	return Count;
}
