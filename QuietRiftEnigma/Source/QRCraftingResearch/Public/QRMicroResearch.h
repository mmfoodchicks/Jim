#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "QRTypes.h"
#include "QRMicroResearch.generated.h"

// A micro-research project that multiplically buffs a stat when stacked
UCLASS(BlueprintType, Const)
class QRCRAFTINGRESEARCH_API UQRMicroResearchDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName MicroResearchId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EQRResearchFamily Family = EQRResearchFamily::Materials;

	// Which stat this buffs (e.g. "HarvestYield", "CraftSpeed", "FoodPreservation")
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName AffectedStatName;

	// Factor added per stack (e.g. 0.05 = +5% per stack, multiplicative)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float FactorPerStack = 0.05f;

	// Max times this can be stacked
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxStacks = 5;

	// Research points to complete one stack
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	float PointsPerStack = 50.0f;

	// Cost items for each stack
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> RequiredItemIds;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("QRMicroResearch"), MicroResearchId);
	}
};

// Runtime tracking for one micro-research project
USTRUCT(BlueprintType)
struct QRCRAFTINGRESEARCH_API FQRMicroResearchRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName MicroResearchId;

	UPROPERTY(BlueprintReadOnly)
	int32 CompletedStacks = 0;

	UPROPERTY(BlueprintReadOnly)
	float AccumulatedPoints = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bIsQueued = false;

	// Compute the current multiplier value
	float GetCurrentMultiplier(float FactorPerStack) const
	{
		float M = 1.0f;
		for (int32 i = 0; i < CompletedStacks; ++i)
			M *= (1.0f + FactorPerStack);
		return M;
	}
};
