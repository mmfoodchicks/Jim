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
};
