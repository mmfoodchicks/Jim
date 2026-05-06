#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "QRTypes.h"
#include "QRItemInstance.generated.h"

class UQRItemDefinition;

// Runtime instance of an item — tracks quantity, durability, spoil, and per-instance state
UCLASS(BlueprintType)
class QRITEMS_API UQRItemInstance : public UObject
{
	GENERATED_BODY()

public:
	// ── Data ─────────────────────────────────
	// Replicated so clients can read item properties without a separate asset lookup;
	// avoids null-deref window between Items array replication and Definition loading.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Item")
	TObjectPtr<const UQRItemDefinition> Definition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Item", ReplicatedUsing = OnRep_Quantity)
	int32 Quantity = 1;

	// Current durability (0 = broken; -1 = N/A for consumables)
	UPROPERTY(BlueprintReadOnly, Category = "Item", ReplicatedUsing = OnRep_Durability)
	float Durability = -1.0f;

	// Spoil progress [0..1], 1 = fully rotten
	UPROPERTY(BlueprintReadOnly, Category = "Item", ReplicatedUsing = OnRep_Spoil)
	float SpoilProgress = 0.0f;

	// Derived from SpoilProgress via OnRep_Spoil — not replicated separately
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	EQRSpoilState SpoilState = EQRSpoilState::Fresh;

	// Per-instance edibility override (research can change this from the default)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Item")
	EQREdibilityState EdibilityState = EQREdibilityState::Unknown;

	// Unique runtime ID for save/load and network tracking
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Item")
	FGuid InstanceGuid;

	// ── Spatial Placement (Tarkov-style) ─────
	// Which logical grid this instance currently occupies. Body / ChestRig /
	// Backpack are valid; None means the instance hasn't been placed yet
	// (newly created or being moved). Updated through UQRInventoryComponent
	// placement APIs — never set this directly.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Placement, Category = "Item|Placement")
	EQRContainerKind ContainerKind = EQRContainerKind::None;

	// Top-left grid cell of this item's footprint within ContainerKind. -1
	// when unplaced. Combined with the definition's GridFootprintW/H (and
	// bRotated) it determines which cells the item occupies.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Placement, Category = "Item|Placement")
	int32 GridX = -1;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Placement, Category = "Item|Placement")
	int32 GridY = -1;

	// True when the item is rotated 90°, swapping its W and H footprint.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Placement, Category = "Item|Placement")
	bool bRotated = false;

	// ── Accessors ────────────────────────────
	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsValid() const { return Definition != nullptr && Quantity > 0; }

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsBroken() const { return Durability >= 0.0f && Durability < KINDA_SMALL_NUMBER; }

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsSpoiled() const { return SpoilState == EQRSpoilState::Rotten || SpoilProgress >= 1.0f; }

	UFUNCTION(BlueprintCallable, Category = "Item")
	void AdvanceSpoilByHours(float GameHoursElapsed);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void ApplyDurabilityDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void Initialize(const UQRItemDefinition* InDefinition, int32 InQuantity = 1);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Quantity();

	UFUNCTION()
	void OnRep_Durability();

	UFUNCTION()
	void OnRep_Spoil();

	UFUNCTION()
	void OnRep_Placement();

	void RefreshSpoilState();
};
