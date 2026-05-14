#include "QRHotbarComponent.h"
#include "QRWorldItem.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UQRHotbarComponent::UQRHotbarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Slots.SetNum(NumSlots);
}

void UQRHotbarComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Slots.Num() != NumSlots) Slots.SetNum(NumSlots);
}

void UQRHotbarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRHotbarComponent, Slots);
	DOREPLIFETIME(UQRHotbarComponent, ActiveSlotIndex);
}

UQRInventoryComponent* UQRHotbarComponent::GetOwnerInventory() const
{
	const AActor* O = GetOwner();
	return O ? O->FindComponentByClass<UQRInventoryComponent>() : nullptr;
}

UQRItemInstance* UQRHotbarComponent::GetSlot(int32 SlotIndex) const
{
	if (!Slots.IsValidIndex(SlotIndex)) return nullptr;
	return Slots[SlotIndex];
}

UQRItemInstance* UQRHotbarComponent::GetActiveItem() const
{
	return GetSlot(ActiveSlotIndex);
}

void UQRHotbarComponent::AssignSlot(int32 SlotIndex, UQRItemInstance* Item)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;
	Slots[SlotIndex] = Item;
	OnSlotChanged.Broadcast(SlotIndex, Item);
	if (SlotIndex == ActiveSlotIndex) ApplyActiveSlotToHand();
}

bool UQRHotbarComponent::AssignDefinitionToSlot(int32 SlotIndex, const UQRItemDefinition* Definition,
	int32 Quantity)
{
	if (!Slots.IsValidIndex(SlotIndex) || !Definition) return false;

	UQRInventoryComponent* Inv = GetOwnerInventory();
	if (!Inv) return false;

	int32 Remainder = 0;
	const EQRInventoryResult R = Inv->TryAddByDefinition(Definition, Quantity, Remainder);
	if (R != EQRInventoryResult::Success) return false;

	// Find the freshly-added instance: scan the inventory back-to-front for
	// the matching item id with at least one unit. TryAddByDefinition may
	// have stacked into an existing instance, in which case the slot binds
	// to that stack — same item, so the user gets what they asked for.
	UQRItemInstance* Added = nullptr;
	for (int32 i = Inv->Items.Num() - 1; i >= 0; --i)
	{
		UQRItemInstance* It = Inv->Items[i];
		if (It && It->Definition && It->Definition->ItemId == Definition->ItemId)
		{
			Added = It;
			break;
		}
	}
	if (!Added) return false;

	AssignSlot(SlotIndex, Added);
	return true;
}

void UQRHotbarComponent::SelectSlot(int32 SlotIndex)
{
	if (!GetOwner()) return;
	if (!GetOwner()->HasAuthority())
	{
		Server_SelectSlot(SlotIndex);
		return;
	}

	// Re-selecting the active slot toggles back to "hands empty".
	const int32 NewIndex = (SlotIndex == ActiveSlotIndex) ? -1 : SlotIndex;
	if (NewIndex != -1 && !Slots.IsValidIndex(NewIndex)) return;

	ActiveSlotIndex = NewIndex;
	ApplyActiveSlotToHand();
	OnActiveSlotChanged.Broadcast(ActiveSlotIndex);
}

void UQRHotbarComponent::Server_SelectSlot_Implementation(int32 SlotIndex)
{
	SelectSlot(SlotIndex);
}

void UQRHotbarComponent::SelectNext()
{
	int32 Next = ActiveSlotIndex + 1;
	if (Next >= NumSlots) Next = 0;
	SelectSlot(Next);
}

void UQRHotbarComponent::SelectPrev()
{
	int32 Prev = ActiveSlotIndex - 1;
	if (Prev < 0) Prev = NumSlots - 1;
	SelectSlot(Prev);
}

void UQRHotbarComponent::ApplyActiveSlotToHand()
{
	UQRInventoryComponent* Inv = GetOwnerInventory();
	if (!Inv) return;

	if (ActiveSlotIndex >= 0 && Slots.IsValidIndex(ActiveSlotIndex) && Slots[ActiveSlotIndex])
	{
		Inv->TryEquipToHandSlot(Slots[ActiveSlotIndex]);
	}
	else
	{
		Inv->ClearHandSlot();
	}
}

AQRWorldItem* UQRHotbarComponent::DropActiveItem(int32 QuantityToDrop)
{
	UQRItemInstance* Held = GetActiveItem();
	if (!Held || !Held->IsValid()) return nullptr;

	UQRInventoryComponent* Inv = GetOwnerInventory();
	if (!Inv) return nullptr;

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner) return nullptr;

	const int32 Qty = (QuantityToDrop <= 0) ? Held->Quantity : FMath::Min(QuantityToDrop, Held->Quantity);
	if (Qty <= 0) return nullptr;

	// Spawn just in front of the owner so the dropped item doesn't clip into
	// their capsule. Half a meter forward + a bit up.
	const FVector SpawnLoc = Owner->GetActorLocation()
		+ Owner->GetActorForwardVector() * 80.0f
		+ FVector(0, 0, 20.0f);
	const FRotator SpawnRot = FRotator::ZeroRotator;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = Owner;

	AQRWorldItem* World_Item = World->SpawnActor<AQRWorldItem>(
		AQRWorldItem::StaticClass(), SpawnLoc, SpawnRot, Params);
	if (!World_Item) return nullptr;

	World_Item->InitializeFrom(Held->Definition, Qty);

	// Remove from inventory. Track whether this empties the held stack so
	// we can clear the slot reference (otherwise we'd leave a dangling ref
	// to a quantity-0 instance in the hotbar).
	Inv->TryRemoveItem(Held->Definition->ItemId, Qty);

	if (!Held->IsValid())
	{
		// Stack emptied — drop the slot binding and refresh the held mesh.
		Slots[ActiveSlotIndex] = nullptr;
		OnSlotChanged.Broadcast(ActiveSlotIndex, nullptr);
		ApplyActiveSlotToHand();
	}

	return World_Item;
}

void UQRHotbarComponent::OnRep_Slots()
{
	// Broadcast a generic refresh — UI bound to OnSlotChanged for any change
	// can re-pull all 9. We use -1 to mean "any/all".
	OnSlotChanged.Broadcast(-1, nullptr);
}

void UQRHotbarComponent::OnRep_ActiveSlot()
{
	OnActiveSlotChanged.Broadcast(ActiveSlotIndex);
}
