#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRMath.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"

UQRInventoryComponent::UQRInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 5.0f; // spoil ticks every 5 real-seconds
	SetIsReplicatedByDefault(true);
}

void UQRInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRInventoryComponent, Items);
	DOREPLIFETIME(UQRInventoryComponent, HandSlot);
	DOREPLIFETIME(UQRInventoryComponent, OffhandSlot);
	DOREPLIFETIME(UQRInventoryComponent, HandsSlotState);
	DOREPLIFETIME(UQRInventoryComponent, ShoulderStackMax);
	DOREPLIFETIME(UQRInventoryComponent, MaxCarryWeightKg);
	DOREPLIFETIME(UQRInventoryComponent, MaxVolumeLiters);
	DOREPLIFETIME(UQRInventoryComponent, MaxSlots);
	DOREPLIFETIME(UQRInventoryComponent, EquippedChestRig);
	DOREPLIFETIME(UQRInventoryComponent, EquippedBackpack);
	DOREPLIFETIME(UQRInventoryComponent, EquippedHelm);
	DOREPLIFETIME(UQRInventoryComponent, EquippedChestArmour);
	DOREPLIFETIME(UQRInventoryComponent, EquippedLegsArmour);
	DOREPLIFETIME(UQRInventoryComponent, BaseCarryWeightKg);
	DOREPLIFETIME(UQRInventoryComponent, BaseVolumeLiters);
	DOREPLIFETIME(UQRInventoryComponent, BaseSlots);
}

bool UQRInventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch,
	FReplicationFlags* RepFlags)
{
	bool bWrote = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	for (UQRItemInstance* Item : Items)
	{
		if (Item) bWrote |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
	}
	TArray<UQRItemInstance*> Equipped;
	GetEquippedInstances(Equipped);
	for (UQRItemInstance* Item : Equipped)
	{
		bWrote |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
	}
	return bWrote;
}

void UQRInventoryComponent::GetEquippedInstances(TArray<UQRItemInstance*>& Out) const
{
	auto Add = [&Out](UQRItemInstance* I) { if (I) Out.Add(I); };
	Add(HandSlot);
	Add(OffhandSlot);
	Add(EquippedHelm);
	Add(EquippedChestArmour);
	Add(EquippedLegsArmour);
	Add(EquippedChestRig);
	Add(EquippedBackpack);
}

void UQRInventoryComponent::ReturnInstanceToGrid(UQRItemInstance* Item)
{
	if (!Item) return;
	Items.AddUnique(Item);
	// The placement it had before being equipped is stale — those cells may
	// be occupied by something else now. Clear and re-place fresh.
	Item->ContainerKind = EQRContainerKind::None;
	Item->GridX = -1;
	Item->GridY = -1;
	(void)TryAutoPlaceItem(Item);
}

EQRInventoryResult UQRInventoryComponent::TryAddItem(UQRItemInstance* Item, int32& OutRemainder)
{
	if (!Item || !Item->Definition)
	{
		OutRemainder = Item ? Item->Quantity : 0;
		return EQRInventoryResult::InvalidItem;
	}

	// Clamp at runtime so malformed CSV data (negative MassKg/VolumeLiters) can't bypass limits
	const float MassKg    = FMath::Max(Item->Definition->MassKg,    0.001f);
	const float VolLiters = FMath::Max(Item->Definition->VolumeLiters, 0.001f);

	// Guard against 0 or negative stack size from malformed definition (would cause infinite loop)
	const int32 StackMax = FMath::Max(Item->Definition->MaxStackSize, 1);

	// Compute how many units capacity actually allows right now
	const float FreeWeight = FMath::Max(MaxCarryWeightKg - GetCurrentWeightKg(), 0.0f);
	const float FreeVolume = FMath::Max(MaxVolumeLiters  - GetCurrentVolumeLiters(), 0.0f);
	const int32 MaxByWeight = FMath::FloorToInt(FreeWeight / MassKg);
	const int32 MaxByVolume = FMath::FloorToInt(FreeVolume / VolLiters);
	const int32 MaxFit = FMath::Min(Item->Quantity, FMath::Min(MaxByWeight, MaxByVolume));

	if (MaxFit <= 0)
	{
		OutRemainder = Item->Quantity;
		return EQRInventoryResult::TooHeavy;
	}

	int32 Remaining = MaxFit;

	// Try to merge into existing stacks first
	if (StackMax > 1)
	{
		if (UQRItemInstance* Existing = FindExistingStack(Item->Definition->ItemId, StackMax))
		{
			int32 Space = StackMax - Existing->Quantity;
			int32 ToAdd = FMath::Min(Space, Remaining);
			Existing->Quantity += ToAdd;
			Remaining -= ToAdd;
		}
	}

	// Open new slots for remainder — check slot count AND re-verify capacity each iteration
	while (Remaining > 0 && Items.Num() < MaxSlots)
	{
		// Re-check capacity so partial adds stay honest (bulk of guard already done above, but
		// protecting against future multi-threaded callers and accumulated rounding drift)
		const float FreeW = FMath::Max(MaxCarryWeightKg - GetCurrentWeightKg(), 0.0f);
		const float FreeV = FMath::Max(MaxVolumeLiters  - GetCurrentVolumeLiters(), 0.0f);
		const int32 SlotMax = FMath::Min(StackMax, FMath::Min(FMath::FloorToInt(FreeW / MassKg),
		                                                        FMath::FloorToInt(FreeV / VolLiters)));
		if (SlotMax <= 0) break;

		UQRItemInstance* NewInst = NewObject<UQRItemInstance>(this);
		NewInst->Initialize(Item->Definition, FMath::Min(Remaining, SlotMax));
		NewInst->SpoilProgress  = Item->SpoilProgress;
		NewInst->EdibilityState = Item->EdibilityState;

		int32 SlotIdx = Items.Add(NewInst);
		Remaining -= NewInst->Quantity;
		// Auto-place the new instance in the first free cell across grids.
		// If no grid has room (rare — slot count would have rejected first),
		// the item stays in Items with ContainerKind=None until a UI move.
		(void)TryAutoPlaceItem(NewInst);
		OnItemAdded.Broadcast(NewInst, SlotIdx);
	}

	// Any quantity that couldn't fit (slots full or capacity hit mid-loop)
	OutRemainder = Item->Quantity - MaxFit + Remaining;
	OnInventoryChanged.Broadcast();
	return OutRemainder == 0 ? EQRInventoryResult::Success : EQRInventoryResult::Full;
}

