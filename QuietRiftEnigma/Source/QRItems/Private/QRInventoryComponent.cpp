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
	MaxCarryWeightKg = UQRMath::CarryCapacityKg(STR);
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

void UQRInventoryComponent::OnRep_Items()    { OnInventoryChanged.Broadcast(); }
void UQRInventoryComponent::OnRep_HandSlot() { OnInventoryChanged.Broadcast(); }
