#include "QRDepotComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "Net/UnrealNetwork.h"

UQRDepotComponent::UQRDepotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQRDepotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRDepotComponent, StoredItems);
}

bool UQRDepotComponent::CanAccept(const UQRItemDefinition* ItemDef) const
{
	if (!ItemDef) return false;
	return ItemDef->DepotCategory == AcceptedCategory && GetTotalStoredCount() < MaxCapacity;
}

bool UQRDepotComponent::DepositItem(UQRItemInstance* Item)
{
	if (!Item || !CanAccept(Item->Definition)) return false;

	// Snapshot original quantity so we can restore if deposit fails entirely
	const int32 OriginalQuantity = Item->Quantity;

	// Try to merge into existing stacks
	const int32 StackMax = FMath::Max(Item->Definition->MaxStackSize, 1);
	for (UQRItemInstance* Existing : StoredItems)
	{
		if (!Existing || Existing->Definition != Item->Definition || Existing->Quantity >= StackMax)
			continue;

		int32 Space = StackMax - Existing->Quantity;
		int32 ToAdd = FMath::Min(Space, Item->Quantity);
		Existing->Quantity += ToAdd;
		Item->Quantity -= ToAdd;
		if (Item->Quantity <= 0)
		{
			OnDepotChanged.Broadcast();
			return true;
		}
	}

	if (GetTotalStoredCount() < MaxCapacity)
	{
		StoredItems.Add(Item);
		OnDepotChanged.Broadcast();
		return true;
	}

	// Nothing was stored — restore the caller's quantity to avoid silent partial consumption
	Item->Quantity = OriginalQuantity;
	return false;
}

UQRItemInstance* UQRDepotComponent::WithdrawItem(FName ItemId, int32 Quantity)
{
	if (Quantity <= 0 || CountItem(ItemId) < Quantity) return nullptr;

	// Outer to GetOwner so the item survives if this component is later destroyed
	UQRItemInstance* Result = NewObject<UQRItemInstance>(GetOwner());

	int32 Remaining = Quantity;
	for (int32 i = StoredItems.Num() - 1; i >= 0 && Remaining > 0; --i)
	{
		UQRItemInstance* Inst = StoredItems[i];
		// Guard Definition — may be null on a replicated instance that hasn't fully resolved
		if (!Inst || !Inst->Definition || Inst->Definition->ItemId != ItemId) continue;

		if (!Result->IsValid())
			Result->Initialize(Inst->Definition, 0);

		int32 ToTake = FMath::Min(Inst->Quantity, Remaining);
		Inst->Quantity -= ToTake;
		Result->Quantity += ToTake;
		Remaining -= ToTake;

		if (Inst->Quantity <= 0)
			StoredItems.RemoveAt(i);
	}

	OnDepotChanged.Broadcast();
	return Result;
}

int32 UQRDepotComponent::CountItem(FName ItemId) const
{
	int32 Total = 0;
	for (const UQRItemInstance* Inst : StoredItems)
	{
		if (Inst && Inst->Definition && Inst->Definition->ItemId == ItemId)
			Total += Inst->Quantity;
	}
	return Total;
}

int32 UQRDepotComponent::GetTotalStoredCount() const
{
	int32 Total = 0;
	for (const UQRItemInstance* Inst : StoredItems)
	{
		if (Inst) Total += Inst->Quantity;
	}
	return Total;
}

bool UQRDepotComponent::IsFull() const
{
	return GetTotalStoredCount() >= MaxCapacity;
}

bool UQRDepotComponent::IsWithinPullRadius(const AActor* StationActor) const
{
	if (!StationActor || !GetOwner()) return false;
	float DistSq = FVector::DistSquared(GetOwner()->GetActorLocation(), StationActor->GetActorLocation());
	return DistSq <= FMath::Square(PullRadiusCm);
}
