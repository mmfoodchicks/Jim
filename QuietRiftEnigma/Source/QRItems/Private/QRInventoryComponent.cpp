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
}

EQRInventoryResult UQRInventoryComponent::TryAddItem(UQRItemInstance* Item, int32& OutRemainder)
{
	if (!Item || !Item->Definition)
	{
		OutRemainder = Item ? Item->Quantity : 0;
		return EQRInventoryResult::InvalidItem;
	}

	if (GetCurrentWeightKg() + Item->Definition->MassKg * Item->Quantity > MaxCarryWeightKg)
	{
		OutRemainder = Item->Quantity;
		return EQRInventoryResult::TooHeavy;
	}

	int32 Remaining = Item->Quantity;
	const int32 StackMax = Item->Definition->MaxStackSize;

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

	// Open new slots for remainder
	while (Remaining > 0 && Items.Num() < MaxSlots)
	{
		UQRItemInstance* NewInst = NewObject<UQRItemInstance>(this);
		NewInst->Initialize(Item->Definition, FMath::Min(Remaining, StackMax));
		NewInst->SpoilProgress  = Item->SpoilProgress;
		NewInst->EdibilityState = Item->EdibilityState;

		int32 SlotIdx = Items.Add(NewInst);
		Remaining -= NewInst->Quantity;
		OnItemAdded.Broadcast(NewInst, SlotIdx);
	}

	OutRemainder = Remaining;
	OnInventoryChanged.Broadcast();
	return Remaining == 0 ? EQRInventoryResult::Success : EQRInventoryResult::Full;
}

EQRInventoryResult UQRInventoryComponent::TryAddByDefinition(const UQRItemDefinition* Def, int32 Quantity, int32& OutRemainder)
{
	if (!Def)
	{
		OutRemainder = Quantity;
		return EQRInventoryResult::InvalidItem;
	}

	UQRItemInstance* Temp = NewObject<UQRItemInstance>(this);
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
		if (!Inst || Inst->Definition->ItemId != ItemId) continue;

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

	// Bulk items (e.g. Bulk Meat Sack) mark the hands slot as occupied and cannot share
	if (Item->Definition && Item->Definition->bIsBulkItem && HandsSlotState == EQRHandsSlotState::Occupied)
		return false;

	HandSlot = Item;
	HandsSlotState = EQRHandsSlotState::Occupied;
	return true;
}

void UQRInventoryComponent::ClearHandSlot()
{
	HandSlot = nullptr;
	HandsSlotState = EQRHandsSlotState::Empty;
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

void UQRInventoryComponent::OnRep_Items() { OnInventoryChanged.Broadcast(); }
void UQRInventoryComponent::OnRep_HandSlot() {}
