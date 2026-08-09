#include "QRLootContainerComponent.h"
#include "QRLootedRegistry.h"
#include "QRLootLibrary.h"
#include "QRInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UQRLootContainerComponent::UQRLootContainerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQRLootContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRLootContainerComponent, bHasBeenLooted);
}

void UQRLootContainerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-assign a stable id if the designer left it blank. Derive it
	// from the owner's path name instead of FGuid::NewGuid() — a random
	// guid regenerated every session, so the LootedRegistry never matched
	// and every "looted" container came back full after a restart.
	// Worldgen spawns actors deterministically from the seed, so path
	// names reproduce across sessions for spawned containers too.
	if (!UniqueId.IsValid())
	{
		const FString StablePath = GetOwner() ? GetOwner()->GetPathName() : GetPathName();
		const uint32 HashA = GetTypeHash(StablePath);
		const uint32 HashB = GetTypeHash(StablePath + TEXT("::QRLOOT"));
		UniqueId = FGuid(HashA, HashB, 0x51524C54u, static_cast<uint32>(StablePath.Len()));
	}

	// Ask the registry whether we've been looted in a previous session.
	if (UWorld* W = GetWorld())
	{
		if (UQRLootedRegistry* Registry = W->GetSubsystem<UQRLootedRegistry>())
		{
			if (Registry->HasBeenLooted(UniqueId))
			{
				bHasBeenLooted = true;
			}
		}
	}
}

bool UQRLootContainerComponent::TryLoot(AActor* Looter)
{
	if (bHasBeenLooted) return false;
	if (!Looter) return false;

	UQRInventoryComponent* Inv = Looter->FindComponentByClass<UQRInventoryComponent>();
	if (!Inv) return false;

	// Roll the loot table and deposit into the looter's inventory.
	const int32 Added = UQRLootLibrary::RollAndDeposit(
		LootTable, LootTableRowId, LootTier, Inv, ItemDefinitionTable);

	// Mark looted regardless of whether anything was added — the player
	// chose to open this; the table might just have rolled nothing
	// (probabilistically valid result). Marking it looted prevents
	// repeated attempts on a same-session save.
	bHasBeenLooted = true;
	if (UWorld* W = GetWorld())
	{
		if (UQRLootedRegistry* Registry = W->GetSubsystem<UQRLootedRegistry>())
		{
			Registry->MarkLooted(UniqueId);
		}
	}

	OnContainerLooted.Broadcast(Looter, Added);
	return true;
}
