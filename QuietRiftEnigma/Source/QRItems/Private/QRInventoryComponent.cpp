#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRMath.h"
#include "Net/UnrealNetwork.h"

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
	DOREPLIFETIME(UQRInventoryComponent, HandsSlotState);
	DOREPLIFETIME(UQRInventoryComponent, ShoulderStackMax);
	DOREPLIFETIME(UQRInventoryComponent, MaxCarryWeightKg);
	DOREPLIFETIME(UQRInventoryComponent, MaxVolumeLiters);
	DOREPLIFETIME(UQRInventoryComponent, MaxSlots);
	DOREPLIFETIME(UQRInventoryComponent, EquippedChestRig);
	DOREPLIFETIME(UQRInventoryComponent, EquippedBackpack);
	DOREPLIFETIME(UQRInventoryComponent, BaseCarryWeightKg);
	DOREPLIFETIME(UQRInventoryComponent, BaseVolumeLiters);
	DOREPLIFETIME(UQRInventoryComponent, BaseSlots);
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

	UQRItemInstance* Temp = NewObject<UQRItemInstance>(GetOwner());
	Temp->Initialize(Def, Quantity);
	return TryAddItem(Temp, OutRemainder);
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

	OnInventoryChanged.Broadcast();
	return Remaining == 0;
}

bool UQRInventoryComponent::TryEquipToHandSlot(UQRItemInstance* Item)
{
	if (!Item || !Item->IsValid()) return false;
	if (HandsSlotState == EQRHandsSlotState::LockedByAction) return false;
	if (Item->Definition && Item->Definition->bIsBulkItem && HandsSlotState == EQRHandsSlotState::Occupied)
		return false;

	// Verify the item actually belongs to this inventory (prevents equipping a fake pointer)
	const int32 SlotIdx = Items.IndexOfByKey(Item);
	if (SlotIdx == INDEX_NONE) return false;

	// Move from grid to hand slot so it isn't double-counted
	Items.RemoveAt(SlotIdx);
	HandSlot = Item;
	HandsSlotState = EQRHandsSlotState::Occupied;
	OnInventoryChanged.Broadcast();
	return true;
}

void UQRInventoryComponent::ClearHandSlot()
{
	if (HandSlot && HandSlot->IsValid())
	{
		// Return item to grid; if inventory is now full the item is placed anyway
		// (it was already "in" the inventory before being equipped)
		int32 Remainder = 0;
		TryAddItem(HandSlot, Remainder);
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
	// dropped pickup, world container, or trade flow.
	Items.Remove(Item);

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

	// Capacity *after* removing the container's bonus.
	const float WouldLoseKg     = FMath::Max(Container->Definition->ContainerCarryBonusKg, 0.0f);
	const float WouldLoseLiters = FMath::Max(Container->Definition->ContainerVolumeBonusLiters, 0.0f);
	const int32 WouldLoseSlots  = FMath::Max(Container->Definition->ContainerGridW * Container->Definition->ContainerGridH, 0);

	const float CurrentWeight = GetCurrentWeightKg();
	const float CurrentVolume = GetCurrentVolumeLiters();
	const int32 CurrentSlotsUsed = Items.Num();

	if (CurrentWeight     > MaxCarryWeightKg - WouldLoseKg)     return EQRInventoryResult::WouldNotFit;
	if (CurrentVolume     > MaxVolumeLiters  - WouldLoseLiters) return EQRInventoryResult::WouldNotFit;
	if (CurrentSlotsUsed  > MaxSlots         - WouldLoseSlots)  return EQRInventoryResult::WouldNotFit;

	if (Slot == EQRContainerSlotType::ChestRig) EquippedChestRig = nullptr;
	else                                          EquippedBackpack = nullptr;

	RecomputeCapacityFromContainers();
	OutRemovedContainer = Container;
	OnItemRemoved.Broadcast(Container, 1);
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

	// Accumulate real-time into game-hours (assumes 1 real-second ≈ 1 game-minute at default day speed)
	SpoilAccumulatedHours += DeltaTime / 3600.0f;

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
