#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRItemDefinition.generated.h"

// Per-item food stats (valid only when Category == Food)
USTRUCT(BlueprintType)
struct QRITEMS_API FQRFoodStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float CaloriesRaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float CaloriesCooked = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float WaterContentML = 0.0f;

	// Probability of food-borne illness on consumption (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float RawRiskChance = 0.0f;

	// Spoil rate percent per in-game hour (0..1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float SpoilRatePerHour = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQREdibilityState DefaultEdibility = EQREdibilityState::Unknown;

	// Tags that describe effects on consumption (e.g. Status.Toxin, Status.Bleeding heal)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer ConsumeEffectTags;
};

// Physical item definition — the canonical data asset for every item in the game
UCLASS(BlueprintType, Const)
class QRITEMS_API UQRItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ── Identity ──────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	EQRItemCategory Category = EQRItemCategory::None;

	// Gameplay tags for filtering and system queries
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGameplayTagContainer ItemTags;

	// ── Physical Properties ───────────────────
	// Mass in kilograms (affects encumbrance)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.01"))
	float MassKg = 0.1f;

	// Volume in liters (affects pack space)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.01"))
	float VolumeLiters = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	// v1.17: bulk items (e.g. Bulk Meat Sack, Fuel Barrel) occupy the hands slot and
	// cannot be placed in the grid inventory — requires HandsSlotState == Occupied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical")
	bool bIsBulkItem = false;

	// ── Durability ────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "0"))
	float MaxDurability = 0.0f;   // 0 = indestructible / consumable

	// ── Food Properties ───────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food",
		meta = (EditCondition = "Category == EQRItemCategory::Food"))
	FQRFoodStats FoodStats;

	// v1.17: where this food originated — governs the "SafeKnown" rule.
	// EarthSealed / ShipRation items are always safe without research.
	// Native items require CodexDiscoveryState == Known before safe consumption.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food",
		meta = (EditCondition = "Category == EQRItemCategory::Food"))
	EQRFoodOriginClass FoodOriginClass = EQRFoodOriginClass::Unknown;

	// Integrity of sealed packaging [0..1]. Only meaningful for EarthSealed / ShipRation.
	// If PackageIntegrity < 0.5 the item loses its SafeKnown exemption.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Food",
		meta = (EditCondition = "Category == EQRItemCategory::Food",
			ClampMin = "0", ClampMax = "1"))
	float PackageIntegrity = 1.0f;

	// ── Visuals / Audio ───────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UTexture2D> InventoryIcon;

	// ── Depot/Logistics ───────────────────────
	// Which depot category this item belongs to for pull logic
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logistics")
	FGameplayTag DepotCategory;

	// ── Reference Component specifics ─────────
	// If this item is a Reference Component, which tech unlock it gates
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression",
		meta = (EditCondition = "Category == EQRItemCategory::ReferenceComponent"))
	FName UnlocksRefComponentId;

	// ── Crafting ─────────────────────────────
	// True if this item can be used as crafting input without being consumed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	bool bIsReusableCraftingInput = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("QRItem"), ItemId);
	}
};
