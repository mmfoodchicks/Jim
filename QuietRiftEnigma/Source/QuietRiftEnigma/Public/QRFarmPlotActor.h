#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRFarmPlotActor.generated.h"

class UStaticMeshComponent;
class UQRWeatherComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCropHarvested, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCropMutated,   FName, MutatedCultivarId);


/**
 * One tile of cultivated soil. Master GDD §14 — Earth crops have to be
 * babysat past the planet's mutagenic pressure, and the cross-
 * contamination equation
 *
 *   CrossContamExposure =
 *     ToxicSoil + InfectedWater + SporeLoad + FarmerCrossContamScore
 *
 * (UQRMath::CrossContamExposure) drives the mutation roll every growth
 * tick. When exposure ≥ CropMutationThreshold the cultivar mutates --
 * its yield item id is replaced with a "MUT_<original>" branch and the
 * plot's base state advances to Mutated. Mutated cultivars yield more
 * but can carry hazards (route through Codex Sampled state to verify).
 *
 * Drop in the world via worldgen / build mode and call Plant() with a
 * seed item id. AQRGameMode advances every plot via GameHoursElapsed.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRFarmPlotActor : public AActor
{
	GENERATED_BODY()

public:
	AQRFarmPlotActor();

	// ── Configuration ───────────────────────────────────────────
	// Game-hours from Plant() to Harvestable. Tuned per seed in BP.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Farm",
		meta = (ClampMin = "1", ClampMax = "240"))
	float GrowthHours = 72.0f;

	// Default yield item id when no seed is supplied (designer fallback).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Farm")
	FName DefaultYieldItemId;

	// Exposure threshold above which the cultivar mutates each growth
	// tick (rolled once when growth completes; pressure read live).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Farm",
		meta = (ClampMin = "1", ClampMax = "20"))
	float CropMutationThreshold = 4.0f;

	// ── Environmental pressure (UQRMath::CrossContamExposure inputs) ─
	// 0 = clean Earth-grade soil; 1+ = mineral-toxic ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Farm|Pressure",
		meta = (ClampMin = "0", ClampMax = "5"))
	float ToxicSoil = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Farm|Pressure",
		meta = (ClampMin = "0", ClampMax = "5"))
	float InfectedWater = 0.0f;

	// Live spore pressure from nearby Mutated plots (auto-updated each
	// growth tick by polling the world).
	UPROPERTY(BlueprintReadOnly, Category = "QR|Farm|Pressure")
	float SporeLoad = 0.0f;

	// Per-farmer hygiene score (the colonist tending this plot tracks
	// CrossContamScore; the plot reads the last care visit). 0 is clean,
	// rises with handling contaminated crops without washing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Farm|Pressure",
		meta = (ClampMin = "0", ClampMax = "5"))
	float FarmerCrossContamScore = 0.0f;

	// Spore-spread radius — Mutated plots inside this push SporeLoad on
	// this plot every tick.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Farm",
		meta = (ClampMin = "100", ClampMax = "10000"))
	float SporePressureRadiusCm = 1500.0f;

	// ── State ───────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "QR|Farm|State")
	FName PlantedSeedId;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Farm|State")
	FName CurrentYieldItemId;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Farm|State")
	float GrowthProgressHours = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Farm|State")
	EQREarthCropBaseState BaseState = EQREarthCropBaseState::Stable;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Farm|State")
	bool bHarvestable = false;

	UPROPERTY(BlueprintAssignable, Category = "QR|Farm|Events")
	FOnCropHarvested OnCropHarvested;

	UPROPERTY(BlueprintAssignable, Category = "QR|Farm|Events")
	FOnCropMutated   OnCropMutated;

	// ── API ─────────────────────────────────────────────────────
	// Start a fresh grow cycle. SeedId becomes both PlantedSeedId and
	// CurrentYieldItemId (mutation may later branch the yield).
	UFUNCTION(BlueprintCallable, Category = "QR|Farm")
	void Plant(FName SeedId);

	UFUNCTION(BlueprintCallable, Category = "QR|Farm")
	void Harvest();

	// Advance the grow cycle by N game-hours. Pulls SporeLoad from
	// nearby Mutated plots, rolls a mutation check at completion.
	UFUNCTION(BlueprintCallable, Category = "QR|Farm")
	void TickGameHours(float DeltaGameHours);

protected:
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PlotMesh;

	// Walk the world for Mutated plots inside SporePressureRadiusCm and
	// sum their contribution into SporeLoad.
	void RecomputeSporeLoad();
};