EQRInventoryResult UQRInventoryComponent::TryAddByDefinition(const UQRItemDefinition* Def, int32 Quantity, int32& OutRemainder)
{
	if (!Def)
	{
		OutRemainder = Quantity;
		return EQRInventoryResult::InvalidItem;
	}

	// Reject nonsensical quantities up-front — prevents INT_MAX overflow in mass*qty
	if (Quantity <= 0 || Quantity > 9999)
	{
		OutRemainder = Quantity;
		return EQRInventoryResult::InvalidItem;
	}

	// Parent to this component (not the owning actor) so the staging
	// instance has a stable outer while TryAddItem copies it into real
	// stacks; it becomes unreferenced garbage afterwards either way.
	UQRItemInstance* Temp = NewObject<UQRItemInstance>(this);
	Temp->Initialize(Def, Quantity);
	return TryAddItem(Temp, OutRemainder);
}

UQRItemInstance* UQRInventoryComponent::ForceAddByDefinition(const UQRItemDefinition* Def, int32 Quantity)
{
	if (!Def || Quantity <= 0 || Quantity > 9999) return nullptr;

	// Sidestep the weight/volume check — Wildlife items in particular
	// weigh 50 kg and bust the player's default carry limit, so the
	// regular TryAddByDefinition path returns TooHeavy and the creative
	// hotbar slot stays empty. Here we just construct an instance and
	// drop it into Items[] directly.
	UQRItemInstance* Inst = NewObject<UQRItemInstance>(this);
	Inst->Initialize(Def, Quantity);
	const int32 SlotIdx = Items.Add(Inst);

	// Try to place into a grid cell for the inventory UI; harmless if
	// the grid is full.
	(void)TryAutoPlaceItem(Inst);
	OnItemAdded.Broadcast(Inst, SlotIdx);
	OnInventoryChanged.Broadcast();
	return Inst;
}

