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

	void RefreshSpoilState();
};
