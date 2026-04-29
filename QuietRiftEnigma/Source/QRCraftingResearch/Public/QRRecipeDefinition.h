#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRRecipeDefinition.generated.h"

// One ingredient required by a recipe
USTRUCT(BlueprintType)
struct QRCRAFTINGRESEARCH_API FQRRecipeIngredient
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Quantity = 1;

	// If true, item is not consumed (reusable jig/mold/tool)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bIsReusable = false;
};

// Output entry — recipes can produce multiple outputs
USTRUCT(BlueprintType)
struct QRCRAFTINGRESEARCH_API FQRRecipeOutput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Quantity = 1;

	// If < 1, this output has a yield chance
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float YieldChance = 1.0f;
};

// Data asset defining a single crafting recipe
UCLASS(BlueprintType, Const)
class QRCRAFTINGRESEARCH_API UQRRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName RecipeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FText DisplayName;

	// Which station tag is required to craft this
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FGameplayTag RequiredStation;

	// Tech tier requirement
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	EQRTechTier RequiredTier = EQRTechTier::T0_Primitive;

	// TechNode that must be unlocked before this appears
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName RequiredTechNodeId;

	// Reference Component required (physical unlock, not consumed)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	FName RequiredReferenceComponentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TArray<FQRRecipeIngredient> Ingredients;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	TArray<FQRRecipeOutput> Outputs;

	// Craft time in real-seconds (0 = instant)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "0"))
	float CraftTimeSeconds = 5.0f;

	// If true, requires NPC skill to craft (not player-craftable)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe")
	bool bNPCOnly = false;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("QRRecipe"), RecipeId);
	}
};

// ─────────────────────────────────────────────────────────────────────────────
//  DataTable row version of a recipe — use DT_Recipes.csv with this row type
//  instead of individual data assets when you need spreadsheet-driven authoring.
//
//  Ingredient and output slots are flat columns (Ingredient1 / Ingredient1Qty, etc.)
//  so they round-trip cleanly through CSV without custom serialization.
//  Blank FName slots are ignored; Quantity == 0 slots are also skipped.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct QRCRAFTINGRESEARCH_API FQRRecipeTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag RequiredStation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQRTechTier RequiredTier = EQRTechTier::T0_Primitive;

	// Tech node that must be unlocked before this recipe is available
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RequiredTechNodeId;

	// Physical reference component required (not consumed)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RequiredReferenceComponentId;

	// ── Ingredient slots (up to 6) ────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Ingredient1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Ingredient1Qty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool   Ingredient1Reusable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Ingredient2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Ingredient2Qty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool   Ingredient2Reusable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Ingredient3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Ingredient3Qty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool   Ingredient3Reusable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Ingredient4;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Ingredient4Qty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool   Ingredient4Reusable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Ingredient5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Ingredient5Qty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool   Ingredient5Reusable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Ingredient6;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Ingredient6Qty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool   Ingredient6Reusable = false;

	// ── Output slots ──────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  OutputItemId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  OutputQty = 1;

	// Optional byproduct
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName  Output2ItemId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32  Output2Qty = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float Output2YieldChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	float CraftTimeSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bNPCOnly = false;

	// Helper: expand flat slot columns into the runtime ingredient array
	TArray<FQRRecipeIngredient> GetIngredients() const
	{
		TArray<FQRRecipeIngredient> Out;
		auto Push = [&](FName Id, int32 Qty, bool bReusable)
		{
			if (Id.IsNone() || Qty <= 0) return;
			FQRRecipeIngredient R;
			R.ItemId      = Id;
			R.Quantity    = Qty;
			R.bIsReusable = bReusable;
			Out.Add(R);
		};
		Push(Ingredient1, Ingredient1Qty, Ingredient1Reusable);
		Push(Ingredient2, Ingredient2Qty, Ingredient2Reusable);
		Push(Ingredient3, Ingredient3Qty, Ingredient3Reusable);
		Push(Ingredient4, Ingredient4Qty, Ingredient4Reusable);
		Push(Ingredient5, Ingredient5Qty, Ingredient5Reusable);
		Push(Ingredient6, Ingredient6Qty, Ingredient6Reusable);
		return Out;
	}

	// Helper: expand flat output columns into the runtime output array
	TArray<FQRRecipeOutput> GetOutputs() const
	{
		TArray<FQRRecipeOutput> Out;
		if (!OutputItemId.IsNone() && OutputQty > 0)
		{
			FQRRecipeOutput O;
			O.ItemId      = OutputItemId;
			O.Quantity    = OutputQty;
			O.YieldChance = 1.0f;
			Out.Add(O);
		}
		if (!Output2ItemId.IsNone() && Output2Qty > 0)
		{
			FQRRecipeOutput O2;
			O2.ItemId      = Output2ItemId;
			O2.Quantity    = Output2Qty;
			O2.YieldChance = FMath::Clamp(Output2YieldChance, 0.0f, 1.0f);
			Out.Add(O2);
		}
		return Out;
	}
};
