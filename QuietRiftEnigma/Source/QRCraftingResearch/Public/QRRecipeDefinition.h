#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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