bool UQRInventoryComponent::TryRemoveItem(FName ItemId, int32 Quantity)
{
	if (CountItem(ItemId) < Quantity)
		return false;

	int32 Remaining = Quantity;
	for (int32 i = Items.Num() - 1; i >= 0 && Remaining > 0; --i)
	{
		UQRItemInstance* Inst = Items[i];
		// Guard Definition — replicated instances may arrive before their Definition asset resolves
		if (!Inst || !Inst->Definition || Inst->Definition->ItemId != ItemId) continue;

		int32 ToRemove = FMath::Min(Inst->Quantity, Remaining);
		Inst->Quantity -= ToRemove;
		Remaining -= ToRemove;
		OnItemRemoved.Broadcast(Inst, ToRemove);

		if (Inst->Quantity <= 0)
			Items.RemoveAt(i);
	}

	// Grid stacks exhausted — drain equipped/held instances last so loose
	// copies are always consumed before the one you're wearing/wielding.
	if (Remaining > 0)
	{
		auto DrainSlot = [&](TObjectPtr<UQRItemInstance>& SlotRef)
		{
			if (Remaining <= 0 || !SlotRef || !SlotRef->Definition) return;
			if (SlotRef->Definition->ItemId != ItemId) return;
			const int32 ToRemove = FMath::Min(SlotRef->Quantity, Remaining);
			SlotRef->Quantity -= ToRemove;
			Remaining -= ToRemove;
			OnItemRemoved.Broadcast(SlotRef, ToRemove);
			if (SlotRef->Quantity <= 0)
			{
				if (SlotRef == HandSlot) HandsSlotState = EQRHandsSlotState::Empty;
				SlotRef = nullptr;
			}
		};
		DrainSlot(HandSlot);
		DrainSlot(OffhandSlot);
		DrainSlot(EquippedHelm);
		DrainSlot(EquippedChestArmour);
		DrainSlot(EquippedLegsArmour);
		// Worn rig/backpack are deliberately NOT drainable here — destroying
		// an equipped container would orphan the items placed in its grid.
		// Unequip first; the loose container is then a normal grid item.
	}

	OnInventoryChanged.Broadcast();
	return Remaining == 0;
}

