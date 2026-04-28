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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	float MaxCarryWeightKg = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	float MaxVolumeLiters = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 MaxSlots = 30;

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Items, Category = "Inventory")
	TArray<TObjectPtr<UQRItemInstance>> Items;

	// Hand slot (currently equipped/held item)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HandSlot, Category = "Inventory")
	TObjectPtr<UQRItemInstance> HandSlot = nullptr;

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
