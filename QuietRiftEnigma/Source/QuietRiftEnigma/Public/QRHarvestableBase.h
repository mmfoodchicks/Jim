#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRHarvestableBase.generated.h"

class UQRItemDefinition;

// A yield entry for a harvestable node
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRHarvestYield
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 MaxQuantity = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1"))
	float DropChance = 1.0f;

	// Which tool tag is required to harvest this output
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag RequiredToolTag;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHarvested, AQRHarvestableBase*, Node, TArray<FQRHarvestYield>, Yields);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNodeDepleted, AQRHarvestableBase*, Node);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNodeRegrown, AQRHarvestableBase*, Node);

// Base class for all harvestable world objects: trees, rocks, plants, fungi
UCLASS(Abstract, BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRHarvestableBase : public AActor
{
	GENERATED_BODY()

public:
	AQRHarvestableBase();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable")
	FName NodeId;          // Unique per-instance GUID set at BeginPlay

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable")
	FName SpeciesId;       // e.g. PLT_SmokebarkTree

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable")
	EQRBiomeType PreferredBiome = EQRBiomeType::Forest;

	// ── Yield ─────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable")
	TArray<FQRHarvestYield> HarvestYields;

	// How many harvest actions this node supports before depletion
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable", meta = (ClampMin = "1"))
	int32 MaxHarvestCharges = 5;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Harvestable")
	int32 RemainingCharges;

	// Game-hours before this node regrows (0 = never)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable")
	float RegrowthTimeHours = 24.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Harvestable")
	float RegrowthTimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Harvestable")
	bool bIsDepleted = false;

	// ── Codex ────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvestable")
	EQRCodexDiscoveryState InitialCodexState = EQRCodexDiscoveryState::Undiscovered;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Harvestable|Events")
	FOnHarvested OnHarvested;

	UPROPERTY(BlueprintAssignable, Category = "Harvestable|Events")
	FOnNodeDepleted OnNodeDepleted;

	UPROPERTY(BlueprintAssignable, Category = "Harvestable|Events")
	FOnNodeRegrown OnNodeRegrown;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Harvestable")
	TArray<FQRHarvestYield> Harvest(FGameplayTag ToolTag, float EfficiencyMultiplier = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Harvestable")
	bool CanBeHarvested() const { return !bIsDepleted && RemainingCharges > 0; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Harvestable")
	void AdvanceRegrowth(float GameHoursElapsed);

	UFUNCTION(BlueprintNativeEvent, Category = "Harvestable")
	void OnHarvest(AActor* Harvester);
	virtual void OnHarvest_Implementation(AActor* Harvester) {}

	UFUNCTION(BlueprintNativeEvent, Category = "Harvestable")
	void OnDepleted();
	virtual void OnDepleted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Harvestable")
	void OnRegrown();
	virtual void OnRegrown_Implementation();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