bool UQRInventoryComponent::TryEquipToHandSlot(UQRItemInstance* Item)
{
	if (!Item || !Item->IsValid()) return false;
	if (HandsSlotState == EQRHandsSlotState::LockedByAction) return false;
	if (Item->Definition && Item->Definition->bIsBulkItem && HandsSlotState == EQRHandsSlotState::Occupied)
		return false;

	// Already holding this exact instance — nothing to do.
	if (HandSlot == Item) return true;

	// Return the previously-held item to the grid before we overwrite it.
	// Without this, switching hotbar slot 1 (weapon) -> 2 (animal) -> 1
	// fails on the re-equip because the weapon instance was orphaned out
	// of Items[] the first time and IndexOfByKey can't find it for the
	// re-equip.
	if (HandSlot && HandSlot->IsValid())
	{
		ReturnInstanceToGrid(HandSlot);
	}

	// Take the new instance out of the grid if it's there. Tolerates the
	// case where Item already came from outside Items (e.g. a hotbar slot
	// pointing at an instance that was previously held). Its grid placement
	// is cleared — the cells it occupied are free while it's wielded.
	const int32 SlotIdx = Items.IndexOfByKey(Item);
	if (SlotIdx != INDEX_NONE)
	{
		Items.RemoveAt(SlotIdx);
	}
	Item->ContainerKind = EQRContainerKind::None;
	Item->GridX = -1;
	Item->GridY = -1;

	HandSlot = Item;
	HandsSlotState = EQRHandsSlotState::Occupied;

	// Two-handed primary forces the offhand back into the grid -- you can't
	// hold a shield while wielding a rifle. Return it to Items so it's still
	// in the inventory, just no longer wielded.
	if (Item->Definition && Item->Definition->bIsTwoHanded && OffhandSlot)
	{
		if (OffhandSlot->IsValid()) ReturnInstanceToGrid(OffhandSlot);
		OffhandSlot = nullptr;
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UQRInventoryComponent::TryEquipToOffhand(UQRItemInstance* Item)
{
	if (!Item || !Item->IsValid()) return false;
	// Block when the primary is two-handed.
	if (HandSlot && HandSlot->Definition && HandSlot->Definition->bIsTwoHanded)
	{
		return false;
	}
	if (OffhandSlot == Item) return true;

	if (OffhandSlot && OffhandSlot->IsValid())
	{
		ReturnInstanceToGrid(OffhandSlot);
	}
	const int32 SlotIdx = Items.IndexOfByKey(Item);
	if (SlotIdx != INDEX_NONE)
	{
		Items.RemoveAt(SlotIdx);
	}
	Item->ContainerKind = EQRContainerKind::None;
	Item->GridX = -1;
	Item->GridY = -1;
	OffhandSlot = Item;
	OnInventoryChanged.Broadcast();
	return true;
}

void UQRInventoryComponent::ClearOffhand()
{
	// Same instance returns to the grid — NOT TryAddItem, which would copy
	// quantity into fresh stacks and silently drop per-instance state like
	// durability while orphaning the original object.
	if (OffhandSlot && OffhandSlot->IsValid())
	{
		ReturnInstanceToGrid(OffhandSlot);
	}
	OffhandSlot = nullptr;
	OnInventoryChanged.Broadcast();
}

void UQRInventoryComponent::ClearHandSlot()
{
	// Same instance returns to the grid (see ClearOffhand note) — preserves
	// durability/spoil and keeps hotbar slots pointing at a live instance.
	if (HandSlot && HandSlot->IsValid())
	{
		ReturnInstanceToGrid(HandSlot);
	}
	HandSlot = nullptr;
	HandsSlotState = EQRHandsSlotState::Empty;
	OnInventoryChanged.Broadcast();
}

void UQRInventoryComponent::ForceUnlockHandsSlot()
{
	// Recovery path for interrupted actions (death, save-load, ability cancel)
	// Does NOT try to return a hand item — just drops the lock.
	if (HandsSlotState == EQRHandsSlotState::LockedByAction)
	{
		HandsSlotState = HandSlot ? EQRHandsSlotState::Occupied : EQRHandsSlotState::Empty;
		OnInventoryChanged.Broadcast();
	}
}

int32 UQRInventoryComponent::CountItem(FName ItemId) const
{
	int32 Total = 0;
	for (const UQRItemInstance* Inst : Items)
	{
		if (Inst && Inst->Definition && Inst->Definition->ItemId == ItemId)
			Total += Inst->Quantity;
	}
	// Equipped/held instances count too — otherwise "has the player got a
	// torch?" says no while one is literally in their hand, and crafting/
	// quest checks desync from TryRemoveItem (which can consume equipped).
	// Worn rig/backpack are excluded to mirror TryRemoveItem, which refuses
	// to drain them (destroying a worn container would orphan its contents).
	const UQRItemInstance* Countable[5] = {
		HandSlot, OffhandSlot, EquippedHelm, EquippedChestArmour, EquippedLegsArmour };
	for (const UQRItemInstance* Inst : Countable)
	{
		if (Inst && Inst->Definition && Inst->Definition->ItemId == ItemId)
			Total += Inst->Quantity;
	}
	return Total;
}

bool UQRInventoryComponent::HasItem(FName ItemId, int32 MinQuantity) const
{
	return CountItem(ItemId) >= MinQuantity;
}

float UQRInventoryComponent::GetCurrentWeightKg() const
{
	float Total = 0.0f;
	for (const UQRItemInstance* Inst : Items)
	{
		if (Inst && Inst->Definition)
			Total += Inst->Definition->MassKg * Inst->Quantity;
	}
	// Equipped gear lives outside Items[] but you're still carrying it — a
	// 4 kg rifle in hand or a 3 kg rig on your chest counts toward
	// encumbrance just like it would loose in the pack.
	TArray<UQRItemInstance*> Equipped;
	GetEquippedInstances(Equipped);
	for (const UQRItemInstance* Inst : Equipped)
	{
		if (Inst->Definition)
			Total += Inst->Definition->MassKg * Inst->Quantity;
	}
	return Total;
}

float UQRInventoryComponent::GetCurrentVolumeLiters() const
{
	float Total = 0.0f;
	for (const UQRItemInstance* Inst : Items)
	{
		if (Inst && Inst->Definition)
			Total += Inst->Definition->VolumeLiters * Inst->Quantity;
	}
	// Worn/held gear doesn't consume pack volume — it's on your body, not
	// in a bag. Weight counts (see above); volume intentionally does not.
	return Total;
}

bool UQRInventoryComponent::IsOverEncumbered() const
{
	return GetCurrentWeightKg() > MaxCarryWeightKg || GetCurrentVolumeLiters() > MaxVolumeLiters;
}

bool UQRInventoryComponent::IsSprintBlocked() const
{
	return UQRMath::EncumbranceRatio(GetCurrentWeightKg(), MaxCarryWeightKg) >= SprintEncumbranceRatio;
}

void UQRInventoryComponent::SetShoulderStackFromSTR(int32 STR)
{
	ShoulderStackMax = UQRMath::ShoulderStackMax(STR);
}

void UQRInventoryComponent::SetCarryCapacityFromSTR(int32 STR)
{
	BaseCarryWeightKg = UQRMath::CarryCapacityKg(STR);
	RecomputeCapacityFromContainers();
}

void UQRInventoryComponent::RecomputeCapacityFromContainers()
{
	float WeightBonus = 0.0f;
	float VolumeBonus = 0.0f;
	int32 SlotBonus   = 0;

	auto AddBonusFor = [&](UQRItemInstance* Container)
	{
		if (!Container || !Container->Definition) return;
		const UQRItemDefinition* Def = Container->Definition;
		if (Def->ContainerSlot == EQRContainerSlotType::None) return;
		WeightBonus += FMath::Max(Def->ContainerCarryBonusKg, 0.0f);
		VolumeBonus += FMath::Max(Def->ContainerVolumeBonusLiters, 0.0f);
		SlotBonus   += FMath::Max(Def->ContainerGridW * Def->ContainerGridH, 0);
	};

	AddBonusFor(EquippedChestRig);
	AddBonusFor(EquippedBackpack);

	MaxCarryWeightKg = BaseCarryWeightKg + WeightBonus;
	MaxVolumeLiters  = BaseVolumeLiters  + VolumeBonus;
	MaxSlots         = BaseSlots         + SlotBonus;

	OnInventoryChanged.Broadcast();
}

UQRItemInstance* UQRInventoryComponent::GetEquippedContainer(EQRContainerSlotType Slot) const
{
	switch (Slot)
	{
		case EQRContainerSlotType::ChestRig: return EquippedChestRig;
		case EQRContainerSlotType::Backpack: return EquippedBackpack;
		default:                              return nullptr;
	}
}

EQRInventoryResult UQRInventoryComponent::TryEquipContainer(UQRItemInstance* Item)
{
	if (!Item || !Item->Definition) return EQRInventoryResult::InvalidItem;

	const EQRContainerSlotType Slot = Item->Definition->ContainerSlot;
	if (Slot != EQRContainerSlotType::ChestRig && Slot != EQRContainerSlotType::Backpack)
		return EQRInventoryResult::WrongSlot;

	if (GetEquippedContainer(Slot) != nullptr) return EQRInventoryResult::SlotOccupied;

	// Detach from flat inventory if it's living there. Don't touch the array
	// otherwise — the caller may pass a freshly-spawned instance from a
	// dropped pickup, world container, or trade flow. Placement clears so
	// the cells it occupied free up while it's worn.
	Items.Remove(Item);
	Item->ContainerKind = EQRContainerKind::None;
	Item->GridX = -1;
	Item->GridY = -1;

	if (Slot == EQRContainerSlotType::ChestRig) EquippedChestRig = Item;
	else                                          EquippedBackpack = Item;

	RecomputeCapacityFromContainers();
	OnItemAdded.Broadcast(Item, /*SlotIndex*/ -1);
	return EQRInventoryResult::Success;
}

EQRInventoryResult UQRInventoryComponent::TryUnequipContainer(EQRContainerSlotType Slot, UQRItemInstance*& OutRemovedContainer)
{
	OutRemovedContainer = nullptr;
	UQRItemInstance* Container = GetEquippedContainer(Slot);
	if (!Container || !Container->Definition) return EQRInventoryResult::InvalidItem;

	// Refuse if any items are still placed in the grid we're about to remove —
	// the player must move them out first (UI prompt) or stash them in body.
	const EQRContainerKind GridKind = (Slot == EQRContainerSlotType::ChestRig)
		? EQRContainerKind::ChestRig : EQRContainerKind::Backpack;
	for (UQRItemInstance* Inst : Items)
	{
		if (Inst && Inst->ContainerKind == GridKind) return EQRInventoryResult::WouldNotFit;
	}

	// Capacity *after* removing the container's bonus. Weight already counts
	// the equipped container (GetCurrentWeightKg includes equip slots), and
	// it keeps counting once it returns to Items — so the weight comparison
	// needs no mass adjustment. The slot check does: the container occupies
	// one flat slot after it returns to the grid.
	const float WouldLoseKg     = FMath::Max(Container->Definition->ContainerCarryBonusKg, 0.0f);
	const float WouldLoseLiters = FMath::Max(Container->Definition->ContainerVolumeBonusLiters, 0.0f);
	const int32 WouldLoseSlots  = FMath::Max(Container->Definition->ContainerGridW * Container->Definition->ContainerGridH, 0);

	const float CurrentWeight = GetCurrentWeightKg();
	const float CurrentVolume = GetCurrentVolumeLiters() +
		FMath::Max(Container->Definition->VolumeLiters, 0.0f);   // it'll take pack volume once stowed
	const int32 SlotsAfter = Items.Num() + 1;                     // container joins Items

	if (CurrentWeight > MaxCarryWeightKg - WouldLoseKg)     return EQRInventoryResult::WouldNotFit;
	if (CurrentVolume > MaxVolumeLiters  - WouldLoseLiters) return EQRInventoryResult::WouldNotFit;
	if (SlotsAfter    > MaxSlots         - WouldLoseSlots)  return EQRInventoryResult::WouldNotFit;

	if (Slot == EQRContainerSlotType::ChestRig) EquippedChestRig = nullptr;
	else                                          EquippedBackpack = nullptr;

	RecomputeCapacityFromContainers();

	// Put the container back into the flat grid. Previously it was only
	// handed back via OutRemovedContainer — and every UI caller ignored
	// that, leaving the instance unreferenced and GC-collectable. Unequip
	// used to silently DESTROY your backpack.
	ReturnInstanceToGrid(Container);

	OutRemovedContainer = Container;
	OnItemRemoved.Broadcast(Container, 1);
	OnInventoryChanged.Broadcast();
	return EQRInventoryResult::Success;
}

TArray<UQRItemInstance*> UQRInventoryComponent::GetItemsByCategory(EQRItemCategory Category) const
{
	TArray<UQRItemInstance*> Result;
	for (UQRItemInstance* Inst : Items)
	{
		if (Inst && Inst->Definition && Inst->Definition->Category == Category)
			Result.Add(Inst);
	}
	return Result;
}

void UQRInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Accumulate real-time into GAME-hours. The old /3600 used REAL hours,
	// so at the 20-minute game day food spoiled ~72× slower than the
	// vitals sim assumes.
	SpoilAccumulatedHours += DeltaTime * SpoilGameHoursPerRealSecond;

	if (SpoilAccumulatedHours >= 0.01f) // advance spoil every ~36 real-seconds
	{
		for (UQRItemInstance* Inst : Items)
		{
			if (Inst) Inst->AdvanceSpoilByHours(SpoilAccumulatedHours);
		}
		SpoilAccumulatedHours = 0.0f;
	}
}

UQRItemInstance* UQRInventoryComponent::FindExistingStack(FName ItemId, int32 MaxStack) const
{
	for (UQRItemInstance* Inst : Items)
	{
		if (Inst && Inst->Definition && Inst->Definition->ItemId == ItemId && Inst->Quantity < MaxStack)
			return Inst;
	}
	return nullptr;
}

// ── Spatial Placement (Tarkov-style) ────────────────────────────────────────

bool UQRInventoryComponent::GetGridSize(EQRContainerKind Kind, int32& OutW, int32& OutH) const
{
	OutW = 0; OutH = 0;
	if (Kind == EQRContainerKind::Body)
	{
		OutW = InventoryGridW;
		OutH = InventoryGridH;
		return OutW > 0 && OutH > 0;
	}
	const UQRItemInstance* Container = nullptr;
	if (Kind == EQRContainerKind::ChestRig) Container = EquippedChestRig;
	else if (Kind == EQRContainerKind::Backpack) Container = EquippedBackpack;
	if (!Container || !Container->Definition) return false;
	OutW = Container->Definition->ContainerGridW;
	OutH = Container->Definition->ContainerGridH;
	return OutW > 0 && OutH > 0;
}

void UQRInventoryComponent::GetItemFootprint(const UQRItemInstance* Item, int32& OutW, int32& OutH) const
{
	OutW = 1; OutH = 1;
	if (!Item || !Item->Definition) return;
	const int32 W = FMath::Max(Item->Definition->GridFootprintW, 1);
	const int32 H = FMath::Max(Item->Definition->GridFootprintH, 1);
	if (Item->bRotated) { OutW = H; OutH = W; }
	else                { OutW = W; OutH = H; }
}

bool UQRInventoryComponent::IsRectFree(EQRContainerKind Kind, int32 X, int32 Y, int32 W, int32 H,
                                        const UQRItemInstance* Ignore) const
{
	int32 GridW = 0, GridH = 0;
	if (!GetGridSize(Kind, GridW, GridH)) return false;
	if (X < 0 || Y < 0 || W <= 0 || H <= 0) return false;
	if (X + W > GridW || Y + H > GridH) return false;

	for (UQRItemInstance* Inst : Items)
	{
		if (!Inst || Inst == Ignore) continue;
		if (Inst->ContainerKind != Kind) continue;
		if (Inst->GridX < 0 || Inst->GridY < 0) continue;
		int32 IW = 0, IH = 0;
		GetItemFootprint(Inst, IW, IH);
		// Standard AABB overlap test in grid space.
		const bool bDisjoint = (X + W <= Inst->GridX) || (Inst->GridX + IW <= X)
		                     || (Y + H <= Inst->GridY) || (Inst->GridY + IH <= Y);
		if (!bDisjoint) return false;
	}
	return true;
}

UQRItemInstance* UQRInventoryComponent::GetItemAt(EQRContainerKind Kind, int32 X, int32 Y) const
{
	int32 GridW = 0, GridH = 0;
	if (!GetGridSize(Kind, GridW, GridH)) return nullptr;
	if (X < 0 || Y < 0 || X >= GridW || Y >= GridH) return nullptr;

	for (UQRItemInstance* Inst : Items)
	{
		if (!Inst || Inst->ContainerKind != Kind) continue;
		if (Inst->GridX < 0 || Inst->GridY < 0) continue;
		int32 IW = 0, IH = 0;
		GetItemFootprint(Inst, IW, IH);
		if (X >= Inst->GridX && X < Inst->GridX + IW &&
		    Y >= Inst->GridY && Y < Inst->GridY + IH)
		{
			return Inst;
		}
	}
	return nullptr;
}

EQRInventoryResult UQRInventoryComponent::TryPlaceItemAt(UQRItemInstance* Item, EQRContainerKind Kind,
                                                          int32 X, int32 Y, bool bRotated)
{
	if (!Item || !Item->Definition) return EQRInventoryResult::InvalidItem;
	if (!Items.Contains(Item))      return EQRInventoryResult::InvalidItem;
	if (Kind == EQRContainerKind::None) return EQRInventoryResult::WrongSlot;

	int32 GridW = 0, GridH = 0;
	if (!GetGridSize(Kind, GridW, GridH)) return EQRInventoryResult::WrongSlot;

	// Compute prospective footprint with the proposed rotation.
	const int32 BaseW = FMath::Max(Item->Definition->GridFootprintW, 1);
	const int32 BaseH = FMath::Max(Item->Definition->GridFootprintH, 1);
	const int32 W = bRotated ? BaseH : BaseW;
	const int32 H = bRotated ? BaseW : BaseH;

	if (!IsRectFree(Kind, X, Y, W, H, /*Ignore*/ Item)) return EQRInventoryResult::Full;

	Item->ContainerKind = Kind;
	Item->GridX         = X;
	Item->GridY         = Y;
	Item->bRotated      = bRotated;
	OnInventoryChanged.Broadcast();
	return EQRInventoryResult::Success;
}

EQRInventoryResult UQRInventoryComponent::TryAutoPlaceItem(UQRItemInstance* Item)
{
	if (!Item || !Item->Definition) return EQRInventoryResult::InvalidItem;
	if (!Items.Contains(Item))      return EQRInventoryResult::InvalidItem;

	const int32 BaseW = FMath::Max(Item->Definition->GridFootprintW, 1);
	const int32 BaseH = FMath::Max(Item->Definition->GridFootprintH, 1);

	// Try each grid in chest-fast → body → backpack-deep order. For each
	// grid try the natural footprint first, then rotated 90°.
	const EQRContainerKind Order[3] = {
		EQRContainerKind::ChestRig, EQRContainerKind::Body, EQRContainerKind::Backpack
	};
	for (EQRContainerKind Kind : Order)
	{
		int32 GridW = 0, GridH = 0;
		if (!GetGridSize(Kind, GridW, GridH)) continue;

		for (int32 Rot = 0; Rot < 2; ++Rot)
		{
			const bool bRotate = (Rot == 1);
			// 1×1 items don't gain anything from rotation — skip the second pass.
			if (bRotate && BaseW == BaseH) break;
			const int32 W = bRotate ? BaseH : BaseW;
			const int32 H = bRotate ? BaseW : BaseH;
			if (W > GridW || H > GridH) continue;

			for (int32 Y = 0; Y <= GridH - H; ++Y)
			for (int32 X = 0; X <= GridW - W; ++X)
			{
				if (IsRectFree(Kind, X, Y, W, H, /*Ignore*/ Item))
				{
					Item->ContainerKind = Kind;
					Item->GridX         = X;
					Item->GridY         = Y;
					Item->bRotated      = bRotate;
					OnInventoryChanged.Broadcast();
					return EQRInventoryResult::Success;
				}
			}
		}
	}
	return EQRInventoryResult::Full;
}

bool UQRInventoryComponent::TryRotateItem(UQRItemInstance* Item)
{
	if (!Item || !Item->Definition) return false;
	if (Item->ContainerKind == EQRContainerKind::None) return false;

	const int32 BaseW = FMath::Max(Item->Definition->GridFootprintW, 1);
	const int32 BaseH = FMath::Max(Item->Definition->GridFootprintH, 1);
	if (BaseW == BaseH) return true; // nothing to rotate

	const bool bNewRotated = !Item->bRotated;
	const int32 NewW = bNewRotated ? BaseH : BaseW;
	const int32 NewH = bNewRotated ? BaseW : BaseH;

	if (!IsRectFree(Item->ContainerKind, Item->GridX, Item->GridY, NewW, NewH, /*Ignore*/ Item))
		return false;

	Item->bRotated = bNewRotated;
	OnInventoryChanged.Broadcast();
	return true;
}

bool UQRInventoryComponent::TryMoveItem(UQRItemInstance* Item, EQRContainerKind NewKind,
                                          int32 NewX, int32 NewY, bool bNewRotated)
{
	if (!Item || !Item->Definition) return false;
	if (NewKind == EQRContainerKind::None) return false;

	// Snapshot original placement so we can roll back atomically on failure.
	const EQRContainerKind OldKind   = Item->ContainerKind;
	const int32           OldX       = Item->GridX;
	const int32           OldY       = Item->GridY;
	const bool             bOldRot   = Item->bRotated;

	// Temporarily detach so the rect-free check ignores its own footprint.
	Item->ContainerKind = EQRContainerKind::None;
	Item->GridX = -1; Item->GridY = -1;

	const int32 BaseW = FMath::Max(Item->Definition->GridFootprintW, 1);
	const int32 BaseH = FMath::Max(Item->Definition->GridFootprintH, 1);
	const int32 W = bNewRotated ? BaseH : BaseW;
	const int32 H = bNewRotated ? BaseW : BaseH;

	if (!IsRectFree(NewKind, NewX, NewY, W, H))
	{
		// Roll back exactly.
		Item->ContainerKind = OldKind;
		Item->GridX = OldX; Item->GridY = OldY; Item->bRotated = bOldRot;
		return false;
	}

	Item->ContainerKind = NewKind;
	Item->GridX = NewX; Item->GridY = NewY; Item->bRotated = bNewRotated;
	OnInventoryChanged.Broadcast();
	return true;
}

void UQRInventoryComponent::OnRep_Items()    { OnInventoryChanged.Broadcast(); }
void UQRInventoryComponent::OnRep_HandSlot() { OnInventoryChanged.Broadcast(); }
void UQRInventoryComponent::OnRep_EquippedContainers()
{
	// Server is authoritative on Max* values, but recompute on clients too so
	// local prediction sees the same totals during the rep window.
	RecomputeCapacityFromContainers();
}

// ── Worn-armour slots ──────────────────────────────────────────────

static EQRArmourSlot _SlotFromItemId(const FName& Id)
{
	const FString S = Id.ToString().ToUpper();
	if (!S.StartsWith(TEXT("ARM_"))) return EQRArmourSlot::None;
	if (S.Contains(TEXT("HELM")))  return EQRArmourSlot::Helm;
	if (S.Contains(TEXT("CHEST"))) return EQRArmourSlot::Chest;
	if (S.Contains(TEXT("LEGS")))  return EQRArmourSlot::Legs;
	return EQRArmourSlot::None;
}

TObjectPtr<UQRItemInstance>& UQRInventoryComponent::_ArmourRef(EQRArmourSlot Slot)
{
	switch (Slot)
	{
	case EQRArmourSlot::Helm:  return EquippedHelm;
	case EQRArmourSlot::Chest: return EquippedChestArmour;
	case EQRArmourSlot::Legs:  return EquippedLegsArmour;
	default: return EquippedHelm; // unreachable for callers that pre-check
	}
}

UQRItemInstance* UQRInventoryComponent::GetEquippedArmour(EQRArmourSlot Slot) const
{
	switch (Slot)
	{
	case EQRArmourSlot::Helm:  return EquippedHelm;
	case EQRArmourSlot::Chest: return EquippedChestArmour;
	case EQRArmourSlot::Legs:  return EquippedLegsArmour;
	default: return nullptr;
	}
}

bool UQRInventoryComponent::TryEquipArmour(UQRItemInstance* Item)
{
	if (!Item || !Item->Definition) return false;
	if (Item->Definition->Category != EQRItemCategory::Clothing) return false;

	const EQRArmourSlot Slot = _SlotFromItemId(Item->Definition->ItemId);
	if (Slot == EQRArmourSlot::None) return false;

	// Bounce the current occupant back to the loose body grid (with a fresh
	// placement — its old cells may be taken by now).
	TObjectPtr<UQRItemInstance>& SlotRef = _ArmourRef(Slot);
	if (UQRItemInstance* Prev = SlotRef)
	{
		ReturnInstanceToGrid(Prev);
	}
	Items.Remove(Item);
	Item->ContainerKind = EQRContainerKind::None;
	Item->GridX = -1;
	Item->GridY = -1;
	SlotRef = Item;
	OnInventoryChanged.Broadcast();
	return true;
}

bool UQRInventoryComponent::TryUnequipArmour(EQRArmourSlot Slot, UQRItemInstance*& OutRemoved)
{
	OutRemoved = nullptr;
	if (Slot == EQRArmourSlot::None) return false;
	TObjectPtr<UQRItemInstance>& SlotRef = _ArmourRef(Slot);
	if (!SlotRef) return false;
	OutRemoved = SlotRef;
	ReturnInstanceToGrid(SlotRef);
	SlotRef = nullptr;
	OnInventoryChanged.Broadcast();
	return true;
}

void UQRInventoryComponent::OnRep_EquippedArmour()
{
	OnInventoryChanged.Broadcast();
}
