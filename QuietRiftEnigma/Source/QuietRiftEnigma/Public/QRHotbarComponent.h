#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRHotbarComponent.generated.h"

class UQRItemInstance;
class UQRInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHotbarSlotChanged, int32, SlotIndex, UQRItemInstance*, NewItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHotbarActiveSlotChanged, int32, NewActiveSlot);

/**
 * Minecraft-style hotbar living on AQRCharacter.
 *
 * 9 slots, each pointing into an item already in the owner's inventory
 * (no parallel ownership — clearing a slot doesn't destroy the item, it
 * just stops the hotbar referencing it). SelectSlot(i) routes the slot's
 * item into UQRInventoryComponent::HandSlot, which the held-item mesh on
 * the character watches so the world view updates automatically.
 *
 * Authoring path:
 *   - Creative browser drags an UQRItemDefinition onto a slot widget →
 *     widget calls AssignDefinitionToSlot, which spawns a fresh instance
 *     into the inventory and registers it in the slot.
 *   - Player presses 1–9 → SelectSlot(i) is called.
 *   - Held mesh updates from HandSlot's Definition.WorldMesh.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRHotbarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRHotbarComponent();

	static constexpr int32 NumSlots = 9;

	// ── State ────────────────────────────────
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Slots, Category = "Hotbar")
	TArray<TObjectPtr<UQRItemInstance>> Slots;

	// -1 = nothing active (hands empty). 0..8 = slot index.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ActiveSlot, Category = "Hotbar")
	int32 ActiveSlotIndex = -1;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Hotbar|Events")
	FOnHotbarSlotChanged OnSlotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hotbar|Events")
	FOnHotbarActiveSlotChanged OnActiveSlotChanged;

	// ── API ──────────────────────────────────
	// Bind an existing item instance to a slot. Item must already be in the
	// owner's inventory. Pass nullptr to clear. Authority only.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hotbar")
	void AssignSlot(int32 SlotIndex, UQRItemInstance* Item);

	// Convenience: spawn a fresh instance from a definition into the
	// inventory and bind it to the slot in one call. Used by the creative
	// browser drag-drop path. Authority only.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hotbar")
	bool AssignDefinitionToSlot(int32 SlotIndex, const class UQRItemDefinition* Definition,
		int32 Quantity = 1);

	// Activate a slot. Routes the slot's item into HandSlot; the held mesh
	// on the character will pick up the change via HandSlot replication.
	// Passing the currently-active index again deactivates (hands empty).
	// Safe to call from any role — if not authority, fires Server_SelectSlot.
	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SelectSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SelectNext();

	UFUNCTION(BlueprintCallable, Category = "Hotbar")
	void SelectPrev();

	UFUNCTION(BlueprintPure, Category = "Hotbar")
	UQRItemInstance* GetSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Hotbar")
	UQRItemInstance* GetActiveItem() const;

	// Drop the currently-held item into the world. Fails (returns nullptr)
	// if nothing held. Authority only — call from a Server RPC on the
	// character. Returns the spawned world actor on success.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Hotbar")
	class AQRWorldItem* DropActiveItem(int32 QuantityToDrop = -1);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void Server_SelectSlot(int32 SlotIndex);

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_ActiveSlot();

	void ApplyActiveSlotToHand();

	UQRInventoryComponent* GetOwnerInventory() const;
};
