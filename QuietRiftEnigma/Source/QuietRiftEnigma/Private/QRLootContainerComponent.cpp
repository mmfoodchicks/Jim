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

	// Auto-assign a stable id if the designer left it blank. Editor-placed
	// containers should leave this auto so each placement serializes its
	// own assigned id; runtime-spawned containers (e.g. from a POI spawner)
	// can also rely on this path.
	if (!UniqueId.IsValid())
	{
		UniqueId = FGuid::NewGuid();
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
