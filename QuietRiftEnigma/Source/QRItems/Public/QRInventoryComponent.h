#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRTypes.h"
#include "QRInventoryComponent.generated.h"

class UQRItemDefinition;
class UQRItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded,   UQRItemInstance*, Item, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, UQRItemInstance*, Item, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

// Result of an inventory transaction
UENUM(BlueprintType)
enum class EQRInventoryResult : uint8
{
	Success,
	Full,
	InvalidItem,
	NotEnough,
	TooHeavy,
	WrongSlot,        // tried to equip a non-container or wrong-slot item
	SlotOccupied,     // a container is already equipped in that slot
	WouldNotFit,      // unequipping would leave items with nowhere to go
};

// Spatial inventory that tracks weight, volume, and item location
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRITEMS_API UQRInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRInventoryComponent();

	// ── Config ───────────────────────────────
	// v1.17: CarryCapacityKg = 20 + 6*STR. Default assumes STR 3.
	// Replicated so clients can run accurate IsSprintBlocked checks locally.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory")
	float MaxCarryWeightKg = 38.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory")
	float MaxVolumeLiters = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Inventory")
	int32 MaxSlots = 30;

	// Spatial grid dimensions for UI layout (W columns × H rows)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 InventoryGridW = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 InventoryGridH = 5;

	// Sprint is blocked when CurrentWeightKg / MaxCarryWeightKg >= 0.85
	// (enforced by owning character movement component)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory",
		meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SprintEncumbranceRatio = 0.85f;

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "Inventory")
	TArray<TObjectPtr<UQRItemInstance>> Items;

	// Hand slot (currently equipped/held item — typically a weapon or tool)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HandSlot, Category = "Inventory")
	TObjectPtr<UQRItemInstance> HandSlot = nullptr;

	// v1.17: Hands slot occupancy FSM, tracks bulk-item carry state separately from HandSlot ptr
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HandSlot, Category = "Inventory")
	EQRHandsSlotState HandsSlotState = EQRHandsSlotState::Empty;

	// v1.17: max items that can be shoulder-stacked. = 1 + floor(STR/4), clamped 1..3.
	// Updated by the owning character when STR changes.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	int32 ShoulderStackMax = 1;

	// ── Equipped Containers ──────────────────
	// Currently equipped chest rig (extends grid, carry weight, volume).
	// Null when nothing is worn on the chest. Set via TryEquipContainer.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedContainers, Category = "Inventory|Equipment")
	TObjectPtr<UQRItemInstance> EquippedChestRig = nullptr;

	// Currently equipped backpack (extends grid, carry weight, volume).
	// Null when nothing is worn on the back. Set via TryEquipContainer.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedContainers, Category = "Inventory|Equipment")
	TObjectPtr<UQRItemInstance> EquippedBackpack = nullptr;

	// Player's base STR-derived carry capacity, separate from the container bonus.
	// MaxCarryWeightKg is recomputed as BaseCarryWeightKg + sum of container bonuses
	// every time a container is equipped/unequipped or STR changes.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	float BaseCarryWeightKg = 38.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	float BaseVolumeLiters = 60.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inventory")
	int32 BaseSlots = 30;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnInventoryChanged OnInventoryChanged;

	// ── Transactions ─────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	EQRInventoryResult TryAddItem(UQRItemInstance* Item, int32& OutRemainder);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	EQRInventoryResult TryAddByDefinition(const UQRItemDefinition* Def, int32 Quantity, int32& OutRemainder);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryRemoveItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryEquipToHandSlot(UQRItemInstance* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void ClearHandSlot();

	// Equip a chest rig or backpack. Item must have ContainerSlot != None.
	// On success the slot is filled and capacity is recomputed; the item is
	// removed from the flat Items array if it was sitting there. Returns:
	//   Success      — equipped, capacity expanded
	//   InvalidItem  — null or no Definition
	//   WrongSlot    — Definition.ContainerSlot is None or doesn't match a slot
	//   SlotOccupied — a container is already equipped in that slot
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Equipment")
	EQRInventoryResult TryEquipContainer(UQRItemInstance* Item);

	// Unequip the container in the given slot. Fails with WouldNotFit if the
	// remaining capacity (grid slots, weight, or volume) would no longer hold
	// the items currently carried — the player must drop or stash items first.
	// On success the container becomes a regular item the caller can place
	// somewhere (returned in OutRemovedContainer).
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory|Equipment")
	EQRInventoryResult TryUnequipContainer(EQRContainerSlotType Slot, UQRItemInstance*& OutRemovedContainer);

	// Read-only query — current container in a slot (null if empty).
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	UQRItemInstance* GetEquippedContainer(EQRContainerSlotType Slot) const;

	// Force-clears LockedByAction state (call on death, save-load, or action interruption)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void ForceUnlockHandsSlot();

	// ── Queries ──────────────────────────────
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 CountItem(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(FName ItemId, int32 MinQuantity = 1) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetCurrentWeightKg() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetCurrentVolumeLiters() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsOverEncumbered() const;

	// True when CurrentWeight/MaxCarryWeight >= SprintEncumbranceRatio
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSprintBlocked() const;

	// v1.17: recompute ShoulderStackMax = Clamp(1 + floor(STR/4), 1, 3) when character STR changes
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void SetShoulderStackFromSTR(int32 STR);

	// v1.17: Recalculate MaxCarryWeightKg = 20 + 6*STR when character STR changes
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void SetCarryCapacityFromSTR(int32 STR);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UQRItemInstance*> GetItemsByCategory(EQRItemCategory Category) const;

	// ── Tick (spoil advance) ─────────────────
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	float SpoilAccumulatedHours = 0.0f;

	UQRItemInstance* FindExistingStack(FName ItemId, int32 MaxStack) const;

	UFUNCTION()
	void OnRep_Items();

	UFUNCTION()
	void OnRep_HandSlot();

	UFUNCTION()
	void OnRep_EquippedContainers();

	// Recompute MaxCarryWeightKg / MaxVolumeLiters / MaxSlots from
	// BaseCarryWeightKg + BaseVolumeLiters + BaseSlots plus the bonuses
	// contributed by EquippedChestRig and EquippedBackpack. Called whenever
	// a container is equipped, unequipped, or STR changes.
	void RecomputeCapacityFromContainers();
};
